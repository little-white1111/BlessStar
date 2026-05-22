# 第12天 Attach 韧性矩阵（RES-IX）

| 测例 | RES-IX / IMPL | 断言要点 |
|------|----------------|----------|
| `bs_test_attach_resilience` · scheme unset | RES-IX-9 / IMPL-12-01 | `run` → **-4** |
| per_path commit | RES-IX-11 / IMPL-12-04 | manifest `revision` 0→1 |
| CAS conflict | RES-IX-12a / IMPL-12-05 | 二次 `expected_rev=0` → `BS_ATTACH_ERR_CONFLICT` |
| per_batch | RES-IX-10 / IMPL-12-09 | 两 URI 同批 commit 后均为 rev 1 |
| malloc hook | IMPL-12-10 | `bs_attach_store_open` 注入失败 → nullptr |
| session cap | RES-IX-14 / IMPL-12-06 | `set_session_memory_cap(16)` → `BATCH_COMPLETED_WITH_FAILURES` |
| per_batch gate abort | RES-IX-10 / IMPL-12-09 | 一路 gate 失败 → 两 URI manifest **revision 仍为 0** |
| IO OOM read | RES-IX-6 / IMPL-12-11 | mock `oom` → manifest **不变** |
| audit fields | RES-IX-16 | Report JSON 含 `scheme=`、`batch_epoch=`、`revision_base=`、`abort_code=` |
| manifest path= | RES-IX-8 | `bs_attach_store_get_canonical_path` |
| malloc hook | IMPL-12-10 | `BS_TESTING` 下 `bs_attach_store_open` 失败 |

## Valgrind spot check（T2.6）

本地可选：`ctest -C Release -L day12` 通过后，对 `bs_test_attach_resilience` 做 Valgrind 单次 spot check；第13天全量 ASan 门禁不在本日范围。

## 回归命令

```bash
ctest --test-dir build -C Release -L day12 --output-on-failure
ctest --test-dir build -C Release --output-on-failure
```

## 与 day11 分工

- day11：**SEC-IX** 畸形 JSON / depth / 重复键（`bs_test_config_parse_security`）
- day12：**RES-IX** OOM/IO/冲突/批次原子（本矩阵），不重复 day11 主路径
