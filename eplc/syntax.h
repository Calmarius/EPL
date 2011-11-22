#ifndef SYNTAX_H
#define SYNTAX_H

struct LEX_LexerToken;

enum STX_NodeType
{
    STX_ROOT,
};

/**
 * Stores the attributes of a single node
 */
struct STX_NodeAttribute
{
    int id;
    int allocated;
};

/**
 * Stores a single syntax tree node.
 */
struct STX_SyntaxTreeNode
{
    int id;
    int parent;
    int firstChild;
    int lastChild;
    int nextSibling;
    int previousSibling;
    int attributes;
    enum STX_NodeType nodeType;
    int allocated;
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

struct STX_SyntaxTree STX_buildSyntaxTree(
    const struct LEX_LexerToken *token,
    int tokenCount
);

void STX_destroySyntaxTree(struct STX_SyntaxTree *tree);



#endif // SYNTAX_H
