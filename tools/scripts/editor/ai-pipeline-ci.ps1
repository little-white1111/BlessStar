#Requires -Version 7.0
<#
.SYNOPSIS
  AI 管线 CI 自动化测试循环
.DESCRIPTION
  流程：
    1. TSC 类型检查
    2. Vitest 运行确定性阶段测试（不依赖 LLM）
    3. 分析失败用例，输出修复建议
    4. 若失败，等待用户修改代码 → 检测文件变化 → 自动重跑
    5. 若全绿，Vite build + asar 打包
    6. 打印手工 E2E 测试清单（需要用户打开 Electron Editor 逐条测试）
.NOTES
  位置：tools/scripts/editor/ai-pipeline-ci.ps1
  用法：.\ai-pipeline-ci.ps1 [-Watch] [-SkipBuild] [-Group <name>]
#>

param(
  [switch]$Watch,
  [switch]$SkipBuild,
  [string]$Group = '',
  [switch]$LiveLLM
)

$ErrorActionPreference = 'Stop'
$EditorRoot = Join-Path $PSScriptRoot '..' '..' '..' 'app' 'editor' | Resolve-Path
$VitestExe = 'node_modules/vitest/vitest.mjs'

Write-Host @'
╔══════════════════════════════════════════════════════╗
║     AI 管线 CI — 自动化测试与修复循环                ║
║     确定性阶段 → TSC → Vitest → 修复 → 重跑          ║
╚══════════════════════════════════════════════════════╝
'@ -ForegroundColor Cyan

# ═══════════════════════════════════════════════════════════════════════
# Step 1: TSC 类型检查
# ═══════════════════════════════════════════════════════════════════════

function Invoke-TypeCheck {
  Write-Host '[1/4] TSC 类型检查...' -ForegroundColor Yellow
  Push-Location $EditorRoot
  try {
    $output = npx tsc --noEmit 2>&1
    $newErrors = $output | Select-String -Pattern 'error TS' | Where-Object {
      $_ -notmatch 'agentFactoryIntegration|CheckboxControl|GroupControl|vitest-canvas'
    }
    if ($newErrors) {
      Write-Host '  ✗ 发现新增类型错误:' -ForegroundColor Red
      $newErrors | ForEach-Object { Write-Host "    $($_.Line.Trim())" -ForegroundColor Red }
      return $false
    }
    Write-Host '  ✓ TSC 零新增错误' -ForegroundColor Green
    return $true
  } finally {
    Pop-Location
  }
}

# ═══════════════════════════════════════════════════════════════════════
# Step 2: Vitest 运行管线阶段测试
# ═══════════════════════════════════════════════════════════════════════

function Invoke-Vitest {
  param([string]$TestPattern = 'pipeline-stages')
  Write-Host '[2/4] Vitest 确定性阶段测试...' -ForegroundColor Yellow
  Push-Location $EditorRoot
  try {
    $testFile = "src/ai/pipeline/__tests__/${TestPattern}.test.ts"
    $result = node $VitestExe run $testFile --reporter=verbose 2>&1
    $exitCode = $LASTEXITCODE

    # Parse results
    $passed = ($result | Select-String -Pattern '✓' | Measure-Object).Count
    $failed = ($result | Select-String -Pattern '×' | Measure-Object).Count

    Write-Host "  Tests: $passed passed, $failed failed" -ForegroundColor $(if ($failed -eq 0) { 'Green' } else { 'Red' })

    if ($failed -gt 0) {
      Write-Host '  --- 失败详情 ---' -ForegroundColor Red
      $result | Select-String -Pattern '×|FAIL|AssertionError|Expected' -Context 0,2 | ForEach-Object {
        Write-Host "  $($_.Line.Trim())" -ForegroundColor Red
      }
    }

    return @{ Passed = $passed; Failed = $failed; ExitCode = $exitCode; Output = $result }
  } finally {
    Pop-Location
  }
}

# ═══════════════════════════════════════════════════════════════════════
# Step 3: Live LLM 回归测试（可选，需 --LiveLLM）
# ═══════════════════════════════════════════════════════════════════════

function Invoke-LiveLLMRegression {
  param([string]$TestGroup = '')
  Write-Host '[3/5] Live LLM 回归测试 (DeepSeek API)...' -ForegroundColor Yellow
  $ScriptDir = Split-Path $PSCommandPath -Parent
  $RegScript = Join-Path $ScriptDir 'ai-llm-regression.mjs'

  if (-not (Test-Path $RegScript)) {
    Write-Host '  ✗ ai-llm-regression.mjs 未找到' -ForegroundColor Red
    return $false
  }

  Push-Location $EditorRoot
  try {
    $args = @($RegScript)
    if ($TestGroup) { $args += '--group', $TestGroup }
    $result = node $args 2>&1
    $exitCode = $LASTEXITCODE

    # 展示输出
    $result | ForEach-Object { Write-Host "  $_" }

    if ($exitCode -ne 0) {
      Write-Host '  ✗ Live LLM 回归测试失败' -ForegroundColor Red
      return @{ Passed = $false; Output = $result }
    }

    Write-Host '  ✓ Live LLM 回归全绿' -ForegroundColor Green
    return @{ Passed = $true; Output = $result }
  } finally {
    Pop-Location
  }
}

# ═══════════════════════════════════════════════════════════════════════
# Step 4: 分析结果 + 输出修复建议
# ═══════════════════════════════════════════════════════════════════════

function Write-FixSuggestions {
  param([hashtable]$VitestResult)

  if ($VitestResult.Failed -eq 0) { return }

  Write-Host '[3/4] 修复建议:' -ForegroundColor Magenta
  Write-Host @'

  常见失败原因及对应文件:

  ┌────────────────────────────────────────────────────────────────┐
  │ 失败模式                    │ 修复位置                         │
  ├────────────────────────────────────────────────────────────────┤
  │ LABEL_TO_KEY 映射失败       │ src/ai/tools/configLabels.ts     │
  │ KEY_LABELS 子串匹配失败     │ src/ai/pipeline/pipelineManager.ts│
  │ mapTripletsToToolCalls 问题 │ src/ai/pipeline/stage-execute.ts  │
  │ Stage Router 路由错误       │ src/ai/pipeline/stage-router.ts   │
  │ Stage Intent 意图压缩错误   │ src/ai/pipeline/stage-intent.ts   │
  │ 测试用例预期不符合新行为    │ src/ai/pipeline/__tests__/pipeline-test-cases.ts │
  └────────────────────────────────────────────────────────────────┘

  修复后保存文件，CI 将自动重新运行。
'@
}

# ═══════════════════════════════════════════════════════════════════════
# Step 4: Vite build + asar（可选）
# ═══════════════════════════════════════════════════════════════════════

function Invoke-Build {
  Write-Host '[4/4] Vite build + asar 打包...' -ForegroundColor Yellow
  Push-Location $EditorRoot
  try {
    $buildOut = npx vite build 2>&1
    if ($LASTEXITCODE -ne 0) {
      Write-Host '  ✗ Vite build 失败' -ForegroundColor Red
      return $false
    }
    Write-Host '  ✓ Vite build 通过' -ForegroundColor Green

    $asarOut = .\update-asar.ps1 2>&1
    $asarSize = ($asarOut | Select-String -Pattern 'asar 大小').ToString().Trim()
    Write-Host "  ✓ $asarSize" -ForegroundColor Green
    return $true
  } finally {
    Pop-Location
  }
}

# ═══════════════════════════════════════════════════════════════════════
# 手工 E2E 测试清单
# ═══════════════════════════════════════════════════════════════════════

function Write-E2EChecklist {
  Write-Host @'

╔════════════════════════════════════════════════════════════════════╗
║  📋 手工 E2E 测试清单（需启动 Electron Editor 逐条执行）           ║
╚════════════════════════════════════════════════════════════════════╝

  【CHAT 咨询类】
  □ gate是什么          → 应有回复含"门禁"定义 + "如何创建"操作指引
  □ schema是什么        → 应有回复含 Schema 定义
  □ 如何创建gate         → 应有操作指引（"直接说..."）
  □ 有哪些功能           → 应有系统功能总览

  【LIST 列表类】
  □ 当前有哪些配置       → [LIST] 所有配置项 ✅ + 沙箱展开配置详情

  【WRITE 写入类】
  □ 帮我把房间号改成10041 → [WRITE] ✅ Registry 记录 + 已成功设置

  【MIXED 混合意图】（核心回归）
  □ 当前有哪些配置，帮我把房间号改成10041
    → [LIST] ✅ + [WRITE] ✅ 两个都有 Registry 记录
  □ 当前有哪些配置，帮我把房间号改成10041，gate是什么
    → [LIST] ✅ + [WRITE] ✅ + chat 回复 ✅ 全部正常

  【GATE 规则类】
  □ 校验配置             → [VALIDATE] ✅
  □ 给房间号加个规则，不能为负数 → [SET_RULE] → create_gate_chain ✅

  【EDGE 边界类】
  □ /room                → 应有房间号显示
  □ (空输入)             → 不崩溃，不报错
'@ -ForegroundColor Cyan
}

# ═══════════════════════════════════════════════════════════════════════
# 文件变化监控（Watch 模式复用）
# ═══════════════════════════════════════════════════════════════════════

function Wait-FileChange {
  param([DateTime]$LastCheck, [string]$EditorRoot)
  Write-Host '  正在监控文件变化，保存后自动重跑... 按 Ctrl+C 退出' -ForegroundColor DarkGray

  $watcher = New-Object System.IO.FileSystemWatcher
  $watcher.Path = Join-Path $EditorRoot 'src' 'ai'
  $watcher.IncludeSubdirectories = $true
  $watcher.Filter = '*.ts'
  $watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite
  $watcher.EnableRaisingEvents = $true

  $global:bsAiChanged = $false
  $handler = Register-ObjectEvent $watcher 'Changed' -Action { $global:bsAiChanged = $true } | Out-Null

  while (-not $global:bsAiChanged) {
    Start-Sleep -Seconds 1
    if (((Get-Date) - $LastCheck).TotalMinutes -gt 5) {
      Write-Host '  ⏰ 5 分钟无变化，退出监控' -ForegroundColor Yellow
      $watcher.EnableRaisingEvents = $false
      $watcher.Dispose()
      return $LastCheck
    }
  }

  $watcher.EnableRaisingEvents = $false
  $watcher.Dispose()
  Remove-Variable -Name bsAiChanged -Scope Global -ErrorAction SilentlyContinue
  Write-Host '  ↻ 检测到文件变化，重新运行...' -ForegroundColor Cyan
  Start-Sleep -Seconds 2  # debounce
  return Get-Date
}

# ═══════════════════════════════════════════════════════════════════════
# 主循环
# ═══════════════════════════════════════════════════════════════════════

function Invoke-Cycle {
  $round = 0
  $lastCheck = Get-Date

  do {
    $round++
    Write-Host "`n━━━ 第 $round 轮 ━━━ $(Get-Date -Format 'HH:mm:ss') ━━━" -ForegroundColor White

    $tscOk = Invoke-TypeCheck
    if (-not $tscOk) {
      Write-Host '  ⏳ TSC 失败，等待修复...' -ForegroundColor Yellow
      if (-not $Watch) { return @{ Passed = 0; Failed = 1 } }
      Start-Sleep -Seconds 3
      continue
    }

    $vitestResult = Invoke-Vitest -TestPattern 'pipeline-stages'
    Write-FixSuggestions -VitestResult $vitestResult

    if ($vitestResult.Failed -gt 0) {
      Write-Host "  ⏳ ${round} 轮 vitest 失败：$($vitestResult.Failed) 条未通过" -ForegroundColor Yellow
      if (-not $Watch) { return $vitestResult }
      $lastCheck = Wait-FileChange -LastCheck $lastCheck -EditorRoot $EditorRoot
      continue
    }

    # Live LLM 回归（可选）
    if ($LiveLLM) {
      $llmResult = Invoke-LiveLLMRegression -TestGroup $Group
      if (-not $llmResult.Passed) {
        Write-Host "  ⏳ ${round} 轮 LLM 回归失败" -ForegroundColor Yellow
        if (-not $Watch) { return @{ Passed = $vitestResult.Passed; Failed = 1 } }
        $lastCheck = Wait-FileChange -LastCheck $lastCheck -EditorRoot $EditorRoot
        continue
      }
    }

    # 全部通过
    Write-Host "`n  ✓ 全部 $($vitestResult.Passed) 条确定性测试通过！" -ForegroundColor Green

    if (-not $SkipBuild) {
      $buildOk = Invoke-Build
      if (-not $buildOk) { return @{ Passed = $vitestResult.Passed; Failed = 1 } }
    }

    Write-E2EChecklist
    Write-Host "`n  ✅ CI 门禁通过。请按上方清单执行手工 E2E 测试。" -ForegroundColor Green
    return @{ Passed = $vitestResult.Passed; Failed = 0 }
  } while ($Watch)
}

# ═══════════════════════════════════════════════════════════════════════
# 入口
# ═══════════════════════════════════════════════════════════════════════

try {
  if ($Watch) {
    Write-Host '  Watch 模式：文件变化时自动重跑' -ForegroundColor DarkGray
  }

  $final = Invoke-Cycle
  if ($final.Failed -gt 0) {
    exit 1
  }
  exit 0
} catch {
  Write-Host "`n  ✗ 脚本异常: $($_.Exception.Message)" -ForegroundColor Red
  exit 2
}
