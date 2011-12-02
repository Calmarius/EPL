#ifndef SYNTAX_H
#define SYNTAX_H

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
    STX_STATEMENT,
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
    STX_TP_HANDLE
};

enum STX_ParameterDirection
{
    STX_PD_IN,
    STX_PD_OUT,
    STX_PD_REF
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
    };
    const char *name;
    int nameLength;
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
    enum STX_NodeType nodeType;
    int allocated;
    struct STX_SyntaxTree *belongsTo;
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

struct STX_ParserResult STX_buildSyntaxTree(
    const struct LEX_LexerToken *token,
    int tokenCount
);

typedef void (*STX_TransverseCallback)(struct STX_SyntaxTreeNode *node, int level, void *userData);

void STX_destroySyntaxTree(struct STX_SyntaxTree *tree);

void STX_transversePreorder(struct STX_SyntaxTree *tree, STX_TransverseCallback callback, void *userData);



#endif // SYNTAX_H
