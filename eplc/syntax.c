#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "syntax.h"
#include "lexer.h"
#include "error.h"


struct SyntaxContext
{
    struct STX_SyntaxTree *tree;

    const struct LEX_LexerToken *tokens;
    int tokenCount;
    int tokensRemaining;

    const struct LEX_LexerToken  *current;
    struct STX_SyntaxTreeNode *currentNode;
};

static struct STX_SyntaxTreeNode *allocateNode(struct STX_SyntaxTree *tree);
static void initializeNode(struct STX_SyntaxTreeNode *node);

static void preorderStep(struct STX_SyntaxTree *tree, int level, struct STX_SyntaxTreeNode *node, STX_TransverseCallback callback, void *userData)
{
    int index;
    callback(node, level, userData);
    index = node->firstChildIndex;
    while (index >= 0)
    {
        struct STX_SyntaxTreeNode *current = &tree->nodes[index];
        preorderStep(tree, level+1, current, callback, userData);
        index = current->nextSiblingIndex;
    }
}

void STX_transversePreorder(struct STX_SyntaxTree *tree, STX_TransverseCallback callback, void *userData)
{
    struct STX_SyntaxTreeNode *node;
    node = &tree->nodes[tree->rootNodeIndex];
    preorderStep(tree, 0, node, callback, userData);
}


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
    child->parentIndex = node->id;
    if (node->firstChildIndex == -1)
    {
        node->firstChildIndex = child->id;
    }
    if (node->lastChildIndex != -1)
    {
        struct STX_SyntaxTreeNode *lastChild = &tree->nodes[node->lastChildIndex];
        lastChild->nextSiblingIndex = child->id;
        child->previousSiblingIndex = lastChild->id;
    }
    node->lastChildIndex = child->id;
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
        tree->nodes = realloc(
            tree->nodes, tree->nodesAllocated * sizeof(struct STX_SyntaxTreeNode));
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
        tree->attributes = realloc(
            tree->attributes, tree->attributesAllocated * sizeof(struct STX_NodeAttribute));
    }
    attribute = &tree->attributes[tree->attributeCount];
    attribute->allocated = 1;
    attribute->id = tree->attributeCount;
    tree->attributeCount++;
    return attribute;
}

static void initializeNode(struct STX_SyntaxTreeNode *node)
{
    node->parentIndex = -1;
    node->firstChildIndex = -1;
    node->lastChildIndex = -1;
    node->nextSiblingIndex = -1;
    node->previousSiblingIndex = -1;
    node->attributeIndex = -1;
}

static const struct LEX_LexerToken *getCurrent(struct SyntaxContext *context)
{
    return context->current;
}

static void acceptCurrent(struct SyntaxContext *context)
{
    assert(context->current);
    if (context->tokensRemaining)
    {
        context->tokensRemaining--;
        context->current++;
    }
    else
    {
        context->current = 0;
    }
}

static int expect(struct SyntaxContext *context, enum LEX_TokenType type)
{
    const struct LEX_LexerToken *token = getCurrent(context);

    if (!token)
    {
        return 0;
    }
    if (token->tokenType == type)
    {
        acceptCurrent(context);
        return 1;
    }
    else
    {
        return 0;
    }
}

static void ascendToParent(struct SyntaxContext *context)
{
    assert(context->currentNode->parentIndex != -1);
    context->currentNode = &context->tree->nodes[context->currentNode->parentIndex];
}

static void descendNewNode(struct SyntaxContext *context, enum STX_NodeType type)
{
    struct STX_SyntaxTreeNode *node = allocateNode(context->tree);
    initializeNode(node);
    node->nodeType = type;
    appendChild(context->tree, context->currentNode, node);
    context->currentNode = node;
}

static int parseBlock(struct SyntaxContext *context)
{
    descendNewNode(context, STX_BLOCK);
    ascendToParent(context);
    return 1;
}

static int parseVariableDeclaration(struct SyntaxContext *context)
{
    return 1;
}

static int parseDeclarations(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current = getCurrent(context);

    descendNewNode(context, STX_DECLARATIONS);
    if (current->tokenType == LEX_KW_VARDECL)
    {
        if (!parseVariableDeclaration(context)) return 0;
    }
    ascendToParent(context);
    return 1;
}

/**
 * Module ::=
 * 'module' {'exe' | 'lib' | 'dll' } ';'
 * <Declarations>
 * 'main'
 * <Block>
 */

static int parseModule(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_MODULE);

    if (!expect(context, LEX_KW_MODULE))
    {
        ERR_raiseError(E_STX_MODULE_EXPECTED);
        return 0;
    }
    current = getCurrent(context);
    if (
        (current->tokenType == LEX_KW_EXE) ||
        (current->tokenType == LEX_KW_DLL) ||
        (current->tokenType == LEX_KW_LIB))
    {
        attribute = allocateAttribute(context->tree);
        switch (current->tokenType)
        {
            case LEX_KW_EXE:
                attribute->moduleAttributes.type = STX_MOD_EXE;
            break;
            case LEX_KW_LIB:
                attribute->moduleAttributes.type = STX_MOD_LIB;
            break;
            case LEX_KW_DLL:
                attribute->moduleAttributes.type = STX_MOD_DLL;
            break;
            default:
                assert(0);
        }
        context->currentNode->attributeIndex = attribute->id;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_MODULE_TYPE_EXPECTED);
        return 0;
    }
    if (!expect(context, LEX_SEMICOLON))
    {
        ERR_raiseError(E_STX_SEMICOLON_EXPECTED);
        return 0;
    }
    if (!parseDeclarations(context)) return 0;
    if (!expect(context, LEX_KW_MAIN))
    {
        ERR_raiseError(E_STX_MAIN_EXPECTED);
        return 0;
    }
    if (!parseBlock(context)) return 0;

    ascendToParent(context);
    return 1;
}

struct STX_ParserResult STX_buildSyntaxTree(
    const struct LEX_LexerToken *tokens,
    int tokenCount
)
{
    struct STX_SyntaxTree *tree = malloc(sizeof(struct STX_SyntaxTree));
    struct SyntaxContext context;
    struct STX_ParserResult result;

    initializeSyntaxTree(tree);

    context.tokens = tokens;
    context.tokenCount = tokenCount;
    context.tokensRemaining = tokenCount;
    context.current = tokens;
    context.tree = tree;
    context.currentNode = &tree->nodes[tree->rootNodeIndex];

    parseModule(&context);

    result.tree = tree;
    {
        const struct LEX_LexerToken *current = getCurrent(&context);
        if (current)
        {
            result.line = current->beginLine;
            result.column = current->beginColumn;
        }
    }

    return result;
}

