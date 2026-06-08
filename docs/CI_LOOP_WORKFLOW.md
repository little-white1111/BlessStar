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

```powershell
python tools/ci/trigger_workflow_dispatch.py --workflow ci.yml --ref <your-branch>
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
powershell -ExecutionPolicy Bypass -File tools/ci/run_ci_loop.ps1 -Ref my-branch -SaveLogs
```

### 3.2 运行逻辑（与你要求的步骤一一对应）

1. **提交 GitHub**：脚本会检查 `git status --porcelain`；若 dirty，会提示你先手工 commit+push（避免脚本擅自提交）。
2. **提交 GitHub action**：调用 `trigger_workflow_dispatch.py` 触发 `workflow_dispatch`
3. **脚本抓取 CI 错误日志**：等待 run 完成后，失败则调用 `fetch_ci_errors.py`
4. **修改项目错误**：脚本停住等待你修复、commit+push
5. 循环直到 CI 全绿或达到 `MaxIters`

---

## 4. Day19 72h 场景（Linux only）

你本机无 Linux 时，建议走 Actions：

- smoke：`day19-stress-smoke.yml`（Windows 900s）
- smoke_fail：`day19-stress-smoke-fail.yml`（Windows 900s · **XIX-MEM-13**）
- full：`day19-stress-full.yml`（Linux 72h · self-hosted `day19-full` 或 fail-fast 说明）

失败后同样可用 `fetch_ci_errors.py` 抓取日志（按分支/按 run）。

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

