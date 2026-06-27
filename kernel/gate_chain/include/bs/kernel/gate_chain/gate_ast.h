#ifndef BS_GATE_AST_H
#define BS_GATE_AST_H

#include <bs/kernel/gate_chain/gate_chain_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════
 * Gate AST Node Types (mini AST for LLM output → DAG compilation)
 * ══════════════════════════════════════════════════════════════════════ */

/* AST JSON schema types:
 *   condition — { type:"condition", field, op, value }
 *   and       — { type:"and", left:{...}, right:{...} }
 *   or        — { type:"or", left:{...}, right:{...} }
 *   not       — { type:"not", node:{...} }
 *   then      — { type:"then", when:{...}, do:{...} }
 *   action    — { type:"action", name, value }
 */

/* ── Compile an AST JSON string into a DAG chain ────────────────────── */
/* Input: JSON string conforming to Gate AST schema.
 * Output: bs_gate_chain_t* with root pointing to compiled DAG.
 * Returns NULL on parse/compilation error. */
bs_gate_chain_t* bs_gate_ast_compile(const char* ast_json);

/* ── Convenience: build common gate patterns ────────────────────────── */

/* AND of conditions: { and: [ {field,op,val}, {field,op,val}, ... ] } */
bs_gate_chain_t* bs_gate_ast_compile_and(const char** field_keys,
                                          const char** ops,
                                          const char** values,
                                          size_t count);

/* Single condition: { condition: field op value } */
bs_gate_chain_t* bs_gate_ast_compile_condition(const char* field_key,
                                                 const char* op,
                                                 const char* value);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_AST_H */
