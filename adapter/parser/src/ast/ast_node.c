#include "bs/adapter/parser/ASTNode.h"

#include <stdlib.h>
#include <string.h>

ASTNode* ast_node_create(ASTNodeType type, const char* name)
{
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node)
        return NULL;

    node->type          = type;
    node->name          = name ? strdup(name) : NULL;
    node->value         = NULL;
    node->parent        = NULL;
    node->children      = NULL;
    node->child_count   = 0;
    node->line_number   = 0;
    node->column_number = 0;
    node->user_data     = NULL;

    return node;
}

void ast_node_destroy(ASTNode* node)
{
    if (!node)
        return;

    if (node->name)
        free((void*)node->name);
    if (node->value)
        free((void*)node->value);

    for (size_t i = 0; i < node->child_count; i++)
    {
        ast_node_destroy(node->children + i);
    }

    if (node->children)
        free(node->children);

    free(node);
}

void ast_node_add_child(ASTNode* parent, ASTNode* child)
{
    if (!parent || !child)
        return;

    ASTNode* new_children =
        (ASTNode*)realloc(parent->children, sizeof(ASTNode) * (parent->child_count + 1));
    if (!new_children)
        return;

    parent->children                             = new_children;
    parent->children[parent->child_count]        = *child;
    parent->children[parent->child_count].parent = parent;
    parent->child_count++;

    free(child);
}

void ast_node_remove_child(ASTNode* parent, ASTNode* child)
{
    if (!parent || !child)
        return;

    for (size_t i = 0; i < parent->child_count; i++)
    {
        ASTNode* current = parent->children + i;
        if (current->name && child->name && strcmp(current->name, child->name) == 0)
        {
            ast_node_destroy(current);

            for (size_t j = i; j < parent->child_count - 1; j++)
            {
                parent->children[j] = parent->children[j + 1];
            }

            parent->child_count--;
            return;
        }
    }
}

ASTNode* ast_node_get_child(const ASTNode* parent, size_t index)
{
    if (!parent || index >= parent->child_count)
        return NULL;
    return parent->children + index;
}

ASTNode* ast_node_find_child(const ASTNode* parent, const char* name)
{
    if (!parent || !name)
        return NULL;

    for (size_t i = 0; i < parent->child_count; i++)
    {
        ASTNode* child = parent->children + i;
        if (child->name && strcmp(child->name, name) == 0)
        {
            return child;
        }
    }

    return NULL;
}

void ast_node_set_value(ASTNode* node, const char* value)
{
    if (!node)
        return;
    if (node->value)
        free((void*)node->value);
    node->value = value ? strdup(value) : NULL;
}

const char* ast_node_get_value(const ASTNode* node)
{
    return node ? node->value : NULL;
}

void ast_node_set_position(ASTNode* node, size_t line, size_t column)
{
    if (!node)
        return;
    node->line_number   = line;
    node->column_number = column;
}

void ast_node_get_position(const ASTNode* node, size_t* line, size_t* column)
{
    if (!node || !line || !column)
        return;
    *line   = node->line_number;
    *column = node->column_number;
}

void ast_node_set_user_data(ASTNode* node, void* data)
{
    if (!node)
        return;
    node->user_data = data;
}

void* ast_node_get_user_data(const ASTNode* node)
{
    return node ? node->user_data : NULL;
}

ASTNode* ast_node_clone(const ASTNode* node)
{
    if (!node)
        return NULL;

    ASTNode* clone = (ASTNode*)malloc(sizeof(ASTNode));
    if (!clone)
        return NULL;

    clone->type          = node->type;
    clone->name          = node->name ? strdup(node->name) : NULL;
    clone->value         = node->value ? strdup(node->value) : NULL;
    clone->parent        = NULL;
    clone->children      = NULL;
    clone->child_count   = 0;
    clone->line_number   = node->line_number;
    clone->column_number = node->column_number;
    clone->user_data     = NULL;

    for (size_t i = 0; i < node->child_count; i++)
    {
        ASTNode* child_clone = ast_node_clone(node->children + i);
        if (child_clone)
        {
            ast_node_add_child(clone, child_clone);
        }
    }

    return clone;
}

void ast_node_accept(ASTNode* node, void (*visitor)(ASTNode*, void*), void* context)
{
    if (!node || !visitor)
        return;

    visitor(node, context);

    for (size_t i = 0; i < node->child_count; i++)
    {
        ast_node_accept(node->children + i, visitor, context);
    }
}

const char* ast_node_type_to_string(ASTNodeType type)
{
    switch (type)
    {
    case AST_NODE_ROOT:
        return "ROOT";
    case AST_NODE_WORKFLOW:
        return "WORKFLOW";
    case AST_NODE_STAGE:
        return "STAGE";
    case AST_NODE_COMPONENT:
        return "COMPONENT";
    case AST_NODE_CONFIG:
        return "CONFIG";
    case AST_NODE_PROPERTY:
        return "PROPERTY";
    case AST_NODE_DEPENDS_ON:
        return "DEPENDS_ON";
    case AST_NODE_EXPRESSION:
        return "EXPRESSION";
    case AST_NODE_VARIABLE:
        return "VARIABLE";
    case AST_NODE_LIST:
        return "LIST";
    case AST_NODE_MAP:
        return "MAP";
    case AST_NODE_LITERAL:
        return "LITERAL";
    default:
        return "UNKNOWN";
    }
}
