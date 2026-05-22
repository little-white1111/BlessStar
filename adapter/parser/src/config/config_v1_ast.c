#include "bs/adapter/parser/config_v1_ast.h"

#include <stdlib.h>

static void destroy_metadata(ConfigV1Metadata* meta)
{
    while (meta)
    {
        ConfigV1Metadata* next = meta->next;
        free(meta->key);
        free(meta->value);
        free(meta);
        meta = next;
    }
}

static void destroy_instructions(ConfigV1Instruction* head)
{
    while (head)
    {
        ConfigV1Instruction* next = head->next;
        free(head->type);
        free(head->name);
        destroy_metadata(head->metadata);
        free(head);
        head = next;
    }
}

ConfigV1Ast* config_v1_ast_create(void)
{
    return (ConfigV1Ast*)calloc(1, sizeof(ConfigV1Ast));
}

void config_v1_ast_destroy(ConfigV1Ast* ast)
{
    if (!ast)
        return;
    free(ast->kernel_version);
    free(ast->adapter_version);
    if (ast->manual_requirements)
    {
        for (size_t i = 0; i < ast->manual_requirements_count; ++i)
            free(ast->manual_requirements[i]);
        free(ast->manual_requirements);
    }
    destroy_instructions(ast->instructions);
    free(ast);
}
