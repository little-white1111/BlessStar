# Git 提交规范

目标：让提交历史可审计、可回滚、可定位到“第X天交付物”。

## 基本要求

- **一次提交解决一件事**：避免把不相关变更混在一起。
- **提交信息可追溯**：尽量在正文注明“第X天”与触发决策摘要（如有）。
- **不提交机密**：禁止提交凭据、token、私钥等。

## 推荐格式

标题行（必填）：

- `day1: scaffold pure kernel skeleton`
- `day1: add purity gate manifest check`
- `docs: add cpp coding standards draft`

正文（可选，建议）：

- 说明“为什么做”和“影响范围”
- 如涉及门禁/CI，注明运行方式

示例：

```
day1: add minimal CMake/Meson + purity gate

- Introduce kernel modules + adapter skeleton
- Add factory manifest verification in CI
```

