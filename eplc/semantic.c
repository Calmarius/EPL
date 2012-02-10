#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "semantic.h"
#include "syntax.h"
#include "error.h"
#include "assocarray.h"
#include "error.h"

/**
 * A struct for scope.
 *
 * Variable references in the same scope refer to the same variable.
 */
struct Scope
{
    struct Scope *parentScope; ///< parent scope where the lookup will continue.
    struct ASSOC_Array *symbols; ///< associative array storing the symbols.
    /// The node of the symbol that created the scope.
    struct STX_SyntaxTreeNode *node; ///< The node in the syntax tree
    int id; ///< Id of the scope

    struct STX_SyntaxTreeNode **usedNamespaces; ///< dynamic array storing the used namespaces.
    int usedNameSpacesAllocated; ///< entries allocated in the dynamic array.
    int usedNameSpaceCount; ///< count of entries in the dynamic array.
};

/**
 * Context struct storing everything about the semantic checking.
 */
struct SemanticContext
{
    struct STX_SyntaxTree *tree; ///< The syntax tree to be checked.
     /// The current node during the checking.
     /// Its used as target node when reporting errors
    struct STX_SyntaxTreeNode *currentNode;

    struct Scope *currentScope; ///< The current scope we are
    struct Scope *rootScope; ///< The global scope of the module.

    struct Scope **scopePointers; ///< Dynamic array storing the scope references
    int scopePointersAllocated;
    int scopeCount;

};

enum PrecedenceLevel
{
    PREC_RELATIONAL, ///< comparison operators
    PREC_ADDITIVE, ///< arithmetic add, subtract; bitwise or and xor operations; logical or and xor
    PREC_MULTIPLICATIVE, ///< arithmetic division and multiplication.
    PREC_ACCESSOR ///< record access operatior.
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

/**
 * Allocates and initializes a scope
 *
 * @param parentScope the parent of the scope.
 *
 * @return the new scope
 */
static struct Scope *allocateScope(
    struct SemanticContext *context,
    struct Scope *parentScope)
{
    struct Scope *newScope;

    if (context->scopeCount == context->scopePointersAllocated)
    {
        // extend array if needed.
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

/**
 * Creates a new scope as a child scope of the current scope, and makes it current.
 */
static void descendNewScope(struct SemanticContext *context)
{
    context->currentScope = allocateScope(context, context->currentScope);
    context->currentNode->definesScopeId = context->currentScope->id;
    context->currentScope->node = getCurrentNode(context);
}

/**
 * Makes the parents scope of the current scope the current.
 */
static void ascendToParentScope(struct SemanticContext *context)
{
    context->currentScope = context->currentScope->parentScope;
}

/**
 * Adds the name of the current node to the current scope.
 * The name is the 'name' attribute of the current node.
 */
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

/**
 * Dumps the information of one symbol.
 *
 * This is a callback function of the tree transverser.
 */
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

/**
 * Dumps symbols of the scopes to the stdout.
 */
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

/**
 * Makes the first child of the current node current.
 *
 * @param [in] needError Set true to raise E_SMC_CORRUPT_SYNTAX_TREE
 *      error if the current node don't have children.
 *
 * @return Nonzero on success, zero there are no child nodes.
 */
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

/**
 * Compares the type of the current node with the given type.
 * Raises E_SMC_CORRUPT_SYNTAX_TREE if not match.
 *
 * @param [in] type The type to check.
 *
 * @return Nonzero success.
 */
static int assertNodeType(struct SemanticContext *context, enum STX_NodeType type)
{
    if (getCurrentNodeType(context) != type)
    {
        ERR_raiseError(E_SMC_CORRUPT_SYNTAX_TREE);
        return 0;
    }
    return 1;
}

/**
 * Makes the next sibling of the current node current.
 *
 * @param [in] needError Set true to make the function raise an E_SMC_CORRUPT_SYNTAX_TREE
 *      error if there is no next sibling.
 *
 * @return Nonzero on success.
 */
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

/**
 * Makes the parrent node of the current node current.
 *
 * @return 1.
 */
static int leaveCurrentNode(struct SemanticContext *context)
{
    context->currentNode = &context->tree->nodes[context->currentNode->parentIndex];
    return 1;
}

/**
 * Adds the parameter to the current scope.
 *
 * @return Nonzero on success.
 */
static int checkParameter(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_PARAMETER)) return 0;
    return addSymbolToCurrentScope(context);
}

/**
 * Checks parameter list. Raises E_SMC_TOO_FEW_PARAMETERS error if there are too
 * few arguments. Raises E_SMC_TOO_MANY_ARGUMENTS error if there err too
 * many arguemnts.
 *
 * @param [in] minParameterCount Minimum amount of parameters.
 * @param [in] maxParameterCount Maximum amount of parameters.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks statements in a block.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks if statement.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks loop statement.
 *
 * @return Nonzero on success.
 */
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

/**
 * Returns nonzero if the statement of the given type is breakable with
 * the break statement.
 */
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

/**
 * Returns nonzero if the statement of the given type is continuable with
 * the continue statement.
 */
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

/**
 * Checks break or continue statements.
 * Assigns the node id of the statement they break or continue.
 *
 * May raise E_SMC_BREAK_IS_NOT_IN_LOOP_OR_CASE_BLOCK or
 * E_SMC_CONTINUE_IS_NOT_IN_LOOP_OR_CASE_BLOCK errors.
 *
 * @return Nonzero on success.
 */
static int checkBreakContinueStatement(struct SemanticContext *context)
{
    struct STX_NodeAttribute *currentNodeAttr;
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    int remainingLevels = 1;
    enum STX_NodeType type = getCurrentNodeType(context);
    // Check the type of the current node, accept only break and continue.
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
    remainingLevels = currentNodeAttr->breakContinueAttributes.levels; //< break and continue may break or continue multiple levels of loops.

    // Find the statement to break or continue
    // From the current node, move upward in the tree. Look for breakable or continuable statements.
    while (node->parentIndex >= 0)
    {
        if (
            ((type == STX_BREAK) && isBreakableNodeType(node->nodeType)) ||
            ((type == STX_CONTINUE) && isContinuableNodeType(node->nodeType))
        )
        {
            // Decrease the level when found one.
            remainingLevels--;
        }

        if (!remainingLevels)
        {
            // Reached the appropriate statement.
            if ((node->nodeType == STX_LOOP_STATEMENT) && (type == STX_BREAK))
            {
                // If the current statement is a break and it breaks a loop.
                // Set a flag on the loop statement whether it has a break.
                // (All loops must have a break, otherwise they would be infinite.)
                struct STX_NodeAttribute *loopAttr = STX_getNodeAttribute(node);
                loopAttr->loopAttributes.hasBreak = 1;
            }
            // Associtate the node id of the breaked or continued statement.
            currentNodeAttr->breakContinueAttributes.associatedNodeId = node->id;
            return 1;
            // success.
        }
        // Move the parent node.
        node = &node->belongsTo->nodes[node->parentIndex];
    }
    // At this point node appropriate statement found. Raise error.
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

/**
 * Checks the parts of the case block.
 *
 * @return Nonzero on success.
 */
static int checkCaseBlock(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_CASE)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!checkBlock(context)) return 0;
    leaveCurrentNode(context);
    return 1;
}

/**
 * Checks the switch statement.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks a statement.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks function delcarations.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks operator functions.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks platform context declarations.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks platform context declarations.
 *
 * @return Nonzero on success.
 */
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

/**
 * Adds node to the used namespaces of the current scope.
 *
 * @param node The namespace node of the using declaration.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks using declaration. Adds the namespace to the current scope's
 * used namespace set.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks declarations.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks the module.
 *
 * @return Nonzero on success.
 */
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

/**
 * Checks the root node. All checks start from this.
 *
 * @return Nonzero on success.
 */
static int checkRootNode(struct SemanticContext *context)
{
    if (!assertNodeType(context, STX_ROOT)) return 0;
    if (!enterCurrentNode(context, 1)) return 0;
    if (!assertNodeType(context, STX_MODULE)) return 0;
    if (!checkModule(context)) return 0;

    return 1;
}

struct STX_SyntaxTreeNode *STX_getRootNode(struct STX_SyntaxTree *tree);

/**
 * Looks up a symbol.
 *
 * @param startScopeId The id of the scope where the lookup start.
 * @param varName The name of the symbol to look up.
 * @param varNameLength The length of the name.
 * @param lookupOptions It can be a conbination of the following values:
 *      - SLO_CHECK_PARENT_SCOPES: checks the parent scopes of the target scopes.
 *      - SLO_CHECK_USED_NAMESPACES: checks the used namespaces to (using declaration).
 *      When none of them are set, you can use SLO_LOCAL_ONLY which is zero. In this case
 *      only the given scope is checked.
 *
 * @return The node that declares the symbol. Null if the symbols is not found or error happened.
 *      This function can raise E_SMC_AMBIGUOS_NAME when two of the used namespaces declare
 *      a symbol of the given name.
 *
 */
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

        // Symbol still not found, try the parent scopes if any and flag is set.
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

/**
 * Find the symbol declaration from the given fully qualified name node.
 *
 * @param node An STX_QUALIFIED_NAME node. Which is looked up.
 *
 * @return The node that declared the symbol. Returns null if the symbol is not found
 *      in the last namespace of the path.
 *
 *      Raises E_SMC_NOT_A_NAMESPACE if one of the path
 *      elements is not a namespace. (Except the last)
 *
 *      Raises E_SMC_UNDEFINED_SYMBOL if the one of the path elements is not found.
 *      (Except the last)
 */
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

    // Go through the child nodes except the last one.
    while (STX_getNext(currentChild))
    {
        currentAttribute = STX_getNodeAttribute(currentChild);
        // Look the symbol up in the current scope. (Initial scope is the root scope.)
        currentNameSpace = lookUpSymbol(
            context,
            currentScope->id,
            currentAttribute->name,
            currentAttribute->nameLength,
            0
        );
        if (currentNameSpace)
        {
            // Namespace symbol is found.
            if (currentNameSpace->nodeType != STX_NAMESPACE)
            {
                context->currentNode = currentChild;
                ERR_raiseError(E_SMC_NOT_A_NAMESPACE);
                return 0;
            }
            // So this is a namespace, set it's scope as current.
            currentScope = context->scopePointers[currentNameSpace->definesScopeId];
            // Go to the next child.
            currentChild = STX_getNext(currentChild);
        }
        else
        {
            // Not found, error.
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

/**
 * Checks qualified name nodes in expressions. It looks the symbol up
 * and set the definer node id on its parent, the deletes the qualified name
 * node from the tree.
 *
 * This function also checks the context where the symbol is used. For example
 * if the symbol used like an operator, it checks whether it is an operator function.
 *
 * @param node The STX_QUALIFIED_NAME node to check.
 *
 * @return Nonzero on success.
 *
 * Raises E_SMC_UNDEFINED_SYMBOL if the symbol is not found.
 * Raises E_SMC_NOT_AN_OPERATOR if the symbol is not operator and used like
 *      an operator.
 */
static int checkQualifiedName(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
)
{
    struct STX_SyntaxTreeNode *parentNode;
    struct STX_SyntaxTreeNode *firstChild;
    struct STX_SyntaxTreeNode *declarationNode;
    enum STX_NodeType parentNodeType;

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
            // Either symbol is not found, or error happened. If error happened,
            // report error.
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
        // Set the id of the definer symbol on the parent.
        struct STX_NodeAttribute *parentNodeAttr = STX_getNodeAttribute(parentNode);
        parentNodeAttr->symbolDefinitionNodeId = declarationNode->id;
    }
    else
    {
        // Symbol not defined, error.
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
    // Remove the qualified name node from the tree.
    STX_removeNode(context->tree, node);

    return 1;
}

/**
 * WIP!
 */
static int checkTerm(
    struct SemanticContext *context,
    struct STX_SyntaxTreeNode *node
)
{
    return 1;
}

/**
 * Pushes data on a stack. It may enlarge the array provided
 * as stack. So any pointers pointing to the elements may be invalidated.
 * The stack pointer is increased, and size may be doubled.
 *
 * @param [in,out] stack An array of void* the stack itself.
 * @param [in] data The data to push on the stack.
 * @param [in,out] ptr The stack pointer.
 * @param [in,out] size The current size of the stack.
 */
static void push(void **stack, void *data, int *ptr, int *size)
{
    if (*ptr == *size)
    {
        *size *= 2;
        stack = realloc(stack, *size * sizeof(void*));
    }
    stack[(*ptr)++] = data;
}

/**
 * Pops data from a stack.
 *
 * @param [in] stack The stack (an array of void*)
 * @param [in,out] ptr The current stack pointer. Decreased by one.
 *
 * @return The topmost element that was on the top of the stack.
 *      It returns null if the stack is empty.
 */
static void *pop(void **stack, int *ptr)
{
    if (!*ptr) return 0;
    return stack[--(*ptr)];
}

/**
 * Returns the topmost element on a stack.
 *
 * @param [in] stack The stack (an array of void*)
 * @param [in] ptr The stack pointer.
 *
 * @return The topmost element on the stack.
 */
static void *peek(void **stack, int ptr)
{
    if (!ptr) return 0;
    return stack[ptr - 1];
}
/**
 * Returns the precedence level of an operator node (from an expression)
 *
 * @return The precedence level.
 */
static enum PrecedenceLevel getPrecedenceLevel(struct STX_SyntaxTreeNode *operatorNode)
{
    struct STX_NodeAttribute *attr;

    assert(operatorNode);
    attr = STX_getNodeAttribute(operatorNode);
    if (STX_getFirstChild(operatorNode))
    {
        // This is a composite operator.
        switch (attr->functionAttributes.precedence)
        {
            case LEX_KW_ADDITIVE: return PREC_ADDITIVE;
            case LEX_KW_MULTIPLICATIVE: return PREC_MULTIPLICATIVE;
            case LEX_KW_RELATIONAL: return PREC_RELATIONAL;
            default:
                assert(0);
            break;
        }
    }
    else
    {
        // This is a simple operator
        switch (attr->operatorAttributes.type)
        {
            case LEX_ADD_OPERATOR:
            case LEX_SUBTRACT_OPERATOR:
                return PREC_ADDITIVE;
            case LEX_SHIFT_LEFT:
            case LEX_SHIFT_RIGHT:
            case LEX_DIVISION_OPERATOR:
            case LEX_MULTIPLY_OPERATOR:
                return PREC_MULTIPLICATIVE;
            case LEX_LESS_EQUAL_THAN:
            case LEX_LESS_THAN:
            case LEX_GREATER_EQUAL_THAN:
            case LEX_GREATER_THAN:
            case LEX_NOT_EQUAL:
            case LEX_EQUAL:
                return PREC_RELATIONAL;
            case LEX_PERIOD:
                return PREC_ACCESSOR;
            default:
                assert(0);
            break;
        }
    }
}

/**
 * Performs shunting yard algorithm on az expression node.
 * Reorders the nodes according to the precedence. It does not
 * reorders subexpressions in terms.
 *
 * @param [in,out] tree The syntax tree the node is in.
 * @param [in,out] expressionNode the root node of the expression.
 */
static void performShuntingYardAlgorithm(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *expressionNode)
{
    struct STX_SyntaxTreeNode **operatorStack;
    int operatorPtr = 0;
    int operatorStackSize = 20;

    struct STX_SyntaxTreeNode **result;
    int resultPtr = 0;
    int resultSize = 20;

    struct STX_SyntaxTreeNode *currentNode;

    operatorStack = malloc(operatorStackSize * sizeof(struct STX_SyntaxTreeNode *));
    result = malloc(resultSize * sizeof(struct STX_SyntaxTreeNode *));

    // Go through the nodes and pushes them to the result stack in postfix notation.
    for
    (
        currentNode = STX_getFirstChild(expressionNode);
        currentNode;
        currentNode = STX_getNext(currentNode)
    )
    {
        enum STX_NodeType type = currentNode->nodeType;
        switch (type)
        {
            case STX_TERM:
                // Terms are pushed to the result immediately.
                push((void**)result, currentNode, &resultPtr, &resultSize);
            break;
            case STX_OPERATOR:
            {
                // if operator is encountered. All operators on the operator
                // stack with stonger or equal precedence are pushed to the result.
                // (Popping operators with equal precedence means all operators
                // are left associative. )
                for(;;)
                {
                    struct STX_SyntaxTreeNode *top = peek((void**)operatorStack, operatorPtr);
                    if (top)
                    {
                        if (getPrecedenceLevel(currentNode) <= getPrecedenceLevel(top))
                        {
                            push((void**)result, top, &resultPtr, &resultSize);
                            pop((void**)operatorStack, &operatorPtr);
                        }
                        else
                            break;
                    }
                    else
                    {
                        break;
                    }
                }
                // Finally push  the operator on the operator stack.
                push((void**)operatorStack, currentNode, &operatorPtr, &operatorStackSize);
            }
            break;
            default:
            break;
        }
    }
    // Flush remaining operator from the operand stack.
    for(;;)
    {
        struct STX_SyntaxTreeNode *top = peek((void**)operatorStack, operatorPtr);
        if (top)
        {
            push((void**)result, top, &resultPtr, &resultSize);
            pop((void**)operatorStack, &operatorPtr);
        }
        else
        {
            break;
        }
    }
    // Rebuild the expression node tree.
    STX_removeAllChildren(expressionNode);
    {
        int i;
        for (i = 0; i < resultPtr; i++)
        {
            struct STX_SyntaxTreeNode *current = result[i];

            switch (current->nodeType)
            {
                case STX_TERM:
                    // Term nodes are just appended.
                    STX_appendChild(tree, expressionNode, current);
                break;
                case STX_OPERATOR:
                {
                    struct STX_SyntaxTreeNode *operand2 = STX_getLastChild(expressionNode);
                    struct STX_SyntaxTreeNode *operand1 = STX_getPrevious(operand2);
                    // Append operators too, but remove two nodes before it and append them
                    // as children of the operator node.
                    STX_appendChild(tree, expressionNode, current);
                    STX_removeNode(tree, operand1);
                    STX_removeNode(tree, operand2);
                    STX_appendChild(tree, current, operand1);
                    STX_appendChild(tree, current, operand2);
                }
                break;
                default:
                    // This should never happen.
                    assert(0);
                break;
            }
        }
    }

    free(operatorStack);
    free(result);
}

static int checkTypeOfTerm(struct SemanticContext *context, struct STX_SyntaxTreeNode *current)
{
    struct STX_NodeAttribute *attr = STX_getNodeAttribute(current);
    switch (attr->termAttributes.termType)
    {
        case STX_TT_SIMPLE:
        {
            struct STX_TypeInformation *typeInfo = &attr->typeInformation;
            switch (attr->termAttributes.tokenType)
            {
                case LEX_OCTAL_NUMBER:
                case LEX_HEXA_NUMBER:
                case LEX_DECIMAL_NUMBER:
                    typeInfo->metaType = STX_TYT_SIMPLE;
                    typeInfo->bitCount = 0;
                    typeInfo->attribs = "";
                    typeInfo->attribLength = 0;
                    assert(attr->nameLength);
                    if (attr->name[0] == '-')
                    {
                        typeInfo->type = STX_STT_SIGNED_INT;
                    }
                    else
                    {
                        typeInfo->type = STX_STT_UNSIGNED_INT;
                    }
                break;
                case LEX_FLOAT_NUMBER:
                    typeInfo->metaType = STX_TYT_SIMPLE;
                    typeInfo->bitCount = 0;
                    typeInfo->attribs = "";
                    typeInfo->attribLength = 0;
                    assert(attr->nameLength);
                    typeInfo->type = STX_STT_FLOAT;
                break;
                default:
                break;

            }
        }
        break;
        default:
        break;
    }
    return 1;
}

static int checkExpression(struct SemanticContext *context, struct STX_SyntaxTreeNode *exprNode)
{
    struct STX_TreeIterator iterator;
    struct STX_SyntaxTreeNode *current;

    STX_initializeTreeIterator(&iterator, exprNode);
    // First identify symbol, and organize the tree.
    for(
        current = STX_getNextPostorder(&iterator);
        current;
    )
    {
        switch (current->nodeType)
        {
            case STX_QUALIFIED_NAME:
            {
                struct STX_SyntaxTreeNode *tmp = STX_getNextPostorder(&iterator);
                if (!checkQualifiedName(context, current)) return 0;
                current = tmp;
                continue;
            }
            break;
            case STX_TERM:
                if (!checkTerm(context, current)) return 0;
            break;
            case STX_EXPRESSION:
                performShuntingYardAlgorithm(context->tree, current);
            break;
            default:
            break;
        }
        current = STX_getNextPostorder(&iterator);
    }
    // Do another transversal to assign and check the types
    STX_initializeTreeIterator(&iterator, exprNode);
    for(
        current = STX_getNextPostorder(&iterator);
        current;
        current = STX_getNextPostorder(&iterator)
    )
    {
        switch (current->nodeType)
        {
            case STX_TERM:
                if (!checkTypeOfTerm(context, current)) return 0;
            break;
            default:
            break;
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




