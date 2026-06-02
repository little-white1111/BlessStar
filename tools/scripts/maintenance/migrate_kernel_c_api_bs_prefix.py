#!/usr/bin/env python3
"""One-shot migration: kernel public C API -> bs_ prefix (C-ST-1 scope expansion)."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_LIB = Path(__file__).resolve().parents[1] / "lib"
if str(_LIB) not in sys.path:
    sys.path.insert(0, str(_LIB))

from repo_paths import repo_root

# Longest-first explicit PascalCase -> snake bs_ mappings (state module).
EXPLICIT: list[tuple[str, str]] = [
    ("ConfigManager_SubscribeStateChange", "bs_config_manager_subscribe_state_change"),
    ("ConfigManager_UnsubscribeStateChange", "bs_config_manager_unsubscribe_state_change"),
    ("ConfigManager_GetConfigSnapshot", "bs_config_manager_get_config_snapshot"),
    ("ConfigManager_GetConfigState", "bs_config_manager_get_config_state"),
    ("ConfigManager_GetWatchManager", "bs_config_manager_get_watch_manager"),
    ("ConfigManager_ReloadConfig", "bs_config_manager_reload_config"),
    ("ConfigManager_GetStateBus", "bs_config_manager_get_state_bus"),
    ("ConfigManager_GetEventBus", "bs_config_manager_get_event_bus"),
    ("ConfigManager_LoadConfig", "bs_config_manager_load_config"),
    ("ConfigManager_UnloadConfig", "bs_config_manager_unload_config"),
    ("ConfigManager_HotUpdate", "bs_config_manager_hot_update"),
    ("ConfigManager_Destroy", "bs_config_manager_destroy"),
    ("ConfigManager_Create", "bs_config_manager_create"),
    ("ShardedStateBus_GetTotalOperations", "bs_sharded_state_bus_get_total_operations"),
    ("ShardedStateBus_GetAllEntries", "bs_sharded_state_bus_get_all_entries"),
    ("TemporaryState_GetCurrentState", "bs_temporary_state_get_current_state"),
    ("StateMachine_GetCurrentState", "bs_state_machine_get_current_state"),
    ("ShardedStateBus_GetShardCount", "bs_sharded_state_bus_get_shard_count"),
    ("ConfigEventType_ToString", "bs_config_event_type_to_string"),
    ("ShardedStateBus_GetSnapshot", "bs_sharded_state_bus_get_snapshot"),
    ("StateMachine_GetVersion", "bs_state_machine_get_version"),
    ("StateMachine_CanTransition", "bs_state_machine_can_transition"),
    ("StateMachine_SetCallback", "bs_state_machine_set_callback"),
    ("TemporaryState_Destroy", "bs_temporary_state_destroy"),
    ("WatchManager_RemoveWatch", "bs_watch_manager_remove_watch"),
    ("ShardedStateBus_SetState", "bs_sharded_state_bus_set_state"),
    ("ShardedStateBus_GetState", "bs_sharded_state_bus_get_state"),
    ("ShardedStateBus_Destroy", "bs_sharded_state_bus_destroy"),
    ("ShardedStateBus_Create", "bs_sharded_state_bus_create"),
    ("TemporaryState_Activate", "bs_temporary_state_activate"),
    ("TemporaryState_Validate", "bs_temporary_state_validate"),
    ("TemporaryState_Rollback", "bs_temporary_state_rollback"),
    ("TemporaryState_Create", "bs_temporary_state_create"),
    ("TemporaryState_Commit", "bs_temporary_state_commit"),
    ("TemporaryState_IsActive", "bs_temporary_state_is_active"),
    ("StateMachine_Transition", "bs_state_machine_transition"),
    ("StateMachine_Destroy", "bs_state_machine_destroy"),
    ("StateMachine_Create", "bs_state_machine_create"),
    ("WatchManager_AddWatch", "bs_watch_manager_add_watch"),
    ("WatchManager_Destroy", "bs_watch_manager_destroy"),
    ("WatchManager_Create", "bs_watch_manager_create"),
    ("WatchManager_Notify", "bs_watch_manager_notify"),
    ("ConfigState_ToString", "bs_config_state_to_string"),
    ("ConfigEvent_Destroy", "bs_config_event_destroy"),
    ("ConfigEvent_Create", "bs_config_event_create"),
    ("StateBus_GetAllEntries", "bs_state_bus_get_all_entries"),
    ("StateBus_GetSnapshot", "bs_state_bus_get_snapshot"),
    ("StateBus_FreeEntries", "bs_state_bus_free_entries"),
    ("StateBus_Destroy", "bs_state_bus_destroy"),
    ("StateBus_SetState", "bs_state_bus_set_state"),
    ("StateBus_GetState", "bs_state_bus_get_state"),
    ("StateBus_Create", "bs_state_bus_create"),
    ("EventBus_Unsubscribe", "bs_event_bus_unsubscribe"),
    ("EventBus_Subscribe", "bs_event_bus_subscribe"),
    ("EventBus_Destroy", "bs_event_bus_destroy"),
    ("EventBus_Publish", "bs_event_bus_publish"),
    ("EventBus_Create", "bs_event_bus_create"),
    ("EventBus_Drain", "bs_event_bus_drain"),
    ("ShardedStateBus_FreeEntries", "bs_sharded_state_bus_free_entries"),
    ("plugin_manager_get_count_by_type", "bs_plugin_manager_get_count_by_type"),
    ("plugin_manager_get_count", "bs_plugin_manager_get_count"),
    ("plugin_state_to_string", "bs_plugin_state_to_string"),
    ("plugin_type_to_string", "bs_plugin_type_to_string"),
    ("plugin_manager_start_all", "bs_plugin_manager_start_all"),
    ("plugin_manager_stop_all", "bs_plugin_manager_stop_all"),
    ("plugin_manager_get_by_type", "bs_plugin_manager_get_by_type"),
    ("plugin_manager_destroy", "bs_plugin_manager_destroy"),
    ("plugin_manager_reload", "bs_plugin_manager_reload"),
    ("plugin_manager_create", "bs_plugin_manager_create"),
    ("plugin_manager_unload", "bs_plugin_manager_unload"),
    ("plugin_manager_load", "bs_plugin_manager_load"),
    ("plugin_manager_get", "bs_plugin_manager_get"),
    ("plugin_get_state", "bs_plugin_get_state"),
    ("kernel_get_builtin_requirements", "bs_kernel_get_builtin_requirements"),
    ("ir_instruction_list_destroy", "bs_ir_instruction_list_destroy"),
    ("ir_instruction_list_create", "bs_ir_instruction_list_create"),
    ("ir_instruction_list_size", "bs_ir_instruction_list_size"),
    ("ir_instruction_list_add", "bs_ir_instruction_list_add"),
    ("ir_instruction_list_get", "bs_ir_instruction_list_get"),
    ("ir_instruction_add_metadata", "bs_ir_instruction_add_metadata"),
    ("ir_instruction_get_metadata", "bs_ir_instruction_get_metadata"),
    ("ir_instruction_destroy", "bs_ir_instruction_destroy"),
    ("ir_instruction_create", "bs_ir_instruction_create"),
    ("ir_metadata_destroy", "bs_ir_metadata_destroy"),
    ("ir_metadata_create", "bs_ir_metadata_create"),
    ("kernel_config_set_max_pipeline_stages", "bs_kernel_config_set_max_pipeline_stages"),
    ("kernel_config_get_max_pipeline_stages", "bs_kernel_config_get_max_pipeline_stages"),
    ("kernel_config_set_execution_timeout_ms", "bs_kernel_config_set_execution_timeout_ms"),
    ("kernel_config_get_execution_timeout_ms", "bs_kernel_config_get_execution_timeout_ms"),
    ("kernel_config_set_error_handling_mode", "bs_kernel_config_set_error_handling_mode"),
    ("kernel_config_get_error_handling_mode", "bs_kernel_config_get_error_handling_mode"),
    ("kernel_config_set_log_level", "bs_kernel_config_set_log_level"),
    ("kernel_config_get_log_level", "bs_kernel_config_get_log_level"),
    ("kernel_config_set_data_dir", "bs_kernel_config_set_data_dir"),
    ("kernel_config_get_data_dir", "bs_kernel_config_get_data_dir"),
    ("kernel_config_set_name", "bs_kernel_config_set_name"),
    ("kernel_config_get_name", "bs_kernel_config_get_name"),
    ("kernel_config_destroy", "bs_kernel_config_destroy"),
    ("kernel_config_validate", "bs_kernel_config_validate"),
    ("kernel_config_create", "bs_kernel_config_create"),
    ("kernel_execute_async", "bs_kernel_execute_async"),
    ("kernel_unregister_pipeline", "bs_kernel_unregister_pipeline"),
    ("kernel_register_pipeline", "bs_kernel_register_pipeline"),
    ("kernel_get_execution_count", "bs_kernel_get_execution_count"),
    ("kernel_get_start_time", "bs_kernel_get_start_time"),
    ("kernel_get_pipeline", "bs_kernel_get_pipeline"),
    ("kernel_get_version", "bs_kernel_get_version"),
    ("kernel_get_config", "bs_kernel_get_config"),
    ("kernel_set_config", "bs_kernel_set_config"),
    ("kernel_get_state", "bs_kernel_get_state"),
    ("kernel_destroy", "bs_kernel_destroy"),
    ("kernel_execute", "bs_kernel_execute"),
    ("kernel_create", "bs_kernel_create"),
    ("kernel_start", "bs_kernel_start"),
    ("kernel_stop", "bs_kernel_stop"),
    ("result_error_with_detail", "bs_result_error_with_detail"),
    ("result_code_to_string", "bs_result_code_to_string"),
    ("result_is_success", "bs_result_is_success"),
    ("result_needs_retry", "bs_result_needs_retry"),
    ("result_is_error", "bs_result_is_error"),
    ("result_to_string", "bs_result_to_string"),
    ("result_destroy", "bs_result_destroy"),
    ("result_create", "bs_result_create"),
    ("result_success", "bs_result_success"),
    ("result_to_json", "bs_result_to_json"),
    ("result_error", "bs_result_error"),
    ("report_set_error_message", "bs_report_set_error_message"),
    ("report_get_error_message", "bs_report_get_error_message"),
    ("report_set_next_target", "bs_report_set_next_target"),
    ("report_get_next_target", "bs_report_get_next_target"),
    ("report_set_next_action", "bs_report_set_next_action"),
    ("report_get_next_action", "bs_report_get_next_action"),
    ("report_get_duration", "bs_report_get_duration"),
    ("report_add_entry", "bs_report_add_entry"),
    ("report_set_status", "bs_report_set_status"),
    ("report_get_status", "bs_report_get_status"),
    ("report_mark_start", "bs_report_mark_start"),
    ("report_mark_end", "bs_report_mark_end"),
    ("report_add_debug", "bs_report_add_debug"),
    ("report_add_error", "bs_report_add_error"),
    ("report_add_fatal", "bs_report_add_fatal"),
    ("report_add_info", "bs_report_add_info"),
    ("report_add_warn", "bs_report_add_warn"),
    ("report_to_string", "bs_report_to_string"),
    ("report_destroy", "bs_report_destroy"),
    ("report_create", "bs_report_create"),
    ("report_to_json", "bs_report_to_json"),
    ("metadata_set_string", "bs_metadata_set_string"),
    ("metadata_set_integer", "bs_metadata_set_integer"),
    ("metadata_set_boolean", "bs_metadata_set_boolean"),
    ("metadata_set_binary", "bs_metadata_set_binary"),
    ("metadata_set_double", "bs_metadata_set_double"),
    ("metadata_get_string", "bs_metadata_get_string"),
    ("metadata_get_integer", "bs_metadata_get_integer"),
    ("metadata_get_boolean", "bs_metadata_get_boolean"),
    ("metadata_get_binary", "bs_metadata_get_binary"),
    ("metadata_get_double", "bs_metadata_get_double"),
    ("metadata_destroy", "bs_metadata_destroy"),
    ("metadata_has_key", "bs_metadata_has_key"),
    ("metadata_create", "bs_metadata_create"),
    ("metadata_remove", "bs_metadata_remove"),
    ("metadata_clear", "bs_metadata_clear"),
    ("metadata_clone", "bs_metadata_clone"),
    ("metadata_merge", "bs_metadata_merge"),
    ("metadata_count", "bs_metadata_count"),
    ("metadata_size", "bs_metadata_size"),
    ("context_get_all_metadata", "bs_context_get_all_metadata"),
    ("context_set_metadata", "bs_context_set_metadata"),
    ("context_get_metadata", "bs_context_get_metadata"),
    ("context_set_user_data", "bs_context_set_user_data"),
    ("context_get_user_data", "bs_context_get_user_data"),
    ("context_get_age_ms", "bs_context_get_age_ms"),
    ("context_get_scope", "bs_context_get_scope"),
    ("context_destroy", "bs_context_destroy"),
    ("context_create", "bs_context_create"),
    ("context_get_id", "bs_context_get_id"),
    ("context_touch", "bs_context_touch"),
    ("context_clone", "bs_context_clone"),
    ("context_merge", "bs_context_merge"),
    ("pipeline_get_stage_count", "bs_pipeline_get_stage_count"),
    ("pipeline_remove_stage", "bs_pipeline_remove_stage"),
    ("pipeline_add_stage", "bs_pipeline_add_stage"),
    ("pipeline_get_stage", "bs_pipeline_get_stage"),
    ("pipeline_destroy", "bs_pipeline_destroy"),
    ("pipeline_execute", "bs_pipeline_execute"),
    ("pipeline_create", "bs_pipeline_create"),
    ("pipeline_reset", "bs_pipeline_reset"),
    ("stage_set_dependencies", "bs_stage_set_dependencies"),
    ("stage_get_dependencies", "bs_stage_get_dependencies"),
    ("stage_set_context", "bs_stage_set_context"),
    ("stage_get_context", "bs_stage_get_context"),
    ("stage_set_state", "bs_stage_set_state"),
    ("stage_get_state", "bs_stage_get_state"),
    ("stage_destroy", "bs_stage_destroy"),
    ("stage_execute", "bs_stage_execute"),
    ("stage_cleanup", "bs_stage_cleanup"),
    ("stage_create", "bs_stage_create"),
    ("stage_is_ready", "bs_stage_is_ready"),
]

SKIP_DIRS = {"build_ci_test", "build", "build-meson", "Source", ".git", "_deps", "legacy"}


def iter_source_files(root: Path) -> list[Path]:
    out: list[Path] = []
    for base_name in ("kernel", "adapter", "app", "tools"):
        base = root / base_name
        if not base.is_dir():
            continue
        for ext in ("*.c", "*.cpp", "*.h", "*.hpp", "*.md"):
            for p in base.rglob(ext):
                if any(part in SKIP_DIRS for part in p.parts):
                    continue
                out.append(p)
    return sorted(set(out))


def migrate_text(text: str) -> str:
    for old, new in sorted(EXPLICIT, key=lambda p: len(p[0]), reverse=True):
        if old.startswith("bs_"):
            continue
        text = re.sub(rf"\b{re.escape(old)}\b", new, text)
    return text


def main() -> int:
    root = repo_root()
    changed = 0
    for path in iter_source_files(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        new_text = migrate_text(text)
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            changed += 1
            print(f"[OK] updated {path.relative_to(root)}")
    print(f"[OK] migrate complete: {changed} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
