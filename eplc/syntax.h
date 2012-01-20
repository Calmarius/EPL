#ifndef SYNTAX_H
#define SYNTAX_H

#include "lexer.h"

struct LEX_LexerToken;

enum STX_NodeType
{
    STX_ROOT,
    STX_MODULE,
    STX_DECLARATIONS,
    STX_BLOCK,
    STX_TYPE,
    STX_VARDECL,
    STX_TYPE_PREFIX,
    STX_PARAMETER,
    STX_ARGUMENT_LIST,
    STX_FUNCTION,
    STX_EXPRESSION_STATEMENT,
    STX_TERM,
    STX_RETURN_STATEMENT,
    STX_EXPRESSION,
    STX_OPERATOR,
    STX_IF_STATEMENT,
    STX_LOOP_STATEMENT,
    STX_ASSIGNMENT,
    STX_NAMESPACE,
    STX_USING,
    STX_QUALIFIED_NAME,
    STX_QUALIFIED_NAME_PART,
    STX_STRUCT,
    STX_FIELD,
    STX_COMMENT,
    STX_FUNCPTR,
    STX_SWITCH,
    STX_CASE,
    STX_CONTINUE,
    STX_BREAK,
    STX_OPERATOR_FUNCTION,
    STX_PLATFORM,
    STX_FOR_PLATFORMS,
    STX_PARAMETER_LIST,
    STX_PLATFORM_LIST,
};

enum STX_ModuleAttribute
{
    STX_MOD_EXE,
    STX_MOD_DLL,
    STX_MOD_LIB
};

enum STX_TypePrefix
{
    STX_TP_POINTER,
    STX_TP_LOCALPTR,
    STX_TP_BUFFER,
    STX_TP_HANDLE,
    STX_TP_STATICPTR
};

enum STX_ParameterDirection
{
    STX_PD_IN,
    STX_PD_OUT,
    STX_PD_REF
};

enum STX_TermType
{
   STX_TT_SIMPLE,
   STX_TT_FUNCTION_CALL,
   STX_TT_CAST_EXPRESSION,
   STX_TT_PARENTHETICAL,
   STX_TT_UNARY_OPERATOR,
   STX_TT_ARRAY_SUBSCRIPT
};

/**
 * Stores the attributes of a single node
 */
struct STX_NodeAttribute
{
    int id;
    int allocated;

    union
    {
        struct
        {
            enum STX_ModuleAttribute type;
        } moduleAttributes;
        struct
        {
            enum STX_TypePrefix type;
            int elements;
        } typePrefixAttributes;
        struct
        {
            enum STX_ParameterDirection direction;
        } parameterAttributes;
        struct
        {
            enum LEX_TokenType type;
        } operatorAttributes;
        struct
        {
            enum STX_TermType termType;
            enum LEX_TokenType tokenType;
        } termAttributes;
        struct
        {
            int caseValue;
            int isDefault;
        } caseAttributes;
        struct
        {
            enum LEX_TokenType precedence; // used for operators
            const char *externalLocation;
            int externalLocationLength;
            const char *externalFileType;
            int externalFileTypeLength;
            int isExternal;
        } functionAttributes;
        struct
        {
            int levels;
            int associatedNodeId;
        } breakContinueAttributes;
        struct
        {
            int hasBreak;
        } loopAttributes;
    };
    const char *name;
    int nameLength;
    const char *comment;
    int commentLength;
    int symbolTableEntry;
    struct STX_SyntaxTreeNode *symbolDefinitionNode;

    struct STX_SyntaxTree *belongsTo;

};

/**
 * Stores a single syntax tree node.
 */
struct STX_SyntaxTreeNode
{
    int id;
    int parentIndex;
    int firstChildIndex;
    int lastChildIndex;
    int nextSiblingIndex;
    int previousSiblingIndex;
    int attributeIndex;
    int beginLine;
    int beginColumn;
    int endLine;
    int endColumn;
    enum STX_NodeType nodeType;
    int allocated;
    struct STX_SyntaxTree *belongsTo;
    int scopeId;
};

/**
 * Stores the syntax tree itself.
 */
struct STX_SyntaxTree
{
    struct STX_SyntaxTreeNode *nodes;
    int nodesAllocated;
    int nodeCount;

    struct STX_NodeAttribute *attributes;
    int attributesAllocated;
    int attributeCount;

    int rootNodeIndex;

};

struct STX_ParserResult
{
    struct STX_SyntaxTree *tree;

    int line;
    int column;
};

struct STX_TreeIterator
{
    struct STX_SyntaxTreeNode *current;
    struct STX_SyntaxTreeNode *previous;
};

struct STX_ParserResult STX_buildSyntaxTree(
    const struct LEX_LexerToken *token,
    int tokenCount
);

typedef int (*STX_TransverseCallback)(
    struct STX_SyntaxTreeNode *node,
    int level,
    void *userData
);

void STX_destroySyntaxTree(struct STX_SyntaxTree *tree);

int STX_transversePreorder(struct STX_SyntaxTree *tree, STX_TransverseCallback callback, void *userData);

struct STX_NodeAttribute *STX_getNodeAttribute(const struct STX_SyntaxTreeNode *node);

struct STX_SyntaxTreeNode *STX_getParentNode(const struct STX_SyntaxTreeNode *node);

struct STX_SyntaxTreeNode *STX_getFirstChild(const struct STX_SyntaxTreeNode *node);

struct STX_SyntaxTreeNode *STX_getLastChild(const struct STX_SyntaxTreeNode *node);

struct STX_SyntaxTreeNode *STX_getNext(const struct STX_SyntaxTreeNode *node);

void STX_initializeTreeIterator(struct STX_TreeIterator *iterator, struct STX_SyntaxTreeNode *node);

struct STX_SyntaxTreeNode *STX_getNextPreorder(struct STX_TreeIterator *iterator);

const char *STX_nodeTypeToString(enum STX_NodeType nodeType);

#endif // SYNTAX_H
