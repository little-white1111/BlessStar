#ifndef BLESSSTAR_SPEC_RENDERER_H
#define BLESSSTAR_SPEC_RENDERER_H

#include "blessstar/contract_parser.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render a Mustache template file with contract data to output stream.
 * Supports:
 *   {{var}} - variable substitution
 *   {{#section}}...{{/section}} - section (if truthy, render once)
 *   {{^section}}...{{/section}} - inverted section (if falsy, render once)
 *   {{#list}}...{{/list}} - list iteration (for array fields)
 *   
 * Built-in transforms:
 *   {{type_upper}} - field type as uppercase
 *   {{trigger_dot_to_underscore}} - replace '.' with '_' in effect trigger
 *   {{biz_id_underscore}} - replace '.' with '_' in biz_id
 *
 * Returns 0 on success, -1 on error.
 */
int bs_render_template(const bs_contract_t* contract, const char* template_text, 
                        const char* lang, FILE* output);

/**
 * Render template from file.
 */
int bs_render_template_file(const bs_contract_t* contract, const char* template_path,
                             const char* lang, FILE* output);

#ifdef __cplusplus
}
#endif

#endif
