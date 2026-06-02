#!/usr/bin/env python3
"""Replace boilerplate C-ST-7 blocks with module-specific contract text."""

from __future__ import annotations

import sys
from pathlib import Path

_BOILERPLATE = """/*
 * C-ST-7 contract block:
 * Thread safety: See implementation; default not thread-safe unless documented otherwise.
 * Error semantics: See bs_status / module-specific return codes in implementation.
 * Platform notes: N/A unless stated in .c/.cpp implementation.
 */"""

# relative path -> replacement block (without trailing newline beyond closing */
SEMANTIC: dict[str, str] = {
    "kernel/state/include/bs/kernel/state/ConfigManager.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; external lock if shared across threads.
 * Error semantics: 0 success; -1 invalid arg; -2 not found; -3 path already loaded (load).
 * Platform notes: Coordinates StateBus + EventBus drain + WatchManager notify on transitions.
 */""",
    "kernel/state/include/bs/kernel/state/StateBus.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized (shared_mutex); safe for concurrent readers.
 * Error semantics: 0 ok; -1 invalid; -2 missing path; -3 alloc failure on snapshot copy.
 * Platform notes: Owns path-keyed StateEntry map; snapshots are malloc'd copies for callers.
 */""",
    "kernel/state/include/bs/kernel/state/EventBus.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized; listeners invoked from bs_event_bus_drain only.
 * Error semantics: 0 ok; -1 invalid; -2 unsubscribe miss; publish queues until drain.
 * Platform notes: Reentrancy guards wrap listener callbacks (bs_reentrancy_*).
 */""",
    "kernel/state/include/bs/kernel/state/WatchManager.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized; callbacks run on notify thread without reentrancy guard.
 * Error semantics: 0 ok; -1 invalid; -2 remove miss; WATCH_MODE_ONCE auto-removes after fire.
 * Platform notes: Path-keyed watcher lists; pairs with ConfigManager state notifications.
 */""",
    "kernel/state/include/bs/kernel/state/ConfigEvent.h": """/*
 * C-ST-7 contract block:
 * Thread safety: ConfigEvent heap objects are owned by publisher until destroy.
 * Error semantics: bs_config_event_create returns nullptr on alloc failure.
 * Platform notes: Published copies are cloned into EventBus pending queue.
 */""",
    "kernel/state/include/bs/kernel/state/ConfigState.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Pure enum / string helper; reentrant.
 * Error semantics: N/A (no failing API besides unknown state string).
 * Platform notes: Drives ConfigEventType ENTER_* mapping in ConfigManager.
 */""",
    "kernel/state/include/bs/kernel/state/StateMachine.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; one StateMachine per controlling thread.
 * Error semantics: Transition failures return non-zero; illegal transitions rejected.
 * Platform notes: C++ implementation; complements StateBus path-level state.
 */""",
    "kernel/state/include/bs/kernel/state/ShardedStateBus.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Shard-level locking; route by path hash.
 * Error semantics: Same as StateBus per shard; aggregate helpers may return partial data.
 * Platform notes: Wraps multiple StateBus instances for contention reduction.
 */""",
    "kernel/state/include/bs/kernel/state/TemporaryState.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; scoped to attach/reload session.
 * Error semantics: Non-zero on invalid session or exhausted temp bus capacity.
 * Platform notes: Uses nested StateBus for short-lived staging during reload.
 */""",
    "kernel/ir/include/bs/kernel/ir/Metadata.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; one Metadata object per owner thread.
 * Error semantics: Setters return -1 on alloc failure; getters return -1 if key/type mismatch.
 * Platform notes: Linked-list key/value store; binary values are heap copies.
 */""",
    "kernel/ir/include/bs/kernel/ir/ir.h": """/*
 * C-ST-7 contract block:
 * Thread safety: IR lists/instructions are not thread-safe unless externally locked.
 * Error semantics: NULL on alloc failure; destroy functions tolerate NULL.
 * Platform notes: Core IRInstruction graph; pairs with adapter config_v1_ir generator.
 */""",
    "kernel/ir/include/bs/kernel/ir/requirements.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Builtin requirement tables are read-only after init.
 * Error semantics: bs_kernel_get_builtin_requirements never fails (static tables).
 * Platform notes: Used by adapter requirement_filter and config_parse precheck.
 */""",
    "kernel/ir/include/bs/kernel/ir/ir_gate.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Gate evaluation is stateless aside from optional user context pointer.
 * Error semantics: Reject/accept encoded as BsStatus-compatible ints in implementation.
 * Platform notes: Placeholder IR gate hooks for reload orchestration integration.
 */""",
    "kernel/ir/include/bs/kernel/ir/ir_plugin.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Plugin IR hooks must not call back into gate while holding locks.
 * Error semantics: See bs_status in ir_plugin.cpp for plugin-specific codes.
 * Platform notes: Bridges static plugin registration to IR requirement checks.
 */""",
    "kernel/ir/include/bs/kernel/ir/resolver.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Resolver tables are immutable after build.
 * Error semantics: Non-zero when requirement path cannot be resolved.
 * Platform notes: Resolves symbolic requirement names to registry paths.
 */""",
    "kernel/report/include/bs/kernel/report/report.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; one Report per workflow/batch on a single thread.
 * Error semantics: void helpers no-op on NULL report; execute paths set REPORT_STATUS_*.
 * Platform notes: Batch reload audit trail; JSON via bs_report_to_json.
 */""",
    "kernel/report/include/bs/kernel/report/Result.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Result objects are independent heap nodes; not shared across threads.
 * Error semantics: Factory helpers return NULL on OOM; bs_result_code_from_bs_status maps domains.
 * Platform notes: Rich single-point errors complement thin BsStatus on IO paths.
 */""",
    "kernel/pipeline/include/bs/kernel/pipeline/Stage.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Stages are owned by their Pipeline list; not thread-safe.
 * Error semantics: bs_stage_execute returns -1 if execute fn missing; dependency gating via is_ready.
 * Platform notes: StageExecuteFunc transforms IRInstruction to IRInstruction*.
 */""",
    "kernel/runtime/include/bs/kernel/runtime/Kernel.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Kernel instance is not thread-safe across threads.
 * Error semantics: NULL Kernel* on create failure; async execute uses internal queue stub.
 * Platform notes: Optional runtime orchestrator; links report + pipeline registries.
 */""",
    "kernel/runtime/include/bs/kernel/runtime/Context.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Context metadata map is not thread-safe.
 * Error semantics: Metadata setters return -1 on failure; getters return NULL if missing key.
 * Platform notes: Uses bs_metadata_* for per-context key/value storage.
 */""",
    "kernel/runtime/include/bs/kernel/runtime/Config.h": """/*
 * C-ST-7 contract block:
 * Thread safety: KernelConfig is copied by value into Kernel; not shared mutable state.
 * Error semantics: bs_kernel_config_validate may return error_message alloc failures as -1.
 * Platform notes: Holds log level, timeouts, and data directory strings.
 */""",
    "kernel/io/include/bs/kernel/io/io.h": """/*
 * C-ST-7 contract block:
 * Thread safety: IoFacade serializes provider dispatch; providers may define own rules.
 * Error semantics: BsStatus / BS_IO_* via io_status_table; IoReadResult owns error_message heap.
 * Platform notes: Registry-backed provider table; local file provider is MVP default.
 */""",
    "kernel/io/include/bs/kernel/io/io_status_table.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Read-only mapping tables; reentrant.
 * Error semantics: Maps BS_IO_* codes to human strings; unknown codes return generic text.
 * Platform notes: Shared by facade and adapter IO tests.
 */""",
    "kernel/registry/include/bs/kernel/registry/registry_facade.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Facade uses internal hub locking; see registry_facade.cpp.
 * Error semantics: BS_REGISTRY_ERR_* / BsStatus; no exceptions across extern "C" boundary.
 * Platform notes: Primary registry C ABI per ADR-BS-ABI-001.
 */""",
    "kernel/registry/include/bs/kernel/registry/registry_hub.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Hub mutex protects provider tables.
 * Error semantics: Registration collisions return domain-specific error codes.
 * Platform notes: Backing store for facade lookups.
 */""",
    "kernel/registry/include/bs/kernel/registry/path_registry.h": """/*
 * C-ST-7 contract block:
 * Thread safety: PathRegistry not thread-safe unless externally synchronized.
 * Error semantics: Invalid paths rejected at register time with non-zero status.
 * Platform notes: Normalizes paths via path_normalize helpers.
 */""",
    "kernel/registry/include/bs/kernel/registry/path_normalize.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Pure functions on caller buffers; reentrant.
 * Error semantics: Non-zero when normalization would overflow output buffer.
 * Platform notes: Ensures registry keys use canonical slash form.
 */""",
    "kernel/registry/include/bs/kernel/registry/registry_status_table.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Read-only status strings; reentrant.
 * Error semantics: Maps registry error integers to text for logs/reports.
 * Platform notes: Companion to registry_facade error returns.
 */""",
    "kernel/registry/include/bs/kernel/registry/types.h": """/*
 * C-ST-7 contract block:
 * Thread safety: POD descriptors; immutable after registration.
 * Error semantics: N/A (types only).
 * Platform notes: Shared structs for hub/facade/provider entries.
 */""",
    "kernel/common/include/bs/kernel/common/bs_status.h": """/*
 * C-ST-7 contract block:
 * Thread safety: bs_status_make and helpers are reentrant.
 * Error semantics: Packed domain+code BsStatus; BS_STATUS_OK is zero.
 * Platform notes: Thin error model per day7; maps to Result via result_status_map.
 */""",
    "kernel/common/include/bs/kernel/common/bs_log.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Log bus pointer on AttachContext; backends may serialize internally.
 * Error semantics: Logging never throws; drops messages if bus unset.
 * Platform notes: Domains registered via adapter plugin_log_domains.
 */""",
    "kernel/common/include/bs/kernel/common/bs_reentrancy.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Thread-local depth counters for nested callbacks.
 * Error semantics: N/A (markers only).
 * Platform notes: Guards EventBus listener and attach state notifier paths.
 */""",
    "kernel/common/include/bs/kernel/common/bs_safe_format.h": """/*
 * C-ST-7 contract block:
 * Thread safety: bs_safe_snprintf is reentrant when output buffer is caller-owned.
 * Error semantics: Returns required length excluding NUL; truncates safely.
 * Platform notes: Used instead of raw snprintf in C ABI paths.
 */""",
    "kernel/common/include/bs/kernel/common/Plugin.h": """/*
 * C-ST-7 contract block:
 * Thread safety: PluginManager list mutations are not thread-safe.
 * Error semantics: NULL plugin on miss; manager ops return -1 on invalid args.
 * Platform notes: In-process plugin registry for validator/transformer hooks.
 */""",
    "kernel/common/include/bs/kernel/common/Metrics.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Metrics counters use atomics where implemented; see Metrics.cpp.
 * Error semantics: Recording failures are silent drops in MVP.
 * Platform notes: Lightweight counters for benchmarks and diagnostics.
 */""",
    "kernel/common/include/bs/kernel/common/MemoryPool.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Pool not thread-safe unless documented per pool instance.
 * Error semantics: Alloc returns NULL on exhaustion; free tolerates NULL.
 * Platform notes: Optional arena for attach/reload hot paths.
 */""",
    "kernel/common/test_support/include/bs/kernel/test_support/bs_test_log_bus.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Test-only memory log bus; single-threaded tests only.
 * Error semantics: N/A for test harness binding.
 * Platform notes: Excluded from production gates; binds bs_log to capturing buffer.
 */""",
    "adapter/include/bs/adapter/attach_context.h": """/*
 * C-ST-7 contract block:
 * Thread safety: AttachContext is owned by reload/attach driver thread unless locked.
 * Error semantics: BsStatus returns; C++ exceptions do not cross extern "C" exports.
 * Platform notes: Aggregates registry, IO, log, persistence handles for one attach session.
 */""",
    "adapter/include/bs/adapter/attach_runtime.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Runtime bootstrap is single-threaded during attach freeze window.
 * Error semantics: Non-zero when phase registry or manifest validation fails.
 * Platform notes: Wires attach phases P0/P1/P2 per bootstrap manifest.
 */""",
    "adapter/include/bs/adapter/registry_bootstrap.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Bootstrap runs once per attach before freeze.
 * Error semantics: BsStatus; optional post_freeze hook must not block.
 * Platform notes: Registers IO/log/plugin providers into RegistryFacade.
 */""",
    "adapter/include/bs/adapter/requirement_filter.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Filter reads immutable builtin tables; reentrant.
 * Error semantics: Drops IR nodes failing requirement paths; status via caller context.
 * Platform notes: Applies kernel builtin requirements to parsed IR lists.
 */""",
    "adapter/include/bs/adapter/orchestration/reload_batch_controller.h": """/*
 * C-ST-7 contract block:
 * Thread safety: One ReloadBatchController per batch on driver thread.
 * Error semantics: BATCH_* outcomes; per-path failures recorded in Report audit entries.
 * Platform notes: PER_BATCH vs PER_PATH schemes affect batch_had_failure abort semantics.
 */""",
    "adapter/include/bs/adapter/orchestration/reload_batch_factory.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Factory builds isolated controller instances; not shared.
 * Error semantics: NULL controller when allocation or attach_store binding fails.
 * Platform notes: Convenience ctor for tests and adapter_cli.
 */""",
    "adapter/include/bs/adapter/orchestration/reload_gate_default.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Gate runs on batch thread; must not re-enter attach store unsafely.
 * Error semantics: BS_RELOAD_GATE_* codes; parse failures vs IR reject distinguished.
 * Platform notes: Default gate parses bytes via bs_adapter_parser_parse_bytes.
 */""",
    "adapter/include/bs/adapter/orchestration/reload_with_report.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Binds Report for duration of run(); clears after return.
 * Error semantics: Propagates controller outcome; report may be NULL.
 * Platform notes: Thin RAII wrapper around reload_batch_controller_run.
 */""",
    "adapter/include/bs/adapter/persistence/attach_store.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Store serializes batch_begin/stage/commit; not process-wide.
 * Error semantics: BS_ATTACH_* status domain; WAL errors surface as IO failures.
 * Platform notes: Core attach persistence API backing reload commits.
 */""",
    "adapter/include/bs/adapter/persistence/attach_wal.h": """/*
 * C-ST-7 contract block:
 * Thread safety: WAL append is serialized by attach_store caller.
 * Error semantics: Non-zero on checksum or fsync failure.
 * Platform notes: Write-ahead log segments for crash recovery (MVP subset).
 */""",
    "adapter/include/bs/adapter/persistence/attach_audit.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Audit records written from batch thread only.
 * Error semantics: Drops silently if report/store unset; see implementation.
 * Platform notes: Maps reload stages to Report entries for operators.
 */""",
    "adapter/include/bs/adapter/persistence/attach_watch.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Watch registry guarded by internal mutex in attach_watch.c.
 * Error semantics: BS_ATTACH_* on register failure; callbacks on watcher thread.
 * Platform notes: File watch integration for config hot reload (platform-specific).
 */""",
    "adapter/include/bs/adapter/io/local_file_provider.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Provider instance not thread-safe; one facade serializes calls.
 * Error semantics: BS_IO_* via IoReadResult; max read limits enforced.
 * Platform notes: file:// URI reads for local config paths.
 */""",
    "adapter/include/bs/adapter/io/io_providers.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Provider table mutations only during bootstrap.
 * Error semantics: Registration failures return BsStatus to bootstrap caller.
 * Platform notes: Registers local/stub/remote providers with registry hub.
 */""",
    "adapter/include/bs/adapter/io/provider_stubs.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Stubs are stateless aside from configured failure injection.
 * Error semantics: Deterministic BS_IO_ERR_* for tests.
 * Platform notes: Used when remote/DB providers are not linked.
 */""",
    "adapter/include/bs/adapter/log/log_bus.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Depends on backend (memory vs spdlog); see adapter_log implementation.
 * Error semantics: Never throws across C API; drops if domain unknown.
 * Platform notes: Binds kernel bs_log domains to adapter sinks.
 */""",
    "adapter/include/bs/adapter/plugin/plugin_api.h": """/*
 * C-ST-7 contract block:
 * Thread safety: register_fn invoked once per plugin during bootstrap.
 * Error semantics: int status returns per ADR-BS-ABI-001; no exceptions.
 * Platform notes: PLUGIN-VIII static plugin entry signatures.
 */""",
    "adapter/include/bs/adapter/plugin/plugin_loader.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Dynamic load occurs on loader thread during attach only.
 * Error semantics: Non-zero when library missing symbols or manifest mismatch.
 * Platform notes: Wraps platform dlopen for optional plugins.
 */""",
    "adapter/include/bs/adapter/plugin/plugin_ir_requirements.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Requirement merge is read-only on IR lists post-parse.
 * Error semantics: Filters instructions failing plugin-declared requirements.
 * Platform notes: Extends kernel builtin requirements at attach time.
 */""",
    "adapter/include/bs/adapter/plugin/plugin_manifest_paths.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Path resolution is pure on caller buffers.
 * Error semantics: Non-zero when manifest path cannot be composed within cap.
 * Platform notes: YAML manifest discovery for static plugins.
 */""",
    "adapter/include/bs/adapter/plugin/attach_manifest_yaml.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Parse once per manifest load; not concurrent.
 * Error semantics: Non-zero YAML/schema errors; see tests in bs_test_attach_manifest_yaml.
 * Platform notes: Validates attach manifest before freeze.
 */""",
    "app/sdk/include/bs/app/sdk/app_ir_mapper.h": """/*
 * C-ST-7 contract block:
 * Thread safety: Mapper objects are stack-scoped per request; not shared.
 * Error semantics: MapToIr returns false on schema mismatch; strings hold error detail.
 * Platform notes: Intentional C++17 App surface (not C ABI).
 */""",
}


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    changed = 0
    missing = 0
    for rel, block in sorted(SEMANTIC.items()):
        path = root / rel
        if not path.exists():
            print(f"[SKIP] missing {rel}")
            missing += 1
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if _BOILERPLATE not in text:
            print(f"[SKIP] no boilerplate {rel}")
            continue
        path.write_text(text.replace(_BOILERPLATE, block, 1), encoding="utf-8")
        changed += 1
        print(f"[OK] {rel}")
    print(f"[OK] updated {changed} files ({missing} missing)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
