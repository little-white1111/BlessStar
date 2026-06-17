#ifndef BS_KERNEL_SCHEMA_CUSTOM_VALIDATOR_H
#define BS_KERNEL_SCHEMA_CUSTOM_VALIDATOR_H

/*
 * Custom validator dual-expression framework:
 *   expr string interpreter + C function registration.
 *
 * C-ST-7 contract block:
 * Thread safety: reentrant (bs_custom_validator_eval_expr uses only stack).
 * Error semantics: 1 = valid, 0 = invalid, -1 = runtime error.
 * Platform notes: Pure C; minimal expr parser.
 */

#include "schema_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Evaluate a custom validator expression string.
 * Supported: value variable, numeric/string/bool literals,
 *   arithmetic (+, -), comparison (==, !=, >, <, >=, <=),
 *   logical (&&, ||, !), grouping ().
 *
 * @param expr  Expression string (e.g. "value > 0 && value < 100")
 * @param val   Current config value being validated
 * @param err_buf  Error buffer for messages
 * @param err_sz   Error buffer size
 * @return      1 valid, 0 invalid, -1 runtime error
 */
    int bs_custom_validator_eval_expr(const char* expr,
                                       const bs_value_t* val,
                                       char* err_buf, size_t err_sz);

/**
 * Global C validator table (simple, non-thread-safe).
 * For use when no registry is available.
 */

/**
 * Register a C validator globally.
 */
    int bs_custom_validator_global_register(const char* name,
                                             bs_custom_validator_fn fn);

/**
 * Find a globally registered validator.
 */
    int bs_custom_validator_global_find(const char* name,
                                         bs_custom_validator_fn* out_fn);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_CUSTOM_VALIDATOR_H */
