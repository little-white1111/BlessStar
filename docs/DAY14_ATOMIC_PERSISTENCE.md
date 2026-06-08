# Day 14 — Atomic persistence (ZK-style WAL + Snapshot + pointer)

## Architecture mapping

| ZK concept | BlessStar |
|----------|-----------|
| WAL | `manifest.bs.wal` — batch intent (uri, rev, checksum, staging_path) |
| Snapshot | `canonical` files + `manifest.bs` at `batch_epoch` |
| Pointer | `rename(manifest.bs.tmp → manifest.bs)` + `manifest.bs.prev` |

## per_batch commit order (ATOM-XIV-3)

1. CAS all staged URIs  
2. `bs_adapter_attach_persist_wal_append_batch` + fsync  
3. Write each canonical to **staging path** (≠ old manifest path)  
4. `save_manifest_file` (checksum, `.prev`, fsync, rename)  
5. `bs_adapter_attach_persist_wal_mark_committed`

## Recovery

On `bs_adapter_attach_persist_store_open`: load manifest (fallback `.prev` if checksum bad); `bs_adapter_attach_persist_wal_recover_unfinished` parses **H2 record framing WAL** and removes orphan staging files only when the WAL is **fully decodable and valid**.

### H2 WAL framing (record layout)

- Header (little-endian): `magic(u32) version(u16) type(u16) len(u32) crc32(u32)`
- Payload: `len` bytes
- `crc32` covers `header_without_crc + payload`
- Any framing failure (`magic/version/len cap/crc`) is treated as **corruption** (ATOM-WAL-FRAME-1, ATOM-WAL-DBG-3).

### Recovery safety

- If WAL is corrupted/invalid at any point: **conservative** recovery (ATOM-REC-SAFE-2): **no deletions** of staging/canonical; only best-effort debug evidence.
- Batch-level validation: `BATCH_END` checks `entry_count` and `batch_hash` (ATOM-WAL-INT-2).

### Debug / dump (testing only)

When `BS_TESTING` is enabled, `bs_adapter_attach_persist_wal_dump()` can dump records by `epoch`, `offset`, and `max_records` to support targeted debugging (ATOM-WAL-DBG-1~3).

### P3 · WAL rotate + purge

- **Active segment**: `manifest.bs.wal` (append target).
- **History segment**: `manifest.bs.wal.e{epoch}` after `mark_committed` + fsync (rotate via `rename`).
- **Purge**: on `bs_adapter_attach_persist_store_open`, delete `.wal.e{N}` when `N <= manifest_epoch - K` (default **K=2**) and segment validates `COMMITTED == N`. Corruption anywhere in scanned segments => **no purge** (ATOM-PURGE-9..12).

### Platform fsync/rename contract (ATOM-WIN-7..9)

| Category | Contract |
|----------|----------|
| **Guaranteed** | Single-file write: `tmp` → `fflush` → `bs_adapter_attach_persist_fsync_file(tmp fd)` → `rename(tmp, final)`; manifest/WAL/canonical use the same primitive |
| **Not guaranteed** | Directory fsync; `rename` across volumes; network filesystems |
| **Deployment** | WAL, manifest, and canonical paths must reside on the **same local volume** |

Spot test: `bs_test_attach_fsync_spot` (`-L win_spot`).

## Tests

- `bs_test_attach_atomicity` — `-L day14`
- Regression: `-L day12`, full Release ctest

## Evidence for RES-D-06

Orphan staging after incomplete batch does not change reader-visible `batch_epoch` until manifest flip succeeds.
