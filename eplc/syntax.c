#include <string.h>
#include <stdlib.h>

#include "syntax.h"


struct SyntaxContext
{
};

static struct STX_SyntaxTreeNode *allocateNode(struct STX_SyntaxTree *tree);
static void initializeNode(struct STX_SyntaxTreeNode *node);

static void initializeSyntaxTree(struct STX_SyntaxTree *tree)
{
    struct STX_SyntaxTreeNode *node;
    memset(tree, 0, sizeof(struct STX_SyntaxTree));
    node = allocateNode(tree);
    initializeNode(node);
    tree->rootNodeIndex = node->id;
    node->nodeType = STX_ROOT;
}

static void appendChild(
    struct STX_SyntaxTree *tree,
    struct STX_SyntaxTreeNode *node,
    struct STX_SyntaxTreeNode *child)
{
    child->parent = node->id;
    if (node->firstChild == -1)
    {
        node->firstChild = child->id;
    }
    if (node->lastChild != -1)
    {
        struct STX_SyntaxTreeNode *lastChild = &tree->nodes[node->lastChild];
        lastChild->nextSibling = child->id;
        child->previousSibling = lastChild->id;
    }
    node->lastChild = child->id;
}

static struct STX_SyntaxTreeNode *allocateNode(struct STX_SyntaxTree *tree)
{
    struct STX_SyntaxTreeNode *node;
    if (tree->nodeCount == tree->nodesAllocated)
    {
        if (tree->nodes)
        {
            tree->nodesAllocated <<= 1;
        }
        else
        {
            tree->nodesAllocated = 10;
        }
        tree->nodes = realloc(tree->nodes, tree->nodesAllocated);
    }
    node = &tree->nodes[tree->nodeCount];
    node->allocated = 1;
    node->id = tree->nodeCount;
    tree->nodeCount++;
    return node;
}

static struct STX_NodeAttribute *allocateAttribute(struct STX_SyntaxTree *tree)
{
    struct STX_NodeAttribute *attribute;
    if (tree->attributeCount == tree->attributesAllocated)
    {
        if (tree->attributes)
        {
            tree->attributesAllocated <<= 1;
        }
        else
        {
            tree->attributesAllocated = 10;
        }
        tree->attributes = realloc(tree->attributes, tree->attributesAllocated);
    }
    attribute = &tree->attributes[tree->attributeCount];
    attribute->allocated = 1;
    attribute->id = tree->attributeCount;
    tree->attributeCount++;
    return attribute;
}

static void initializeNode(struct STX_SyntaxTreeNode *node)
{
    node->parent = -1;
    node->firstChild = -1;
    node->lastChild = -1;
    node->nextSibling = -1;
    node->previousSibling = -1;
    node->attributes = -1;
}

struct STX_SyntaxTree STX_buildSyntaxTree(
    const struct LEX_LexerToken *token,
    int tokenCount
)
{
    struct STX_SyntaxTree result;
    initializeSyntaxTree(&result);



    return result;
}

