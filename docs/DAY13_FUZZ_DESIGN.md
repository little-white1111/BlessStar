# Day13 Fuzz Design (P2 · No Implementation)

## Target

- **Entry**：`json_parse_config_v1` / `bs_config_parse_bytes`  
- **Input**：UTF-8 JSON bytes, `len <= BS_JSON_MAX_INPUT_BYTES`

## Out of scope (Day13)

- libFuzzer/AFL CI integration  
- Corpus seed management  
- Windows MSVC fuzz builds

## Suggested Phase-2 shape

1. `LLVMFuzzerTestOneInput` wrapper calling `bs_config_parse_bytes` with zeroed `BsConfigParseResult`.  
2. Dictionary seeds from `tools/normalize/examples/*.json` + day11/day13 negative snippets.  
3. Sanitizer build reused from `ci.yml` `sanitizer` job.

## Invariants to assert under fuzz

- No heap buffer overflow (ASan)  
- No UB on integer paths (UBSan)  
- Failed parse always returns `bs_config_parse_result_destroy`-safe state
