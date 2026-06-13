#ifndef BS_ADAPTER_PARSER_META_RULE_H
#define BS_ADAPTER_PARSER_META_RULE_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure function; no global state; reentrant for disjoint outputs.
 * Error semantics: returns index of first failing rule or (size_t)-1 on all-pass;
 *   err buffer populated on mismatch (caller provides buffer).
 * Platform notes: uses POSIX regex.h for REGEX op; portable C99.
 */

#include "bs/kernel/ir/ir.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BS_META_EQ,          // ==
    BS_META_NE,          // !=
    BS_META_GT,          // >
    BS_META_LT,          // <
    BS_META_GE,          // >=
    BS_META_LE,          // <=
    BS_META_EXISTS,      // key exists
    BS_META_NOT_EXISTS,  // key does not exist
    BS_META_REGEX,       // POSIX regex match
    BS_META_CONTAINS     // substring contains
} BsMetaOp;

typedef struct {
    const char* instr_name;  // target instruction name (NULL or "" = match all)
    const char* key;         // metadata field name
    BsMetaOp    op;          // operator
    const char* value;       // comparison value (ignored for EXISTS/NOT_EXISTS)
} BsMetaRule;

/**
 * Check IRInstructionList against an array of metadata rules.
 *
 * For each rule, iterates all instructions (filtered by instr_name if non-empty).
 * All rules must pass (AND semantics). Returns index of first failing rule,
 * or (size_t)-1 if all pass.
 *
 * @param instructions  Parsed IR instruction list
 * @param rules         Rule array
 * @param rule_count    Number of rules
 * @param err           Error buffer (optional, set on first mismatch)
 * @param err_cap       Error buffer capacity
 * @return              Index of first failing rule, or (size_t)-1 on all-pass
 */
size_t bs_meta_rule_check(const IRInstructionList* instructions,
                          const BsMetaRule* rules, size_t rule_count,
                          char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_META_RULE_H */
