#ifndef BS_ADAPTER_PARSER_AST_NODE_H
#define BS_ADAPTER_PARSER_AST_NODE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ASTNodeType
    {
        AST_NODE_ROOT,
        AST_NODE_WORKFLOW,
        AST_NODE_STAGE,
        AST_NODE_COMPONENT,
        AST_NODE_CONFIG,
        AST_NODE_PROPERTY,
        AST_NODE_DEPENDS_ON,
        AST_NODE_EXPRESSION,
        AST_NODE_VARIABLE,
        AST_NODE_LIST,
        AST_NODE_MAP,
        AST_NODE_LITERAL
    } ASTNodeType;

    typedef struct ASTNode ASTNode;

    struct ASTNode
    {
        ASTNodeType type;
        const char* name;
        const char* value;
        ASTNode*    parent;
        ASTNode*    children;
        size_t      child_count;
        size_t      line_number;
        size_t      column_number;
        void*       user_data;
    };

    ASTNode* ast_node_create(ASTNodeType type, const char* name);
    void     ast_node_destroy(ASTNode* node);

    void     ast_node_add_child(ASTNode* parent, ASTNode* child);
    void     ast_node_remove_child(ASTNode* parent, ASTNode* child);
    ASTNode* ast_node_get_child(const ASTNode* parent, size_t index);
    ASTNode* ast_node_find_child(const ASTNode* parent, const char* name);

    void        ast_node_set_value(ASTNode* node, const char* value);
    const char* ast_node_get_value(const ASTNode* node);

    void ast_node_set_position(ASTNode* node, size_t line, size_t column);
    void ast_node_get_position(const ASTNode* node, size_t* line, size_t* column);

    void  ast_node_set_user_data(ASTNode* node, void* data);
    void* ast_node_get_user_data(const ASTNode* node);

    ASTNode* ast_node_clone(const ASTNode* node);
    void     ast_node_accept(ASTNode* node, void (*visitor)(ASTNode*, void*), void* context);

    const char* ast_node_type_to_string(ASTNodeType type);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_AST_NODE_H
