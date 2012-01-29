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

    struct STX_SyntaxTreeNode **usedNamespaces;
    int usedNameSpacesAllocated;
    int usedNameSpaceCount;
};

struct SemanticContext
{
    struct STX_SyntaxTree *tree;
    struct STX_SyntaxTreeNode *currentNode;

    struct Scope *currentScope;
    struct Scope *rootScope;

    struct Scope **scopePointers;
    int scopePointersAllocated;
    int scopeCount;

    int currentLevel;
    int currentScopeId;
};

#define SLO_LOCAL_ONLY 0
#define SLO_CHECK_PARENT_SCOPES 1
#define SLO_CHECK_USED_NAMESPACES 2

static int checkStatement(struct SemanticContext *context);
static int checkDeclaration(struct SemanticContext *context);
static struct STX_SyntaxTreeNode *findSymbolDeclarationFromFullyQualifiedName(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
);


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
    context->scopePointers[context->scopeCount] = newScope;
    newScope->parentScope = parentScope;
    newScope->id = context->scopeCount;
    newScope->symbols = malloc(sizeof(*newScope->symbols));
    context->scopeCount++;
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
    context->currentNode->definesScopeId = context->currentScope->id;
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
    context->currentNode->inScopeId = context->currentScope->id;

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
    context->currentNode->inScopeId = context->currentScope->id;
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
    descendNewScope(context);
    if (!assertNodeType(context, STX_BLOCK)) return 0;
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

static int isContinuableNodeType(enum STX_NodeType type)
{
    switch (type)
    {
        case STX_LOOP_STATEMENT:
            return 1;
        break;
        default:
        break;
    }
    return 0;
}


static int checkBreakContinueStatement(struct SemanticContext *context)
{
    struct STX_NodeAttribute *currentNodeAttr;
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    int remainingLevels = 1;
    enum STX_NodeType type = getCurrentNodeType(context);

    switch (type)
    {
        case STX_BREAK:
        case STX_CONTINUE:
        break;
        default:
            ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
            return 0;
    }
    currentNodeAttr = STX_getNodeAttribute(getCurrentNode(context));
    remainingLevels = currentNodeAttr->breakContinueAttributes.levels;

    while (node->parentIndex >= 0)
    {
        if (
            ((type == STX_BREAK) && isBreakableNodeType(node->nodeType)) ||
            ((type == STX_CONTINUE) && isContinuableNodeType(node->nodeType))
        )
        {
            remainingLevels--;
        }

        if (!remainingLevels)
        {
            if (node->nodeType == STX_LOOP_STATEMENT)
            {
                struct STX_NodeAttribute *loopAttr = STX_getNodeAttribute(node);
                loopAttr->loopAttributes.hasBreak = 1;
            }
            currentNodeAttr->breakContinueAttributes.associatedNodeId = node->id;
            return 1;
        }
        node = &node->belongsTo->nodes[node->parentIndex];
    }
    switch (type)
    {
        case STX_BREAK:
            ERR_raiseError(E_SMC_BREAK_IS_NOT_IN_LOOP_OR_CASE_BLOCK);
        break;
        case STX_CONTINUE:
            ERR_raiseError(E_SMC_CONTINUE_IS_NOT_IN_LOOP_OR_CASE_BLOCK);
        break;
        default:
        break;
    }
    return 0;
}

static int checkCaseBlock(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_CASE)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!checkBlock(context)) return 0;
    leaveCurrentNode(context);
    return 1;
}

static int checkSwitchStatement(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_SWITCH)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!assertNodeType(context, STX_EXPRESSION)) return 0;
    while (moveToNextNode(context, 0))
    {
        if (!checkCaseBlock(context)) return 0;
    }
    leaveCurrentNode(context);
    return 1;
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
        case STX_SWITCH:
            if (!checkSwitchStatement(context)) return 0;
        break;
        case STX_BREAK:
        case STX_CONTINUE:
            if (!checkBreakContinueStatement(context)) return 0;
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

static int checkNamespace(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_NAMESPACE)) return 0;
    descendNewScope(context);
    if (!enterCurrentNode(context, 1)) return 1;
    {
        do
        {
            if (!checkDeclaration(context)) return 0;
        }
        while (moveToNextNode(context, 0));
    }
    ascendToParentScope(context);
    leaveCurrentNode(context);
    return 1;
}

static int addNodeToUsedNamespaces(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
)
{
    struct Scope *scope = context->currentScope;

    if (node->nodeType != STX_NAMESPACE)
    {
        ERR_raiseError(E_SMC_NOT_A_NAMESPACE);
        return 0;
    }

    if (scope->usedNameSpacesAllocated == scope->usedNameSpaceCount)
    {
        if (!scope->usedNameSpacesAllocated)
        {
            scope->usedNameSpacesAllocated = 5;
        }
        else
        {
            scope->usedNameSpacesAllocated <<= 1;
        }

        scope->usedNamespaces = realloc(
            scope->usedNamespaces,
            scope->usedNameSpacesAllocated * sizeof(*scope)
        );
    }

    scope->usedNamespaces[scope->usedNameSpaceCount++] = node;

    return 1;
}

static int checkUsingDeclaration(struct SemanticContext *context)
{
    struct STX_SyntaxTreeNode *declarationNode;

    if (!assertNodeType(context, STX_USING)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;

    declarationNode = findSymbolDeclarationFromFullyQualifiedName(
        context,
        context->currentNode
    );

    if (!declarationNode)
    {
        if (ERR_isError()) return 0;
        ERR_raiseError(E_SMC_UNDEFINED_SYMBOL);
        return 0;
    }

    if (!addNodeToUsedNamespaces(context, declarationNode)) return 0;

    leaveCurrentNode(context);
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
        case STX_NAMESPACE:
            if (!addSymbolToCurrentScope(context)) return 0;
            if (!checkNamespace(context)) return 0;
        break;
        case STX_USING:
            if (!checkUsingDeclaration(context)) return 0;
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
                if (!checkBlock(context)) return 0;
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

static int checkRootNode(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_ROOT)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!assertNodeType(context, STX_MODULE)) return 0;
    if (!checkModule(context)) return 0;

    return 1;
}

struct STX_SyntaxTreeNode *STX_getRootNode(struct STX_SyntaxTree *tree);

static struct STX_SyntaxTreeNode *lookUpSymbol(
    struct SemanticContext *context,
    int startScopeId,
    const char *varName,
    int varNameLength,
    int lookupOptions
)
{
    struct Scope *scope;
    struct STX_SyntaxTreeNode *node;

    assert(startScopeId >= 0);
    scope = context->scopePointers[startScopeId];
    node = ASSOC_find(scope->symbols, varName, varNameLength);
    if (node)
    {
        // symbol found, return it.
        return node;
    }
    else
    {
        // symbol not found check used namespaces
        int i;
        int foundCount = 0;
        struct STX_SyntaxTreeNode *foundNode = 0;
        if (lookupOptions & SLO_CHECK_USED_NAMESPACES)
        {
            for (i = 0; i < scope->usedNameSpaceCount; i++ )
            {
                // Check all used namespaces for ambiguity.
                struct STX_SyntaxTreeNode *namespaceNode = scope->usedNamespaces[i];
                foundNode = lookUpSymbol(
                    context,
                    namespaceNode->definesScopeId,
                    varName,
                    varNameLength,
                    0
                );
                foundCount++;
                if (foundCount > 1)
                {
                    ERR_raiseError(E_SMC_AMBIGUOS_NAME);
                    return 0;
                }
            }
        }
        if (foundNode)
        {
            return foundNode;
        }

        // try the parent scope if any and flag is set.
        if (scope->parentScope && (lookupOptions & SLO_CHECK_PARENT_SCOPES))
        {
            return lookUpSymbol(
                context,
                scope->parentScope->id,
                varName,
                varNameLength,
                lookupOptions
            );
        }
    }
    return 0;
}

static struct STX_SyntaxTreeNode *findSymbolDeclarationFromFullyQualifiedName(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
)
{
    struct STX_SyntaxTreeNode *currentChild;
    struct Scope *currentScope = context->rootScope;
    struct STX_SyntaxTreeNode *currentNameSpace = 0;
    struct STX_SyntaxTreeNode *declarationNode = 0;
    struct STX_NodeAttribute *currentAttribute;

    assert(node);
    if (node->nodeType != STX_QUALIFIED_NAME)
    {
        ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        return 0;
    }

    currentChild = STX_getFirstChild(node);

    while (STX_getNext(currentChild))
    {
        currentAttribute = STX_getNodeAttribute(currentChild);
        currentNameSpace = lookUpSymbol(
            context,
            currentScope->id,
            currentAttribute->name,
            currentAttribute->nameLength,
            0
        );
        if (currentNameSpace)
        {
            if (currentNameSpace->nodeType != STX_NAMESPACE)
            {
                context->currentNode = currentChild;
                ERR_raiseError(E_SMC_NOT_A_NAMESPACE);
                return 0;
            }
            // So this is a namespace set it's scope as current.
            currentScope = context->scopePointers[currentNameSpace->definesScopeId];
            // Go to the next child.
            currentChild = STX_getNext(currentChild);
        }
        else
        {
            context->currentNode = currentChild;
            ERR_raiseError(E_SMC_UNDEFINED_SYMBOL);
            return 0;
        }
    }
    // currentChild is the last child.
    // Look it up.
    currentAttribute = STX_getNodeAttribute(currentChild);
    declarationNode = lookUpSymbol(
        context,
        currentScope->id,
        currentAttribute->name,
        currentAttribute->nameLength,
        0
    );
    return declarationNode;
}

static int checkQualifiedName(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
)
{
    struct STX_SyntaxTreeNode *parentNode;
    struct STX_SyntaxTreeNode *firstChild;
    struct STX_SyntaxTreeNode *declarationNode;
    enum STX_NodeType parentNodeType;
    struct STX_NodeAttribute *nodeAttr = STX_getNodeAttribute(node);

    if (node->nodeType != STX_QUALIFIED_NAME) return 1;
    parentNode = STX_getParentNode(node);
    if (!parentNode) return 1;
    parentNodeType = parentNode->nodeType;
    if (
        (parentNodeType != STX_TERM) &&
        (parentNodeType != STX_OPERATOR)
    )
    {
        return 1;
    }
    firstChild = STX_getFirstChild(node);
    if (STX_getNext(firstChild))
    {
        // This is a fully qualified name.
        declarationNode = findSymbolDeclarationFromFullyQualifiedName(context, node);
    }
    else
    {
        // This is not a qualified name.
        struct STX_NodeAttribute *firstChildAttr = STX_getNodeAttribute(firstChild);

        declarationNode = lookUpSymbol(
            context,
            node->inScopeId,
            firstChildAttr->name,
            firstChildAttr->nameLength,
            SLO_CHECK_PARENT_SCOPES | SLO_CHECK_USED_NAMESPACES
        );

        if (!declarationNode)
        {
            // Either symbol is not found, or error happened.
            if (ERR_isError())
            {
                context->currentNode = node;
                return 0;
            }
        }
    }
    // Check if the symbol found.
    if (declarationNode)
    {
        nodeAttr->symbolDefinitionNodeId = declarationNode->id;
    }
    else
    {
        context->currentNode = node;
        ERR_raiseError(E_SMC_UNDEFINED_SYMBOL);
        return 0;
    }
    if (parentNodeType == STX_OPERATOR)
    {
        // Check whether the symbol is really an operator.
        if (declarationNode->nodeType != STX_OPERATOR_FUNCTION)
        {
            context->currentNode = node;
            ERR_raiseError(E_SMC_NOT_AN_OPERATOR);
            return 0;
        }
    }

    return 1;
}

static int checkExpression(struct SemanticContext *context, struct STX_SyntaxTreeNode *current)
{
    struct STX_TreeIterator iterator;

    STX_initializeTreeIterator(&iterator, current);
    for(
        current = STX_getNextPostorder(&iterator);
        current;
        current = STX_getNextPostorder(&iterator)
    )
    {
        if (current->nodeType == STX_QUALIFIED_NAME)
        {
            if (!checkQualifiedName(context, current)) return 0;
        }
    }
    return 1;
}

static int checkExpressions(struct SemanticContext *context)
{
    struct STX_TreeIterator iterator;
    struct STX_SyntaxTreeNode *current = STX_getRootNode(context->tree);
    for (
        STX_initializeTreeIterator(&iterator, current);
        current;
        current = STX_getNextPreorder(&iterator)
    )
    {
        switch (current->nodeType)
        {
            case STX_EXPRESSION:
                STX_setSkipSubtree(&iterator, 1);
                if (!checkExpression(context, current)) return 0;
            break;
            default:
            break;
        }
    }

    return 1;
}

static void setScopeIdsOnAllNodes(struct SemanticContext *context)
{
    struct STX_TreeIterator iterator;
    struct STX_SyntaxTreeNode *current = STX_getRootNode(context->tree);
    STX_initializeTreeIterator(&iterator, current);

    for (
        current = STX_getNextPreorder(&iterator);
        current;
        current = STX_getNextPreorder(&iterator)
    )
    {
        struct STX_SyntaxTreeNode *parent = STX_getParentNode(current);
        if (parent)
        {
            if (current->inScopeId < 0)
            {
                current->inScopeId = parent->inScopeId;
            }
        }
    }
}

struct SMC_CheckerResult SMC_checkSyntaxTree(struct STX_SyntaxTree *syntaxTree)
{
    struct SemanticContext sc = {0};
    int ok;
    struct SMC_CheckerResult result;

    sc.tree = syntaxTree;
    sc.currentNode = STX_getRootNode(syntaxTree);
    descendNewScope(&sc);
    sc.rootScope = sc.currentScope;
    ok = checkRootNode(&sc);
    ascendToParentScope(&sc);
    setScopeIdsOnAllNodes(&sc);
    if (ok) ok = checkExpressions(&sc);
    dumpScopes(&sc);
    result.lastNode = sc.currentNode;
    return result;
}




