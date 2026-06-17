#ifndef BS_GATE_CHAIN_SERIALIZE_H
#define BS_GATE_CHAIN_SERIALIZE_H

#include "gate_chain_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* JSON → bs_gate_chain_t */
int bs_gate_chain_from_json(const char* json, bs_gate_chain_t** out);

/* bs_gate_chain_t → JSON */
int bs_gate_chain_to_json(const bs_gate_chain_t* chain, char** out_json, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_CHAIN_SERIALIZE_H */
