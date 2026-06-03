#ifndef BS_ADAPTER_PLUGIN_PLUGIN_IR_REQUIREMENTS_H
#define BS_ADAPTER_PLUGIN_PLUGIN_IR_REQUIREMENTS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Requirement merge is read-only on IR lists post-parse.
 * Error semantics: Filters instructions failing plugin-declared requirements.
 * Platform notes: Extends kernel builtin requirements at attach time.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * PLUGIN-VIII-5: every instruction type in ref file must exist in kernel builtin list.
     * File format: one instruction type per line; '#' comments; blank lines ignored.
     * @return 0 ok, non-zero invalid or unreadable
     */
    int bs_adapter_plugin_validate_ir_requirements_ref(const char* ref_path);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PLUGIN_PLUGIN_IR_REQUIREMENTS_H */
