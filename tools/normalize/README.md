# BlessStar Config normalization (`tools/normalize`)

## 目的

- **规范形（canonical）**：BlessStar 主链（M2/M3 起）**只接受** **BlessStar Config JSON v1** 字节流。
- **厂外工具**：本目录下的 Python 脚本为**可选集成**；**不**编入 `bs_adapter_parser`；**不作为** `reload` 默认步骤（见《架构方案选择记录》第 9 天 **PARSE-IX-4**）。

## 契约文件

| 文件 | 说明 |
|------|------|
| `blessstar_config_v1.schema.json` | JSON Schema（draft-07）；字段与第 9 天 **CFG-IX** 一致 |
| `examples/blessstar_config_v1.minimal.json` | 金标准示例（与 `kernel/ir/src/requirements.cpp` builtin `type` 对齐） |
| `examples/identity_normalize.py` | **官方**规范化脚本：校验 + 写出稳定排序的 UTF-8 JSON（**CI 必跑**） |
| `examples/money_normalize.py` | **第10天** 财务 metadata 字符串规则（`amount` / `tax_rate`；**CI 必跑**） |
| `examples/blessstar_config_v1.money_good.json` | money 正例夹具 |
| `examples/blessstar_config_v1.money_bad_tax.json` | money 负例（`13%` 应失败） |

## 导入方式（MVP）

1. 用脚本或手工生成 **规范化后的** JSON 文件（UTF-8，建议无 BOM）。
2. BlessStar 通过 **`file://`** URI 指向该文件，经 **IO read** 进入主链（M3 接入 `reload_gate_default` 后生效）。

## 官方脚本用法

```bash
python tools/normalize/examples/identity_normalize.py \
  tools/normalize/examples/blessstar_config_v1.minimal.json \
  /path/to/canonical.json
```

退出码：`0` 成功；`1` 校验失败；`2` I/O 或参数错误。

### 第10天 · 财务字符串（2-A′）

推荐链：**ERP 私有脚本（可选）→ `money_normalize.py` → `identity_normalize.py` → canonical 文件 → `file://` reload**。

```bash
python tools/normalize/examples/money_normalize.py \
  tools/normalize/examples/blessstar_config_v1.money_good.json \
  /path/to/canonical_money.json
```

规则（MVP）：

- `metadata.amount`：若出现，须为两位小数字符串（如 `100.50`）。
- `metadata.tax_rate`：若出现，须为纯数字字符串（如 `13`，**不可** `13%`）。
- 主链 C parser **不**解析 JSON number 小数；合法性由本脚本与第 11 天安全专题分工（见 **BOUND-IX**）。

### 可选依赖

若已安装 `jsonschema`，`identity_normalize.py` 会按 `blessstar_config_v1.schema.json` 校验；否则使用等价的 **stdlib 结构校验**（CI 不强制安装 `jsonschema`）。

```bash
pip install jsonschema
```

## 用户自定义脚本

- 可按 ERP 环境（用友 / 金蝶等）编写 **私有** normalizer，将厂商格式转为 **BlessStar Config JSON v1**。
- **不**纳入仓库 CI 必过范围；建议放在本机或私有仓库，勿提交含密钥的脚本。
- 推荐目录名：`user/`（可自行加入 `.gitignore`）。

## 与内核清单的关系

- **`kernel_get_builtin_requirements()`** 仍为 **唯一权威根**；JSON **不替代** builtin。
- `instructions[].type` 在运行时必须 ⊆ `bs_adapter_requirement_filter_merge_activation(...)` 的结果；`identity_normalize.py` 对 **M1 金标准** 校验类型 ∈ 当前 builtin 集合（与 `requirements.cpp` 同步维护）。

## 参考

- 《架构方案选择记录》第 9 天：**方案 A′-OPT**、**M1/M2/M3** 落地建议。
- 内核 builtin：`kernel/ir/src/requirements.cpp`
