# Adapter parser (BlessStar Config JSON v1)

## MVP main chain (PARSE-IX-1)

Production reload and tests use:

- `bs_adapter_parser_parse_bytes()` in `include/bs/adapter/parser/config_parse.h`
- `bs_json_lexer_*` / `bs_json_parse_config_v1` -> `ConfigV1Ast` -> `bs_config_v1_generate_ir_from_ast`

Canonical bytes may be produced off-chain by App SDK or `tools/normalize/` (see repo root).

## Removed legacy skeleton (XVII-DOC-1)

PascalCase `Parser.h` / `FormatAdapter.h` / `ASTNode.h` / `SchemaRegistry.h` / `IRGenerator.h` / `MetaExecutor.h` (formerly `LEGACY-NOT-BUILT`) were **removed** in change **17.22**. MVP config parsing is **only** via `config_parse.h` and the linked `config_*` / `json_*` sources in `bs_adapter_parser`.
