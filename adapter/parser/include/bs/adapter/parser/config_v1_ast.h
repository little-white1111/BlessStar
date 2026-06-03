#ifndef BS_ADAPTER_PARSER_CONFIG_V1_AST_H
#define BS_ADAPTER_PARSER_CONFIG_V1_AST_H

/*
 * C-ST-7 contract block:
 * Thread safety: not thread-safe; caller serializes ast create/destroy.
 * Error semantics: create may return null on OOM; destroy accepts null.
 * Platform notes: N/A.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ConfigV1Metadata
    {
        char*                    key;
        char*                    value;
        struct ConfigV1Metadata* next;
    } ConfigV1Metadata;

    typedef struct ConfigV1Instruction
    {
        char*                       type;
        char*                       name;
        ConfigV1Metadata*           metadata;
        struct ConfigV1Instruction* next;
    } ConfigV1Instruction;

    typedef struct ConfigV1Ast
    {
        char*                kernel_version;
        char*                adapter_version;
        char**               manual_requirements;
        size_t               manual_requirements_count;
        ConfigV1Instruction* instructions;
        size_t               instructions_count;
    } ConfigV1Ast;

    ConfigV1Ast* bs_config_v1_ast_create(void);
    void         bs_config_v1_ast_destroy(ConfigV1Ast* ast);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_V1_AST_H */
