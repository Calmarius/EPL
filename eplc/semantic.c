#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "semantic.h"
#include "syntax.h"
#include "error.h"
#include "assocarray.h"
#include "error.h"

struct Scope
{
    struct Scope *parentScope;
    struct ASSOC_Array *symbols;
    /// The node of the symbol that created the scope.
    struct STX_SyntaxTreeNode *node;
};

struct SemanticContext
{
    struct STX_SyntaxTree *tree;
    struct STX_SyntaxTreeNode *currentNode;

    struct Scope *currentScope;

    struct Scope **scopePointers;
    int scopePointersAllocated;
    int scopeCount;

    int currentLevel;
};

static struct Scope *allocateScope(
    struct SemanticContext *context,
    struct Scope *parentScope)
{
    struct Scope *newScope;

    if (context->scopeCount == context->scopePointersAllocated)
    {
        if (!context->scopePointersAllocated)
        {
            context->scopePointersAllocated = 20;
        }
        else
        {
            context->scopePointersAllocated *= 2;
        }
        context->scopePointers =
            realloc(
                context->scopePointers,
                context->scopePointersAllocated * sizeof(*context->scopePointers)
            );
    }
    newScope = calloc(1, sizeof(struct Scope));
    context->scopePointers[context->scopeCount++] = newScope;
    newScope->parentScope = parentScope;
    newScope->symbols = malloc(sizeof(*newScope->symbols));
    ASSOC_initializeArray(newScope->symbols);
    return newScope;
}

static struct STX_SyntaxTreeNode *getCurrentNode(struct SemanticContext *context)
{
    return context->currentNode;
}

static enum STX_NodeType getCurrentNodeType(struct SemanticContext *context)
{
    return getCurrentNode(context)->nodeType;
}

static void descendNewScope(struct SemanticContext *context)
{
    context->currentScope = allocateScope(context, context->currentScope);
//    context->currentScope->level = context->currentLevel;
    context->currentScope->node = getCurrentNode(context);
}

static void ascendToParentScope(struct SemanticContext *context)
{
    context->currentScope = context->currentScope->parentScope;
}


static void addSymbolToCurrentScope(
    struct SemanticContext *context)
{
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    const struct STX_NodeAttribute *attr = STX_getNodeAttribute(node);

    ASSOC_insert(context->currentScope->symbols, attr->name, attr->nameLength, node);
}


static int handleSymbol(struct ASSOC_KeyValuePair *kvp, int level, int index, void *userData)
{
    printf("%.*s : %d\n", kvp->keyLength, kvp->key, ((struct STX_SyntaxTreeNode*)kvp->value)->nodeType);
    return 1;
}

static void dumpScopes(struct SemanticContext *context)
{
    int i;
    for (i = 0; i < context->scopeCount; i++)
    {
        struct Scope *scope = context->scopePointers[i];
        const struct STX_NodeAttribute *attr = STX_getNodeAttribute(scope->node);
        printf(
            "Scope %p, parentScope: %p, nodeTypeId : %d, name : %.*s\n",
            scope,
            scope->parentScope,
            scope->node->nodeType,
            attr ? attr->nameLength : 0,
            attr ? attr->name : ""
        );
        ASSOC_transverseInorder(scope->symbols, handleSymbol, 0);
    }
}

static int enterCurrentNode(struct SemanticContext *context)
{
    if (context->currentNode->firstChildIndex == -1)
    {
        ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        return 0;
    }
    context->currentNode = &context->currentNode->belongsTo->nodes[context->currentNode->firstChildIndex];

    return 1;
}

static int assertNodeType(struct SemanticContext *context, enum STX_NodeType type)
{
    if (getCurrentNodeType(context) != type)
    {
        ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        return 0;
    }
    return 1;
}

static int moveToNextNode(struct SemanticContext *context)
{
    if (getCurrentNode(context)->nextSiblingIndex < 0) return 0;
    context->currentNode = &context->tree->nodes[context->currentNode->nextSiblingIndex];
    return 1;
}

static int leaveCurrentNode(struct SemanticContext *context)
{
    context->currentNode = &context->tree->nodes[context->currentNode->parentIndex];
    return 1;
}

static int checkParameter(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_PARAMETER)) return 0;
    addSymbolToCurrentScope(context);
    return 1;
}

static int checkParameterList(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_PARAMETER_LIST)) return 0;
    if (enterCurrentNode(context))
    {
        do
        {
            if (!checkParameter(context)) return 0;
        }
        while (moveToNextNode(context));
        leaveCurrentNode(context);
    }
    return 1;
}

static int checkStatement(struct SemanticContext *context)
{
    switch (getCurrentNodeType(context))
    {
        case STX_VARDECL:
            addSymbolToCurrentScope(context);
        break;
        case STX_EXPRESSION_STATEMENT:
        case STX_ASSIGNMENT:
        break;
        default:
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
            return 0;
        break;
    }
    return 1;
}

static int checkBlock(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_BLOCK)) return 0;
    descendNewScope(context);
    if (enterCurrentNode(context))
    {
        do
        {
            if (!checkStatement(context)) return 0;
        }
        while (moveToNextNode(context));
        leaveCurrentNode(context);
    }
    ascendToParentScope(context);
    return 1;
}

static int checkFunction(struct SemanticContext *context)
{
    const struct STX_NodeAttribute *attr;

    if (!assertNodeType(context, STX_FUNCTION)) return 0;
    attr = STX_getNodeAttribute(getCurrentNode(context));
    if (attr->functionAttributes.isExternal)
    {
        return 1;
    }

    descendNewScope(context);
    if (!enterCurrentNode(context)) return 0;
    {
        if (!assertNodeType(context, STX_TYPE)) return 0;
        if (!moveToNextNode(context)) return 0;
        if (!checkParameterList(context)) return 0;
        if (!moveToNextNode(context)) return 0;
        if (!checkBlock(context)) return 0;
    }
    ascendToParentScope(context);
    leaveCurrentNode(context);
    return 1;
}

static int checkModule(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_MODULE)) return 0;
    if (!enterCurrentNode(context)) return 0;
    do
    {
        switch (getCurrentNodeType(context))
        {
            case STX_FUNCTION:
                addSymbolToCurrentScope(context);
                if (!checkFunction(context)) return 0;
            break;
            case STX_VARDECL:
                addSymbolToCurrentScope(context);
            break;
            default:
            break;
        }
    }
    while (moveToNextNode(context));
    leaveCurrentNode(context);

    return 1;
}

int checkRootNode(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_ROOT)) return 0;
    if (!enterCurrentNode(context)) return 0;
    if (!assertNodeType(context, STX_MODULE)) return 0;
    checkModule(context);

    return 1;
}

struct STX_SyntaxTreeNode *STX_getRootNode(struct STX_SyntaxTree *tree);

int SMC_checkSyntaxTree(struct STX_SyntaxTree *syntaxTree)
{
    struct SemanticContext sc = {0};
    int ok;

    sc.tree = syntaxTree;
    sc.currentNode = STX_getRootNode(syntaxTree);
    descendNewScope(&sc);
    ok = checkRootNode(&sc);
    ascendToParentScope(&sc);
    dumpScopes(&sc);
    return ok;
}




