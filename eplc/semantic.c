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
    int id;
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
    int currentScopeId;
};

static int checkStatement(struct SemanticContext *context);
static int checkDeclaration(struct SemanticContext *context);

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
    context->currentScope->id = context->currentScopeId++;
//    context->currentScope->level = context->currentLevel;
    context->currentScope->node = getCurrentNode(context);
}

static void ascendToParentScope(struct SemanticContext *context)
{
    context->currentScope = context->currentScope->parentScope;
}


static int addSymbolToCurrentScope(
    struct SemanticContext *context)
{
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    const struct STX_NodeAttribute *attr = STX_getNodeAttribute(node);
    struct Scope *currentScope = context->currentScope;
    const char *name = attr->name;
    int length = attr->nameLength;
    int found = 0;

    for(;;)
    {
        if (ASSOC_find(currentScope->symbols, name, length))
        {
            found = 1;
            break;
        }
        if (currentScope->node->nodeType == STX_BLOCK)
        {
            currentScope = currentScope->parentScope;
        }
        else
        {
            break;
        }
    }

    if (!found)
    {
        ASSOC_insert(context->currentScope->symbols, attr->name, attr->nameLength, node);
    }
    else
    {
        ERR_raiseError(E_SMC_REDEFINITION_OF_SYMBOL);
        return 0;
    }
    return 1;
}


static int handleSymbol(struct ASSOC_KeyValuePair *kvp, int level, int index, void *userData)
{
    printf(
        "%.*s : %s\n",
        kvp->keyLength,
        kvp->key,
        STX_nodeTypeToString(((struct STX_SyntaxTreeNode*)kvp->value)->nodeType)
    );
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
            "Scope %p, parentScope: %p, nodeType : %s, name : %.*s\n",
            scope,
            scope->parentScope,
            STX_nodeTypeToString(scope->node->nodeType),
            attr ? attr->nameLength : 0,
            attr ? attr->name : ""
        );
        ASSOC_transverseInorder(scope->symbols, handleSymbol, 0);
    }
}

static int enterCurrentNode(struct SemanticContext *context, int needError)
{
    if (context->currentNode->firstChildIndex == -1)
    {
        if (needError)
        {
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        }
        return 0;
    }
    context->currentNode = &context->currentNode->belongsTo->nodes[context->currentNode->firstChildIndex];
    context->currentNode->scopeId = context->currentScope->id;

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

static int moveToNextNode(struct SemanticContext *context, int needError)
{
    if (getCurrentNode(context)->nextSiblingIndex < 0)
    {
        if (needError)
        {
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        }
        return 0;
    }
    context->currentNode = &context->tree->nodes[context->currentNode->nextSiblingIndex];
    context->currentNode->scopeId = context->currentScope->id;
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
    return addSymbolToCurrentScope(context);
}

static int checkParameterList(
    struct SemanticContext *context,
    int minParameterCount,
    int maxParameterCount
)
{
    int count = 0;
    if (!assertNodeType(context, STX_PARAMETER_LIST)) return 0;
    if (enterCurrentNode(context, 0))
    {
        do
        {
            if (!checkParameter(context)) return 0;
            count++;
        }
        while (moveToNextNode(context, 0));
        leaveCurrentNode(context);
        if (minParameterCount >= 0)
        {
            if (count < minParameterCount)
            {
                leaveCurrentNode(context);
                ERR_raiseError(E_SMC_TOO_FEW_PARAMETERS);
                return 0;
            }
        }
        if (maxParameterCount >= 0)
        {
            if (count > maxParameterCount)
            {
                leaveCurrentNode(context);
                ERR_raiseError(E_SMC_TOO_MANY_PARAMETERS);
                return 0;
            }
        }
    }
    return 1;
}

static int checkBlock(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_BLOCK)) return 0;
    descendNewScope(context);
    if (enterCurrentNode(context, 0))
    {
        do
        {
            if (!checkStatement(context)) return 0;
        }
        while (moveToNextNode(context, 0));
        leaveCurrentNode(context);
    }
    ascendToParentScope(context);
    return 1;
}

static int checkIfStatement(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_IF_STATEMENT)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!moveToNextNode(context, 1)) return 0;
    if (!checkBlock(context)) return 0;
    if (moveToNextNode(context, 0))
    {
        switch (getCurrentNodeType(context))
        {
            case STX_BLOCK:
                if (!checkBlock(context)) return 0;
            break;
            case STX_IF_STATEMENT:
                if (!checkIfStatement(context)) return 0;
            break;
            default:
                ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
                return 0;
            break;
        }
    }
    leaveCurrentNode(context);

    return 1;
}

static int checkLoopStatement(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_LOOP_STATEMENT)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!checkBlock(context)) return 0;
    if (moveToNextNode(context, 0))
    {
        if (!checkBlock(context)) return 0;
    }
    leaveCurrentNode(context);
    return 1;
}

static int isBreakableNodeType(enum STX_NodeType type)
{
    switch (type)
    {
        case STX_CASE:
        case STX_LOOP_STATEMENT:
            return 1;
        break;
        default:
        break;
    }
    return 0;
}


static int checkBreakStatement(struct SemanticContext *context)
{
    const struct STX_NodeAttribute *attr;
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    int remainingLevels = 1;

    if (!assertNodeType(context, STX_BREAK)) return 0;
    attr = STX_getNodeAttribute(getCurrentNode(context));
    remainingLevels = attr->breakContinueAttributes.levels;

    while (node->parentIndex >= 0)
    {
        if (isBreakableNodeType(node->nodeType))
        {
            remainingLevels--;
        }

        if (!remainingLevels)
        {
            if (node->nodeType == STX_LOOP_STATEMENT)
            {
                struct STX_NodeAttribute *attr = STX_getNodeAttribute(node);
                attr->loopAttributes.hasBreak = 1;
            }
            return 1;
        }
        node = &node->belongsTo->nodes[node->parentIndex];
    }
    ERR_raiseError(E_SMC_BREAK_IS_NOT_IN_LOOP_OR_CASE_BLOCK);
    return 0;
}

static int checkStatement(struct SemanticContext *context)
{
    switch (getCurrentNodeType(context))
    {
        case STX_VARDECL:
            if (!addSymbolToCurrentScope(context)) return 0;
        break;
        case STX_EXPRESSION_STATEMENT:
        case STX_ASSIGNMENT:
        case STX_RETURN_STATEMENT:
        break;
        case STX_BREAK:
            if (!checkBreakStatement(context)) return 0;
        break;
        case STX_BLOCK:
            if (!checkBlock(context)) return 0;
        break;
        case STX_IF_STATEMENT:
            if (!checkIfStatement(context)) return 0;
        break;
        case STX_LOOP_STATEMENT:
            if (!checkLoopStatement(context)) return 0;
        break;
        default:
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
            return 0;
        break;
    }
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
    if (!enterCurrentNode(context, 1)) return 0;
    {
        if (!assertNodeType(context, STX_TYPE)) return 0;
        if (!moveToNextNode(context, 1)) return 0;
        if (!checkParameterList(context, -1, -1)) return 0;
        if (!moveToNextNode(context, 1)) return 0;
        if (!checkBlock(context)) return 0;
    }
    ascendToParentScope(context);
    leaveCurrentNode(context);
    return 1;
}

static int checkOperatorFunction(struct SemanticContext *context)
{
    const struct STX_NodeAttribute *attr;

    if (!assertNodeType(context, STX_OPERATOR_FUNCTION)) return 0;
    attr = STX_getNodeAttribute(getCurrentNode(context));
    if (attr->functionAttributes.isExternal)
    {
        return 1;
    }

    descendNewScope(context);
    if (!enterCurrentNode(context, 1)) return 0;
    {
        if (!assertNodeType(context, STX_TYPE)) return 0;
        if (!moveToNextNode(context, 1)) return 0;
        if (!checkParameterList(context, 2, 2)) return 0;
        if (!moveToNextNode(context, 1)) return 0;
        if (!checkBlock(context)) return 0;
    }
    ascendToParentScope(context);
    leaveCurrentNode(context);
    return 1;
}

static int checkForPlatformDeclaration(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_FOR_PLATFORMS)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    {
        if (!assertNodeType(context, STX_PLATFORM_LIST)) return 0;
        if (!moveToNextNode(context, 1)) return 0;
        if (!assertNodeType(context, STX_DECLARATIONS)) return 0;
        if (!enterCurrentNode(context, 0))
        {
            ERR_raiseError(E_SMC_EMPTY_PLATFORM_BLOCK);
            return 0;
        }
        {
            do
            {
                if (!checkDeclaration(context)) return 0;
            }
            while (moveToNextNode(context, 0));
        }
        leaveCurrentNode(context); // STX_DECLARATIONS;
    }
    leaveCurrentNode(context); // STX_FOR_PLATFORMS;
    return 1;
}

static int checkDeclaration(struct SemanticContext *context)
{
    switch (getCurrentNodeType(context))
    {
        case STX_FUNCTION:
            if (!addSymbolToCurrentScope(context)) return 0;
            if (!checkFunction(context)) return 0;
        break;
        case STX_OPERATOR_FUNCTION:
            if (!addSymbolToCurrentScope(context)) return 0;
            if (!checkOperatorFunction(context)) return 0;
        break;
        case STX_VARDECL:
            if (!addSymbolToCurrentScope(context)) return 0;
        break;
        case STX_FOR_PLATFORMS:
            if (!checkForPlatformDeclaration(context)) return 0;
        break;
        default:
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
            return 0;
        break;
    }
    return 1;
}

static int checkModule(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_MODULE)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    do
    {
        switch (getCurrentNodeType(context))
        {
            case STX_BLOCK:
                checkBlock(context);
            break;
            default:
                if (!checkDeclaration(context)) return 0;
            break;
        }
    }
    while (moveToNextNode(context, 0));
    leaveCurrentNode(context);

    return 1;
}

int checkRootNode(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_ROOT)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!assertNodeType(context, STX_MODULE)) return 0;
    if (!checkModule(context)) return 0;

    return 1;
}

struct STX_SyntaxTreeNode *STX_getRootNode(struct STX_SyntaxTree *tree);

struct SMC_CheckerResult SMC_checkSyntaxTree(struct STX_SyntaxTree *syntaxTree)
{
    struct SemanticContext sc = {0};
    int ok;
    struct SMC_CheckerResult result;

    sc.tree = syntaxTree;
    sc.currentNode = STX_getRootNode(syntaxTree);
    descendNewScope(&sc);
    ok = checkRootNode(&sc);
    ascendToParentScope(&sc);
    dumpScopes(&sc);
    result.lastNode = sc.currentNode;
    return result;
}




