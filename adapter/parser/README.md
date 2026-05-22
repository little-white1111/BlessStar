# Adapter parser (BlessStar Config JSON v1)

## MVP main chain (PARSE-IX-1)

Production reload and tests use:

- `bs_config_parse_bytes()` in `include/bs/adapter/parser/config_parse.h`
- `json_lexer` / `json_parser` -> `ConfigV1Ast` -> `ir_generate_config_v1_from_ast`

Canonical bytes may be produced off-chain by `tools/normalize/` (see repo root).

## Legacy skeleton (not the MVP path)

The following remain from early skeleton work and are **not** wired into `reload_gate_default`:

- `Parser.h` / `parser.c` — single `IRInstruction*` API
- `FormatAdapter`, `ASTNode` — generic workflow placeholders

Do not extend these for v1 config; use `config_parse.h` instead.
