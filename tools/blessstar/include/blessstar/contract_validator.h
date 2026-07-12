#ifndef BLESSSTAR_CONTRACT_VALIDATOR_H
#define BLESSSTAR_CONTRACT_VALIDATOR_H

#include "blessstar/contract_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate a parsed contract against the schema rules.
 * Returns number of errors found (0 = valid).
 * Error messages are written to error_buf (up to error_buf_size).
 * Each error is separated by newline.
 */
int bs_contract_validate(const bs_contract_t* contract, char* error_buf, size_t error_buf_size);

#ifdef __cplusplus
}
#endif

#endif
