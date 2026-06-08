# Day 22 Crash Recovery A-Prime

## Purpose

This document records the Phase 1 crash recovery path adopted on Day 22. The API is explicitly two-step so callers can separate durable store recovery from runtime cold reload.

## Two-Step API

Step 1 opens the persisted attach store and runs existing WAL cleanup/recovery through `bs_adapter_attach_persist_store_open`. It then creates an `AttachContext` and marks the session `RECOVERING`.

```c
AttachContext* bs_adapter_attach_recover_from_store(
    const char* manifest_path,
    const BsAttachRecoverFromStoreOptions* opts);
```

While `RECOVERING` is set, read APIs return `BS_ATTACH_ERR_RECOVERING`.

Step 2 enumerates manifest URIs and runs a cold reload for each URI:

```c
int bs_adapter_attach_recover_cold_reload(
    AttachContext* ctx,
    const BsAttachRecoverColdReloadOptions* opts);
```

The cold path is single-threaded from the recover caller perspective. It rebuilds and warms the `KernelPool` before reload, then executes `IoFacade.read -> reload_gate -> sync_path` through the existing reload controller.

## Success And Failure

On full success, Step 2 clears `RECOVERING`; the session is READY and normal read APIs resume.

On any read, gate, persist, or ConfigManager sync failure, Step 2 returns an error and keeps `RECOVERING` set. Operators should inspect the attached `Report` when supplied and retry after the source file or manifest issue is corrected.

## Deliberately Out Of Scope

- No MVP `auto_reload` shortcut that merges Step 1 and Step 2.
- No sidecar checkpoint or fast hydrate in Phase 1/2.
- Recover implementation does not include kernel `ConfigManager.h`; ConfigManager sync remains behind adapter APIs.

## Phase 2 Layer-B FSM (per_batch)

When `attach_scheme == per_batch`, the reload controller writes WAL `PHASE_MARK` records at:

- `STAGE` (batch begin)
- `GATE` (after staging)
- `EXEC` (after pool IR exec)
- `PERSIST` (before batch commit)
- `COMMIT` (after successful batch commit)

If the process dies after `EXEC` but before persist/commit, WAL recovery marks **exec rollback** for that batch epoch. The manifest epoch stays unchanged; staged IR snapshots are cleared; ConfigManager is not authoritatively synced to a new revision.

`per_path` reload does **not** write PHASE_MARK records (**REC-A'-6**).

After Step 2 cold reload succeeds, the adapter runs a CM vs manifest revision reconcile (**REC-A'-9**).

## Phase 3 Layer-C Sidecar (runtime.ckpt)

Optional fast path for **clean restart** only. **Disabled by default.**

Enable at process start:

```text
BS_ATTACH_RECOVER_SIDECAR=1
```

Sidecar file: `{manifest_path}.runtime.ckpt` with manifest digest + per-URI canonical payload digests (**REC-A'-12**). Written only after successful Step-2 READY (**REC-A'-11** post-flip). Any reload batch invalidates the sidecar until the next READY write.

Step-2 tries **fast hydrate** (ConfigManager sync from on-disk canonical files, no IoFacade read) when:

- feature flag is on
- sidecar validates against live manifest
- WAL did **not** report EXEC rollback (**crash never trusts ckpt alone**)

On validation failure, Step-2 **degrades to cold reload** (Layer A) unchanged.

## Test Entry

```powershell
ctest --test-dir build_ci_test -C Release -L recover -j 1 --output-on-failure
```

Covers:

- `bs_test_attach_recover_cold` — two-step cold recover (**Phase 1**)
- `bs_test_attach_recover_fsm` — EXEC rollback + per_path PHASE guard (**Phase 2**)
- `bs_test_attach_recover_sidecar` — fast hydrate + digest fallback + flag off (**Phase 3**)
