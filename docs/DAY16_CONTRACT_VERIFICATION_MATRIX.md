# DAY16: Contract Verification Matrix

本文对应 `IMPL-16-06`，给出“契约条文 -> 验证项”的门禁建议。

| 契约条文 | 验证类型 | 建议入口命令/方式 |
|----------|----------|-------------------|
| C-IX-1 App 承接业务语义 | lint/check | `python tools/purity/check_includes.py`（扩展规则：adapter 禁业务字段） |
| C-IX-2 kernel 仅消费通用 IR | unit/integration | parser->gate->attach 主链集成测（禁止 kernel 直读业务配置） |
| C-IX-3 adapter 非业务决策层 | code review + static check | 目录扫描与关键字规则（业务字段仅允许在 `app/sdk`） |
| C-IX-4 对接顺序与权限边界 | unit | `ctest --test-dir build_ci_test -C Release -L day14 --output-on-failure` |
| C-IX-5 契约需绑定验证项 | doc-check | Contract Registry 中 `verify` 非空检查 |
| C-IX-6 冲突优先级 | doc-check | 契约模板与注册表一致性检查 |
| C-IX-7 契约必须入 Registry | doc-check | 检查 `DAY16_CONTRACT_REGISTRY_TEMPLATE.md` 是否登记 |
| C-IX-8 无验证项不得生效 | CI gate | CI 增加 pre-check：缺 verify 则 fail |

## 结构化契约文件（机器校验）

- `docs/contracts/architecture.contracts.json`
- `docs/contracts/integration.contracts.json`

说明：Markdown 文档继续作为可读视图；`docs/contracts/*.json` 作为机器可校验输入。

## 推荐回归命令集合（第16天）

```powershell
ctest --test-dir build_ci_test -C Release -L day15 --output-on-failure
ctest --test-dir build_ci_test -C Release -L day14 --output-on-failure
ctest --test-dir build_ci_test -C Release -L day12 --output-on-failure
ctest --test-dir build_ci_test -C Release -L integration --output-on-failure
ctest --test-dir build_ci_test -C Release -L regression --output-on-failure
ctest --test-dir build_ci_test -C Release -R bs_test_day16_contract_registry_check --output-on-failure
ctest --test-dir build_ci_test -C Release -R bs_test_day16_contract_files_check --output-on-failure
```
