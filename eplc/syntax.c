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
    node->belongsTo = tree;
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
    memset(attribute, 0, sizeof(*attribute));
    attribute->allocated = 1;
    attribute->id = tree->attributeCount;
    attribute->belongsTo = tree;
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

static int parseStatement(struct SyntaxContext *context)
{
    descendNewNode(context, STX_STATEMENT);
    ascendToParent(context);
    return 1;
}

/**
 * <Block> ::=
 * '{' <Statement>* '}'
 */
static int parseBlock(struct SyntaxContext *context)
{
    descendNewNode(context, STX_BLOCK);
    if (!expect(context, LEX_LEFT_BRACE))
    {
        ERR_raiseError(E_STX_LEFT_BRACE_EXPECTED);
        return 0;
    }
    while (getCurrent(context)->tokenType != LEX_RIGHT_BRACE)
    {
        if (!parseStatement(context)) return 0;
    }

    if (!expect(context, LEX_RIGHT_BRACE))
    {
        ERR_raiseError(E_STX_RIGHT_BRACE_EXPECTED);
        return 0;
    }

    ascendToParent(context);
    return 1;
}

static int isIntegerNumberToken(enum LEX_TokenType tokenType)
{
    return
        (tokenType == LEX_DECIMAL_NUMBER) ||
        (tokenType == LEX_OCTAL_NUMBER) ||
        (tokenType == LEX_HEXA_NUMBER);
}

static int getIntegerValue(const struct LEX_LexerToken *token)
{
    const char *current = token->start;
    const char *end = token->start + token->length;
    int value = 0;
    int radix = 0;

    switch (token->tokenType)
    {
        case LEX_DECIMAL_NUMBER:
            radix = 10;
        break;
        case LEX_OCTAL_NUMBER:
            radix = 8;
        break;
        case LEX_HEXA_NUMBER:
            current += 2; // skip 0x
            radix = 16;
        break;
        default:
            assert(0);
    }

    while (current < end)
    {
        char c = *current;
        int digit;
        if (('0' <= c) && (c <= '9')) digit = c - '0';
        else if (radix > 10)
        {
            digit = (c | 0x20) - 'a' + 10;
        }
        if (digit < 0) break;
        if (digit >= radix) break;
        value *= radix;
        value += digit;
        current++;
    }

    return value;

}

/**
 *   TypePrefix ::=
 *       (
 *          (
 *              'handle' |
 *              'buffer' '[' integer_number ']'
 *          ) 'of'
 *      ) |
 *      (
 *         ('pointer' | 'localptr') 'to'
 *      )
 *
 */
static int parseTypePrefix(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_TYPE_PREFIX);
    token = getCurrent(context);

    if (token->tokenType == LEX_KW_HANDLE)
    {

        acceptCurrent(context);
        if (!expect(context, LEX_KW_OF))
        {
            ERR_raiseError(E_STX_OF_EXPECTED);
            return 0;
        }
        attribute = allocateAttribute(context->tree);
        attribute->typePrefixAttributes.type = STX_TP_HANDLE;
        context->currentNode->attributeIndex = attribute->id;
    }
    else if (token->tokenType == LEX_KW_BUFFER)
    {
        int elements = 0;

        acceptCurrent(context);
        if (!expect(context, LEX_LEFT_BRACKET))
        {
            ERR_raiseError(E_STX_LEFT_BRACKET_EXPECTED);
            return 0;
        }
        token = getCurrent(context);
        if (isIntegerNumberToken(token->tokenType))
        {
            elements = getIntegerValue(token);
            acceptCurrent(context);
        }
        else
        {
            ERR_raiseError(E_STX_INTEGER_NUMBER_EXPECTED);
            return 0;
        }
        if (!expect(context, LEX_RIGHT_BRACKET))
        {
            ERR_raiseError(E_STX_RIGHT_BRACKET_EXPECTED);
            return 0;
        }
        if (!expect(context, LEX_KW_OF))
        {
            ERR_raiseError(E_STX_OF_EXPECTED);
            return 0;
        }
        attribute = allocateAttribute(context->tree);
        attribute->typePrefixAttributes.type = STX_TP_BUFFER;
        attribute->typePrefixAttributes.elements = elements;
        context->currentNode->attributeIndex = attribute->id;
    }
    else if (
        (token->tokenType == LEX_KW_POINTER) ||
        (token->tokenType == LEX_KW_LOCALPTR))
    {
        acceptCurrent(context);
        if (!expect(context, LEX_KW_TO))
        {
            ERR_raiseError(E_STX_TO_EXPECTED);
            return 0;
        }
        attribute = allocateAttribute(context->tree);
        switch (token->tokenType)
        {
            case LEX_KW_POINTER:
                attribute->typePrefixAttributes.type = STX_TP_POINTER;
            break;
            case LEX_KW_LOCALPTR:
                attribute->typePrefixAttributes.type = STX_TP_LOCALPTR;
            break;
            default:
                assert(0); //< something is really screwed up.
        }
        context->currentNode->attributeIndex = attribute->id;
    }

    ascendToParent(context);
    return 1;

}

/**
 * Type ::=
 *     (
 *          <TypePrefix> <Type>
 *      ) |
 *     built_in_type |
 *    identifier
 */
static int parseType(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;

    descendNewNode(context, STX_TYPE);
    token = getCurrent(context);

    if ((token->tokenType == LEX_BUILT_IN_TYPE) || (token->tokenType == LEX_IDENTIFIER))
    {
        struct STX_NodeAttribute *attr = allocateAttribute(context->tree);
        attr->name = token->start;
        attr->nameLength = token->length;
        context->currentNode->attributeIndex = attr->id;
        acceptCurrent(context);
    }
    else
    {
        if (!parseTypePrefix(context)) return 0;
        if (!parseType(context)) return 0;
    }

    ascendToParent(context);
    return 1;
}

/**
 * VariableDeclaration ::= 'vardecl' <Type> identifier
 */
static int parseVariableDeclaration(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;

    descendNewNode(context, STX_VARDECL);
    if (!expect(context, LEX_KW_VARDECL))
    {
        ERR_raiseError(E_STX_VARDECL_EXPECTED);
        return 0;
    }
    if (!parseType(context )) return 0;
    current = getCurrent(context);
    if (current->tokenType == LEX_IDENTIFIER)
    {
        struct STX_NodeAttribute *attr = allocateAttribute(context->tree);
        attr->name = current->start;
        attr->nameLength = current->length;
        context->currentNode->attributeIndex = attr->id;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }

    if (!expect(context, LEX_SEMICOLON))
    {
        ERR_raiseError(E_STX_SEMICOLON_EXPECTED);
        return 0;
    }


    ascendToParent(context);
    return 1;
}

static enum STX_ParameterDirection mapTokenTypeToDirection(enum LEX_TokenType type)
{
    switch (type)
    {
        case LEX_KW_IN: return STX_PD_IN;
        case LEX_KW_OUT: return STX_PD_OUT;
        case LEX_KW_REF: return STX_PD_REF;
        default:
            assert(0);
    }
    assert(0);
    return 0;
}

/**
 * <Parameter> ::=
 * ( 'in' | 'out' | 'ref') <Type> identifier
 */
static int parseParameter(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token = getCurrent(context);
    struct STX_NodeAttribute *attribute = allocateAttribute(context->tree);

    descendNewNode(context, STX_PARAMETER);

    if (
        (token->tokenType != LEX_KW_IN) &&
        (token->tokenType != LEX_KW_OUT) &&
        (token->tokenType != LEX_KW_REF))
    {
        ERR_raiseError(E_STX_PARAMETER_DIRECTION_EXPECTED);
        return 0;
    }
    attribute->parameterAttributes.direction = mapTokenTypeToDirection(token->tokenType);
    acceptCurrent(context);
    if (!parseType(context)) return 0;
    token = getCurrent(context);
    if (token->tokenType != LEX_IDENTIFIER)
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    attribute->name = token->start;
    attribute->nameLength = token->length;
    acceptCurrent(context);

    context->currentNode->attributeIndex = attribute->id;
    ascendToParent(context);
    return 1;
}

/**
 * <ArgumentList> ::=
 * (<Parameter> (',' <Parameter>)*)?
 */
static int parseArgumentList(struct SyntaxContext *context)
{
    descendNewNode(context, STX_ARGUMENT_LIST);
    if (!expect(context, LEX_LEFT_PARENTHESIS))
    {
        ERR_raiseError(E_STX_LEFT_PARENTHESIS_EXPECTED);
        return 0;
    }
    if (getCurrent(context)->tokenType != LEX_RIGHT_PARENTHESIS)
    {
        if (!parseParameter(context)) return 0;
        while (getCurrent(context)->tokenType != LEX_RIGHT_PARENTHESIS)
        {
            if (!expect(context, LEX_COMMA))
            {
                ERR_raiseError(E_STX_COMMA_EXPECTED);
                return 0;
            }
            if (!parseParameter(context)) return 0;
        }
    }

    if (!expect(context, LEX_RIGHT_PARENTHESIS))
    {
        ERR_raiseError(E_STX_RIGHT_PARENTHESIS_EXPECTED);
        return 0;
    }
    ascendToParent(context);
    return 1;
}

/**
 * <FunctionDeclaration> ::=
 * 'function' <Type> '(' <ArgumentList> ')' <Block>
 */
static int parseFunctionDeclaration(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attribute;
    const struct LEX_LexerToken *token;

    descendNewNode(context, STX_FUNCTION);
    attribute = allocateAttribute(context->tree);
    if (!expect(context, LEX_KW_FUNCTION))
    {
        ERR_raiseError(E_STX_FUNCTION_EXPECTED);
        return 0;
    }
    if (!parseType(context)) return 0;
    token = getCurrent(context);
    if (token->tokenType != LEX_IDENTIFIER)
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    attribute->name = token->start;
    attribute->nameLength = token->length;
    acceptCurrent(context);
    if (!parseArgumentList(context)) return 0;
    if (!parseBlock(context)) return 0;

    context->currentNode->attributeIndex = attribute->id;
    ascendToParent(context);
    return 1;
}

/**
 * Declarations ::=
 * (<VariableDeclaration> | <FunctionDeclaration>)*
 */
static int parseDeclarations(struct SyntaxContext *context)
{
    int declarationFound = 0;

    descendNewNode(context, STX_DECLARATIONS);
    do
    {
        const struct LEX_LexerToken *current = getCurrent(context);
        declarationFound = 0;
        switch (current->tokenType)
        {
            case LEX_KW_VARDECL:
                declarationFound = 1;
                if (!parseVariableDeclaration(context)) return 0;
            break;
            case LEX_KW_FUNCTION:
                declarationFound = 1;
                if (!parseFunctionDeclaration(context)) return 0;
            break;
            default:
            break;
        }
    }
    while (declarationFound);
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

