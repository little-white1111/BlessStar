#ifndef BS_KERNEL_UI_MAP_UIDL_VALIDATOR_H
#define BS_KERNEL_UI_MAP_UIDL_VALIDATOR_H

/*
 * UIDL JSON structural validator.
 *
 * C-ST-7 contract block:
 * Thread safety: reentrant.
 * Error semantics: int return; negative on system error,
 *                  positive = number of validation errors.
 * Platform notes: Pure C.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Validate a UIDL JSON string.
 *
 * Checks:
 * - Top-level fields: uidl_version, schema_ref, controls
 * - Each control: field, widget (known enum), children, etc.
 * - Recursively validates nested children.
 *
 * @param json        UIDL JSON string (null-terminated)
 * @param len         Length of json
 * @param out_errors  Output array of error message strings (caller must
 *                    free via bs_uidl_validate_errors_free).
 * @param out_error_count  Number of errors
 * @return 0 if valid, >0 if validation errors found, <0 on system error.
 */
    int bs_uidl_validate(const char* json, size_t len,
                          char*** out_errors, size_t* out_error_count);

/**
 * Free the error array returned by bs_uidl_validate.
 */
    void bs_uidl_validate_errors_free(char** errors, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_UI_MAP_UIDL_VALIDATOR_H */
