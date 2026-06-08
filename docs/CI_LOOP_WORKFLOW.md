# CI 循环流（通用）— push → 触发 Actions → 抓错 → 修复 → 再跑（直到全绿）

> 适用仓库：`little-white1111/BlessStar`
>
> 特点：
> - 不依赖 `gh` CLI（本机无 `gh` 也能跑）
> - 复用仓库已有脚本：`tools/ci/fetch_ci_errors.py`
> - 通过 `workflow_dispatch` 触发 Actions，并在失败时自动拉取失败 job 的错误片段

---

## 1. 前置条件

### 1.1 Token（本机）

`tools/ci/fetch_ci_errors.py` 已实现 token 读取：

- **方式 A（推荐）**：PowerShell 环境变量

```powershell
$env:GITHUB_TOKEN = "ghp_xxx..."
```

- **方式 B（本地文件）**：`tools/ci/.github_token`（gitignored）

> 注意：不要把 token 提交进 git。

### 1.2 触发 workflow 的前提

- Actions 必须允许 `workflow_dispatch`（本仓库已支持）
- 你要跑的 ref（branch）必须已 push 到远端

### 1.3 两个维度：分支 + workflow（推荐）

CI 循环需要同时指定：

| 维度 | 参数 | 含义 |
|------|------|------|
| **测哪条分支的代码** | `-Ref` | 被测 git 分支（须已 push） |
| **跑哪个 workflow** | `-Target` | `ci` \| `day21` \| `day19-smoke` \| …（见 `resolve_ci_target.py list`） |
| **怎么 dispatch** | `-Route` | `via-ci`（默认）或 `direct` |

**`via-ci`（模式 A 友好）**：dispatch **`ci.yml`**，`inputs.branch` + `inputs.suite` 路由到对应 reusable。  
**`direct`（模式 B / 侧栏直跑）**：dispatch **独立 yml**（如 `day21.yml`），`--ref` = 被测分支。

查看全部 target：

```powershell
python tools/ci/resolve_ci_target.py list
```

解析单次 dispatch 计划（调试）：

```powershell
python tools/ci/resolve_ci_target.py --target day21 --ref feat/foo --dispatch-ref main
python tools/ci/resolve_ci_target.py --target day19-smoke --ref day19-stress-smoke --route direct
```

独立 `day21.yml` / `day19-stress-*.yml` **保留** push/schedule；与 `ci.yml` 共用 `*-reusable.yml`。

---

## 2. 手工流程（最朴素）

### 2.1 提交并 push（本机）

```powershell
git status
git add -A
git commit -m "..."
git push -u origin HEAD
```

### 2.2 触发 CI（不依赖 gh）

**模式 A — 固定用 main 的 ci.yml，测任意分支：**

```powershell
# 全量回归（默认 suite=full）
python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref main --inputs '{"branch":"<your-branch>"}'
# day21 轻量门禁
python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref main --inputs '{"branch":"<your-branch>","suite":"day21"}'
python tools/ci/wait_workflow_run.py --branch main --workflow_name "full test" --timeout 3600
```

**模式 B — dispatch ref 即目标分支：**

```powershell
python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref <your-branch>
python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref <your-branch> --inputs '{"suite":"day19-smoke"}'
python tools/ci/wait_workflow_run.py --branch <your-branch> --workflow_name "full test" --timeout 3600
```

### 2.3 等待 CI 结束并拿到 run id

```powershell
python tools/ci/wait_workflow_run.py --branch <your-branch> --workflow_name "full test" --timeout 3600
python tools/ci/wait_workflow_run.py --branch feat/day21-... --workflow_name day21 --timeout 1800
```

成功会打印最后一行 run id（例如 `26497427250`）。

### 2.4 失败时抓错误摘要 +（可选）完整日志

```powershell
python tools/ci/fetch_ci_errors.py --branch <your-branch> --run 26497427250 --save-logs
```

日志会（可选）落在 `ci_logs/` 下。

### 2.5 修复 → 再提交 → 重复 2.2～2.4

---

## 3. 一键循环脚本（推荐）

脚本：`tools/ci/run_ci_loop.ps1`

### 3.1 用法

```powershell
# 在当前分支循环跑 CI，失败就抓错，等你修完再继续
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1
```

指定分支 / 保存完整日志：

```powershell
# 模式 A：main 上 ci.yml + 全量回归
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 -Ref feat/my-branch -DispatchRef main -SaveLogs

# via-ci：main 上 ci.yml，测 feat 分支，跑 day21 套件
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 `
  -Target day21 -Ref feat/day21-foo -DispatchRef main

# via-ci：day19 smoke（超时自动 1800s）
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 `
  -Target day19-smoke -Ref day19-stress-smoke -DispatchRef main

# direct：侧栏 day21 workflow，dispatch ref = 被测分支
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 `
  -Target day21 -Ref feat/day21-foo -Route direct

# 全量回归（默认 -Target ci -Route via-ci）
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 -Ref my-branch -DispatchRef main -SaveLogs
```

### 3.2 开新分支 / 删分支

独立命令（`tools/ci/manage_branch.py`）：

```powershell
# 从 main 开分支并 push
python tools/ci/manage_branch.py create feat/day23-foo --from main

# 查看远端分支
python tools/ci/manage_branch.py list --remote

# 删除本地+远端（main/master 受保护）
python tools/ci/manage_branch.py delete feat/day23-foo --local --remote
```

与 CI 循环一体：

```powershell
# 开分支 → 用 main 的 ci.yml 跑循环 → 全绿后自动删分支
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 `
  -CreateBranch feat/day23-foo -FromBranch main -DispatchRef main -DeleteBranchOnSuccess -SaveLogs

# 仅删分支（不跑 CI）
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 -DeleteBranch feat/day23-foo
```

`-CreateBranch` 会自动：`fetch` → `checkout -b` → `push -u origin`。  
循环开始前会检查远端是否存在该分支；不存在则提示先 `git push`。

### 3.3 运行逻辑（与你要求的步骤一一对应）

1. **提交 GitHub**：脚本会检查 `git status --porcelain`；若 dirty，会提示你先手工 commit+push（避免脚本擅自提交）。
2. **提交 GitHub action**：调用 `trigger_workflow_dispatch.py` 触发 `workflow_dispatch`
3. **脚本抓取 CI 错误日志**：等待 run 完成后，失败则调用 `fetch_ci_errors.py`
4. **修改项目错误**：脚本停住等待你修复、commit+push
5. 循环直到 CI 全绿或达到 `MaxIters`

---

## 4. Day19 / Day21 套件（统一走 ci.yml 或独立 yml）

| suite / 独立 yml | 场景 |
|------------------|------|
| `suite=day21` | KernelPool + TSan 轻量门禁 |
| `suite=day19-smoke` | Windows 900s smoke |
| `suite=day19-smoke-fail` | smoke_fail 对照 · **XIX-MEM-13** |
| `suite=day19-gha-6h` | Ubuntu ~5h50m |
| `suite=day19-full` | 72h · self-hosted `day19-full`（`day19_runner` 默认 `self-hosted`） |

**统一循环**：上表 suite 均可通过 `run_ci_loop.ps1 -Suite <name>` + 模式 A/B 触发 **同一 `ci.yml`**。

**保留独立通道**（schedule / 直 dispatch / feat/day21* push）：

- `day21.yml`、`day19-stress-smoke.yml`、`day19-stress-smoke-fail.yml`、`day19-stress-gha-6h.yml`、`day19-stress-full.yml`

失败后同样用 `fetch_ci_errors.py`（`--branch` = dispatch ref，模式 A 为 `main`）。

---

## 5. Day22 本地回归入口

Day22 采用 R-A gate-first 链路，PR blocking 入口止于 `ci`：

```powershell
python tools/scripts/contracts/contract_gate_runner.py --through-stage ci
ctest --test-dir build_ci_test -C Release -L recover -j 1 --output-on-failure
```

覆盖率汇总是报告型，不阻断 PR：

```powershell
python tools/scripts/test/collect_coverage.py --json-out docs/reports/ctest-label-coverage.json
```

Day19 内存压测按 `C-TST-MEM-1` rule-only 保留 staging/Actions 证据链，禁止新增 blocking `GATE-TEST-DAY19-SMOKE`。

