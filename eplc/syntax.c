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
    int currentNodeIndex;
    int currentAttributeIndex;

    const struct LEX_LexerToken *latestComment;
};

static struct STX_SyntaxTreeNode *allocateNode(struct STX_SyntaxTree *tree);
static void initializeNode(struct STX_SyntaxTreeNode *node);

static int preorderStep(
    struct STX_SyntaxTree *tree,
    int level,
    struct STX_SyntaxTreeNode *node,
    STX_TransverseCallback callback,
    void *userData)
{
    int index;
    if (!callback(node, level, userData)) return 0;
    index = node->firstChildIndex;
    while (index >= 0)
    {
        struct STX_SyntaxTreeNode *current = &tree->nodes[index];
        if (!preorderStep(tree, level+1, current, callback, userData)) return 0;
        index = current->nextSiblingIndex;
    }
    return 1;
}

int STX_transversePreorder(struct STX_SyntaxTree *tree, STX_TransverseCallback callback, void *userData)
{
    struct STX_SyntaxTreeNode *node;
    node = &tree->nodes[tree->rootNodeIndex];
    return preorderStep(tree, 0, node, callback, userData);
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
    child->nextSiblingIndex = -1;
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

static int allocateAttribute(struct STX_SyntaxTree *tree)
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
    return attribute->id;
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

static const struct LEX_LexerToken *getCurrentToken(struct SyntaxContext *context)
{
    return context->current;
}

static enum LEX_TokenType getCurrentTokenType(struct SyntaxContext *context)
{
    if (!context->current)
    {
        return LEX_SPEC_EOF;
    }
    return context->current->tokenType;
}

static int isCommentTokenType(enum LEX_TokenType tokenType)
{
    switch (tokenType)
    {
        case LEX_EOL_COMMENT:
        case LEX_BLOCK_COMMENT:
        case LEX_DOCUMENTATION_BLOCK_COMMENT:
        case LEX_DOCUMENTATION_EOL_BACK_COMMENT:
        case LEX_DOCUMENTATION_EOL_COMMENT:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static void descendNewNode(struct SyntaxContext *context, enum STX_NodeType type);
static void allocateAttributeForCurrent(struct SyntaxContext *context);
static struct STX_NodeAttribute *getCurrentAttribute(struct SyntaxContext *context);
static void ascendToParent(struct SyntaxContext *context);

static void advance(struct SyntaxContext *context)
{
    assert(context->current);
    if (context->tokensRemaining)
    {
        assert(context->tokensRemaining);
        context->tokensRemaining--;
        context->current++;
    }
    if (!context->tokensRemaining)
    {
        context->current = 0;
    }
}

static int isForwardDocumentationCommentType(enum LEX_TokenType tokenType)
{
    switch (tokenType)
    {
        case LEX_DOCUMENTATION_BLOCK_COMMENT:
        case LEX_DOCUMENTATION_EOL_COMMENT:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static int isBackDocumentationCommentType(enum LEX_TokenType tokenType)
{
    switch (tokenType)
    {
        case LEX_DOCUMENTATION_EOL_BACK_COMMENT:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static struct STX_SyntaxTreeNode *getCurrentNode(struct SyntaxContext *context)
{
    return &context->tree->nodes[context->currentNodeIndex];
}

static void skipComments(struct SyntaxContext *context)
{
    while (isCommentTokenType(getCurrentTokenType(context)))
    {
        if (isForwardDocumentationCommentType(getCurrentTokenType(context)))
        {
            context->latestComment = getCurrentToken(context);
        }
        else if (isBackDocumentationCommentType(getCurrentTokenType(context)))
        {
            struct STX_SyntaxTreeNode *currentNode = getCurrentNode(context);
            struct STX_NodeAttribute *attr;
            const struct LEX_LexerToken *token = getCurrentToken(context);

            allocateAttributeForCurrent(context);
            attr = getCurrentAttribute(context);
            currentNode->attributeIndex = attr->id;
            attr->comment = token->start;
            attr->commentLength = token->length;
        }
        advance(context);
    }
}

static void acceptCurrent(struct SyntaxContext *context)
{
    advance(context);
    skipComments(context);
}

static int expect(
    struct SyntaxContext *context,
    enum LEX_TokenType type,
    enum ERR_ErrorCode errorToRaise)
{
    const struct LEX_LexerToken *token;

    token = getCurrentToken(context);

    if (!token)
    {
        ERR_raiseError(E_STX_UNEXPECTED_END_OF_FILE);
        return 0;
    }
    if (token->tokenType == type)
    {
        acceptCurrent(context);
        return 1;
    }
    else
    {
        ERR_raiseError(errorToRaise);
        return 0;
    }
}

static void ascendToParent(struct SyntaxContext *context)
{
    assert(getCurrentNode(context)->parentIndex != -1);
    context->currentNodeIndex = getCurrentNode(context)->parentIndex;
    context->currentAttributeIndex = getCurrentNode(context)->attributeIndex; //< getcurrent is now the parentnode.
}

static void descendNewNode(struct SyntaxContext *context, enum STX_NodeType type)
{
    struct STX_SyntaxTreeNode *node = allocateNode(context->tree);
    initializeNode(node);
    node->nodeType = type;
    appendChild(context->tree, getCurrentNode(context), node);
    context->currentNodeIndex = node->id;
    if (context->latestComment)
    {
        struct STX_NodeAttribute *attr;
        allocateAttributeForCurrent(context);
        attr = getCurrentAttribute(context);
        attr->comment = context->latestComment->start;
        attr->commentLength = context->latestComment->length;
        context->latestComment = 0;
    }
}

static int isNumber(const struct LEX_LexerToken *token)
{
    switch (token->tokenType)
    {
        case LEX_OCTAL_NUMBER:
        case LEX_DECIMAL_NUMBER:
        case LEX_HEXA_NUMBER:
        case LEX_FLOAT_NUMBER:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static int isUnaryOperator(const struct LEX_LexerToken *token)
{
    switch (token->tokenType)
    {
        case LEX_KW_NEG:
        case LEX_KW_NOT:
        case LEX_KW_REF:
        case LEX_KW_INC:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static int parseExpression(struct SyntaxContext *context);

/**
* <ArgumentList> ::=
* <Expression> (',' <Expression>)*
*/
static int parseArgumentList(struct SyntaxContext *context)
{
    descendNewNode(context, STX_ARGUMENT_LIST);
    if (!parseExpression(context)) return 0;
    while (getCurrentTokenType(context) == LEX_COMMA)
    {
        acceptCurrent(context);
        if (!parseExpression(context)) return 0;
    }
    ascendToParent(context);
    return 1;
}

static int parseType(struct SyntaxContext *context);

static void allocateAttributeForCurrent(struct SyntaxContext *context)
{
    if (getCurrentNode(context)->attributeIndex == -1)
    {
        int attrIndex = allocateAttribute(context->tree);
        getCurrentNode(context)->attributeIndex = attrIndex;
        context->currentAttributeIndex = attrIndex;
    }
}

static struct STX_NodeAttribute *getCurrentAttribute(struct SyntaxContext *context)
{
    assert(context->currentAttributeIndex >= 0);
    return &context->tree->attributes[context->currentAttributeIndex];
}

static int parseQualifiedname(struct SyntaxContext *context);

/**
 * <Term> ::=
 * ( number |
 * <QualifiedName>  |
 * unaryOperator '(' <Expression> ')' |
 * '(' <Expression> ')' |
 * 'cast' <Type> '(' <Expression> ')' )
 * (( '[' <Expression> ']') | ( '(' <ArgumentList> ')' ))*
 */
static int parseTerm(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_TERM);
    allocateAttributeForCurrent(context);

    token = getCurrentToken(context);
    if (isNumber(token) || (token->tokenType == LEX_STRING))
    {
        attribute = getCurrentAttribute(context);
        attribute->name = token->start;
        attribute->nameLength = token->length;
        attribute->termAttributes.termType = STX_TT_SIMPLE;
        attribute->termAttributes.tokenType = context->current->tokenType;
        acceptCurrent(context);
    }
    else if (token->tokenType == LEX_LEFT_PARENTHESIS)
    {
        acceptCurrent(context);
        if (!parseExpression(context)) return 0;
        if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
        attribute = getCurrentAttribute(context);
        attribute->termAttributes.termType = STX_TT_PARENTHETICAL;
        attribute->termAttributes.tokenType = 0;
    }
    else if (token->tokenType == LEX_IDENTIFIER)
    {
        if (!parseQualifiedname(context)) return 0;
        /*attribute = getCurrentAttribute(context);
        attribute->termAttributes.termType = STX_TT_SIMPLE;
        attribute->termAttributes.tokenType = context->current->tokenType;
        attribute->name = token->start;
        attribute->nameLength = token->length;
        acceptCurrent(context);*/
    }
    else if (isUnaryOperator(token))
    {
        attribute = getCurrentAttribute(context);
        attribute->termAttributes.termType = STX_TT_UNARY_OPERATOR;
        attribute->termAttributes.tokenType = context->current->tokenType;
        acceptCurrent(context);
        if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
        if (!parseExpression(context)) return 0;
        if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
    }
    else if (token->tokenType == LEX_KW_CAST)
    {
        acceptCurrent(context);
        if (!parseType(context)) return 0;
        if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
        if (!parseExpression(context)) return 0;
        if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
        attribute = getCurrentAttribute(context);
        attribute->termAttributes.termType = STX_TT_CAST_EXPRESSION;
        attribute->termAttributes.tokenType = 0;
    }
    else
    {
        ERR_raiseError(E_STX_TERM_EXPECTED);
        return 0;
    }

    for (;;)
    {
        const struct LEX_LexerToken *token = getCurrentToken(context);
        switch (token->tokenType)
        {
            case LEX_LEFT_PARENTHESIS:
            {
                acceptCurrent(context);
                if (!parseArgumentList(context)) return 0;
                if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
            }
            break;
            case LEX_LEFT_BRACKET:
            {
                acceptCurrent(context);
                if (!parseExpression(context)) return 0;
                if (!expect(context, LEX_RIGHT_BRACKET, E_STX_RIGHT_BRACKET_EXPECTED)) return 0;
            }
            break;
            default:
                goto out;
        }
    }
out:

    ascendToParent(context);
    return 1;
}

static int isInfixOperator(const struct LEX_LexerToken *token)
{
    switch (token->tokenType)
    {
        case LEX_PERIOD:
        case LEX_ADD_OPERATOR:
        case LEX_LESS_EQUAL_THAN:
        case LEX_LESS_THAN:
        case LEX_GREATER_EQUAL_THAN:
        case LEX_GREATER_THAN:
        case LEX_EQUAL:
        case LEX_NOT_EQUAL:
        case LEX_MULTIPLY_OPERATOR:
        case LEX_SUBTRACT_OPERATOR:
        case LEX_DIVISION_OPERATOR:
            return 1;
        default:
            return 0;
    }
    return 0;
}

const struct STX_NodeAttribute *STX_getNodeAttribute(const struct STX_SyntaxTreeNode *node)
{
    if (node->attributeIndex == -1)
    {
        return 0;
    }
    else
    {
        return &node->belongsTo->attributes[node->attributeIndex];
    }
}

/*static struct STX_SyntaxTreeNode *getFirstChild(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *node)
{
    if (node->firstChildIndex >= 0)
    {
        return &tree->nodes[node->firstChildIndex];
    }
    else
    {
        return 0;
    }
}*/

/*static struct STX_SyntaxTreeNode *getNextSibling(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *node)
{
    if (node->nextSiblingIndex >= 0)
    {
        return &tree->nodes[node->nextSiblingIndex];
    }
    else
    {
        return 0;
    }
}*/

/*static int getPrecedenceLevel(enum LEX_TokenType type)
{
    switch (type)
    {
        case LEX_LESS_EQUAL_THAN:
        case LEX_LESS_THAN:
        case LEX_GREATER_EQUAL_THAN:
        case LEX_GREATER_THAN:
        case LEX_EQUAL:
        case LEX_NOT_EQUAL:
            return 1;
        case LEX_ADD_OPERATOR:
        case LEX_SUBTRACT_OPERATOR:
            return 2;
        case LEX_MULTIPLY_OPERATOR:
            return 3;
        case LEX_PERIOD:
            return 4;
        default:
            assert(0);
        break;
    }
    return -100;
}*/

/*static void removeNode(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *node)
{

    if (node->previousSiblingIndex >= 0)
    {
        tree->nodes[node->previousSiblingIndex].nextSiblingIndex = node->nextSiblingIndex;
    }
    else
    {
        tree->nodes[node->parentIndex].firstChildIndex = node->nextSiblingIndex;
    }
    if (node->nextSiblingIndex >= 0)
    {
        tree->nodes[node->nextSiblingIndex].previousSiblingIndex = node->previousSiblingIndex;
    }
    else
    {
        tree->nodes[node->parentIndex].lastChildIndex = node->previousSiblingIndex;
    }
}*/

/*static void organizeExpressionTree(struct SyntaxContext *context)
{
    int count;
    struct STX_SyntaxTreeNode **operandStack;
    struct STX_SyntaxTreeNode **operatorStack;
    struct STX_SyntaxTreeNode *expressionNode = getCurrentNode(context);
    int operatorPtr = -1;
    int operandPtr = -1;
    struct STX_SyntaxTreeNode *currentNode = getFirstChild(context->tree, expressionNode);


    assert(getCurrentNode(context)->nodeType == STX_EXPRESSION);
    count = currentNode->lastChildIndex - currentNode->firstChildIndex;

    operandStack = malloc(count * sizeof(struct STX_SyntaxTreeNode *));
    operatorStack = malloc(count * sizeof(struct STX_SyntaxTreeNode *));

    operandStack[++operandPtr] = currentNode;
    removeNode(context->tree, currentNode);
    currentNode = getNextSibling(context->tree, currentNode);

    while (currentNode)
    {
        const struct STX_NodeAttribute *attribute;
        const struct STX_NodeAttribute *topAttribute;
        // get operator
        attribute = STX_getNodeAttribute(currentNode);
        for (;;)
        {
            if (operatorPtr == -1) topAttribute = 0;
            else
            {
                topAttribute = STX_getNodeAttribute(operatorStack[operatorPtr]);
            }
            if (topAttribute &&
                (getPrecedenceLevel(topAttribute->operatorAttributes.type) >=
                    getPrecedenceLevel(attribute->operatorAttributes.type)))
            {
                struct STX_SyntaxTreeNode *operatorNode = operatorStack[operatorPtr--];
                struct STX_SyntaxTreeNode *operand2 = operandStack[operandPtr--];
                struct STX_SyntaxTreeNode *operand1 = operandStack[operandPtr--];
                removeNode(context->tree, operatorNode);
                removeNode(context->tree, operand1);
                removeNode(context->tree, operand2);
                appendChild(context->tree, operatorNode, operand1);
                appendChild(context->tree, operatorNode, operand2);
                operandStack[++operandPtr] = operatorNode;

            }
            else
                break;
        }
        operatorStack[++operatorPtr] = currentNode;

        // get operand
        currentNode = getNextSibling(context->tree, currentNode);
        operandStack[++operandPtr] = currentNode;
        // get next operator
        currentNode = getNextSibling(context->tree, currentNode);

    }
    while(operatorPtr >= 0)
    {
        struct STX_SyntaxTreeNode *operatorNode = operatorStack[operatorPtr--];
        struct STX_SyntaxTreeNode *operand2 = operandStack[operandPtr--];
        struct STX_SyntaxTreeNode *operand1 = operandStack[operandPtr--];
        removeNode(context->tree, operatorNode);
        removeNode(context->tree, operand1);
        removeNode(context->tree, operand2);
        appendChild(context->tree, operatorNode, operand1);
        appendChild(context->tree, operatorNode, operand2);
        operandStack[++operandPtr] = operatorNode;
    }

    appendChild(context->tree, expressionNode, operandStack[0]);

    free(operandStack);
    free(operatorStack);
}*/

/**
 * <Expression> ::=
 * <Term> ((infixOperator | <QualifiedName>) <Term>)*
 */
static int parseExpression(struct SyntaxContext *context)
{
    descendNewNode(context, STX_EXPRESSION);
    allocateAttributeForCurrent(context);
    if (!parseTerm(context)) return 0;
    for (;;)
    {
        const struct LEX_LexerToken *token;
        struct STX_NodeAttribute *attribute;

        token = getCurrentToken(context);
        if (!isInfixOperator(token) && (token->tokenType != LEX_IDENTIFIER))
        {
            break;
        }
        descendNewNode(context, STX_OPERATOR);
        {
            if (token->tokenType == LEX_IDENTIFIER)
            {
                if (!parseQualifiedname(context)) return 0;
            }
            else
            {
                allocateAttributeForCurrent(context);
                attribute = getCurrentAttribute(context);
                attribute->operatorAttributes.type = getCurrentToken(context)->tokenType;
                acceptCurrent(context);
            }
        }
        ascendToParent(context);
        if (!parseTerm(context)) return 0;
    }
    //organizeExpressionTree(context);
    ascendToParent(context);
    return 1;
}

/**
 * <ReturnStatement> ::=
 * 'return' <Expression>? ';'
 */
static int parseReturnStatement(struct SyntaxContext *context)
{
    descendNewNode(context, STX_RETURN_STATEMENT);
    if (!expect(context, LEX_KW_RETURN, E_STX_RETURN_EXPECTED)) return 0;
    if (getCurrentTokenType(context) != LEX_SEMICOLON)
    {
        if (!parseExpression(context)) return 0;
    }
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
    ascendToParent(context);
    return 1;
}

static int parseStatement(struct SyntaxContext *context);
static int parseBlock(struct SyntaxContext *context);



/**
 * <IfStatement> ::=
 * 'if' '(' <Expression> ')' <Block> ('else' <Block> )?
 */

static int parseIfStatement(struct SyntaxContext *context)
{
    descendNewNode(context, STX_IF_STATEMENT);

    if (!expect(context, LEX_KW_IF, E_STX_IF_EXPECTED)) return 0;
    if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
    if (!parseExpression(context)) return 0;
    if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
    if (!parseBlock(context)) return 0;
    if (getCurrentTokenType(context) == LEX_KW_ELSE)
    {
        acceptCurrent(context);
        if (!parseBlock(context)) return 0;
    }

    ascendToParent(context);
    return 1;
}

/**
 * <LoopNextStatement> ::=
 * 'loop' <Block> ('next' <Block>)?
 *
 */

static int parseLoopNextStatement(struct SyntaxContext *context)
{
    descendNewNode(context, STX_LOOP_STATEMENT);
    if (!expect(context, LEX_KW_LOOP, E_STX_LOOP_EXPECTED)) return 0;
    if (!parseBlock(context)) return 0;
    if (getCurrentTokenType(context) == LEX_KW_NEXT)
    {
        acceptCurrent(context);
        if (!parseBlock(context)) return 0;
    }
    ascendToParent(context);
    return 1;
}

static int parseVariableDeclaration(struct SyntaxContext *context);

static void setCurrentNodeType(struct SyntaxContext *context, enum STX_NodeType nodeType)
{
    context->tree->nodes[context->currentNodeIndex].nodeType = nodeType;
}

static int parseSimpleStatement(struct SyntaxContext *context)
{
    descendNewNode(context, STX_EXPRESSION_STATEMENT);

    if (!parseExpression(context))
    {
        return 0;
    }
    if (getCurrentTokenType(context) == LEX_ASSIGN_OPERATOR)
    {
        setCurrentNodeType(context, STX_ASSIGNMENT);
        acceptCurrent(context);
        if (!parseExpression(context))
        {
            return 0;
        }
    }
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;

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
 * <CaseBlock> ::=
 * (('case' integer_number ) | 'default') ':' <Block> ('break' | 'continue' ) ';'
 */

static int parseCaseBlock(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_CASE);
    allocateAttributeForCurrent(context);
    attr = getCurrentAttribute(context);
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_CASE:
        {
            if (!expect(context, LEX_KW_CASE, E_STX_CASE_EXPECTED)) return 0;
            if (!isIntegerNumberToken(getCurrentTokenType(context)))
            {
                ERR_raiseError(E_STX_INTEGER_NUMBER_EXPECTED);
                return 0;
            }
            attr->caseAttributes.caseValue = getIntegerValue(getCurrentToken(context));
            attr->caseAttributes.isDefault = 0;
            acceptCurrent(context);
        }
        break;
        case LEX_KW_DEFAULT:
        {
            attr->caseAttributes.isDefault = 1;
            acceptCurrent(context);
        }
        break;
        default:
            ERR_raiseError(E_STX_CASE_OR_DEFAULT_EXPECTED);
            return 0;
    }

    if (!expect(context, LEX_COLON, E_STX_COLON_EXPECTED)) return 0;
    if (!parseBlock(context)) return 0;
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_BREAK:
        case LEX_KW_CONTINUE:
            acceptCurrent(context);
        break;
        default:
            ERR_raiseError(E_STX_BREAK_OR_CONTINUE_EXPECTED);
            return 0;
    }
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;


    ascendToParent(context);
    return 1;
}

/**
 * <SwitchStatement> ::=
 * 'switch' '(' <Expression> ')' '{' <CaseBlock>* '}'
 */
static int parseSwitchDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_SWITCH);
    if (!expect(context, LEX_KW_SWITCH, E_STX_SWITCH_EXPECTED)) return 0;
    if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
    if (!parseExpression(context)) return 0;
    if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
    if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
    {
        if (!parseCaseBlock(context)) return 0;
    }

    if (!expect(context, LEX_RIGHT_BRACE, E_STX_RIGHT_BRACE_EXPECTED)) return 0;



    ascendToParent(context);
    return 1;
}

/**
 * <Statement> ::=
 * <ReturnStatement> |
 * <IfStatement> |
 * <LoopNextStatement>
 * <VariableDeclaration> |
 * <SimpleStatement> |
 * <SwitchStatement> |
 * <BreakStatement> |
 * <ContinueStatement>
 *
 */
static int parseStatement(struct SyntaxContext *context)
{
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_RETURN:
            if (!parseReturnStatement(context)) return 0;
        break;
        case LEX_KW_IF:
            if (!parseIfStatement(context)) return 0;
        break;
        case LEX_KW_LOOP:
            if (!parseLoopNextStatement(context)) return 0;
        break;
        case LEX_KW_VARDECL:
            if (!parseVariableDeclaration(context)) return 0;
        break;
        case LEX_KW_SWITCH:
            if (!parseSwitchDeclaration(context)) return 0;
        break;
        case LEX_KW_CONTINUE:
            acceptCurrent(context);
            if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
            descendNewNode(context, STX_CONTINUE);
            ascendToParent(context);
        break;
        case LEX_KW_BREAK:
            acceptCurrent(context);
            if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
            descendNewNode(context, STX_BREAK);
            ascendToParent(context);
        break;
        default:
            if (!parseSimpleStatement(context)) return 0;
        break;
    }
    return 1;
}

/**
 * <Block> ::=
 * '{' <Statement>* '}'
 */
static int parseBlock(struct SyntaxContext *context)
{
    descendNewNode(context, STX_BLOCK);
    if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
    {
        if (!parseStatement(context))
        {
            return 0;
        }
    }

    if (!expect(context, LEX_RIGHT_BRACE, E_STX_RIGHT_BRACE_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
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
    allocateAttributeForCurrent(context);
    token = getCurrentToken(context);

    if (token->tokenType == LEX_KW_HANDLE)
    {

        acceptCurrent(context);
        if (!expect(context, LEX_KW_OF, E_STX_OF_EXPECTED)) return 0;
        attribute = getCurrentAttribute(context);
        attribute->typePrefixAttributes.type = STX_TP_HANDLE;
    }
    else if (token->tokenType == LEX_KW_BUFFER)
    {
        int elements = 0;

        acceptCurrent(context);
        if (!expect(context, LEX_LEFT_BRACKET, E_STX_LEFT_BRACKET_EXPECTED)) return 0;
        token = getCurrentToken(context);
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
        if (!expect(context, LEX_RIGHT_BRACKET, E_STX_RIGHT_BRACKET_EXPECTED)) return 0;
        if (!expect(context, LEX_KW_OF, E_STX_OF_EXPECTED)) return 0;
        attribute = getCurrentAttribute(context);
        attribute->typePrefixAttributes.type = STX_TP_BUFFER;
        attribute->typePrefixAttributes.elements = elements;
    }
    else if (
        (token->tokenType == LEX_KW_POINTER) ||
        (token->tokenType == LEX_KW_LOCALPTR))
    {
        acceptCurrent(context);
        if (!expect(context, LEX_KW_TO, E_STX_TO_EXPECTED)) return 0;
        attribute = getCurrentAttribute(context);
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
    allocateAttributeForCurrent(context);
    token = getCurrentToken(context);

    if (token->tokenType == LEX_BUILT_IN_TYPE)
    {
        struct STX_NodeAttribute *attr = getCurrentAttribute(context);
        attr->name = token->start;
        attr->nameLength = token->length;
        acceptCurrent(context);
    }
    else if (token->tokenType == LEX_IDENTIFIER)
    {
        if (!parseQualifiedname(context)) return 0;
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
 * <VariableDeclaration> ::= 'vardecl' <Type> identifier ( ':=' <Expression> )? ';'
 */
static int parseVariableDeclaration(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_VARDECL);
    allocateAttributeForCurrent(context);
    if (!expect(context, LEX_KW_VARDECL, E_STX_VARDECL_EXPECTED)) return 0;
    if (!parseType(context )) return 0;
    current = getCurrentToken(context);
    if (current->tokenType == LEX_IDENTIFIER)
    {
        attr = getCurrentAttribute(context);
        attr->name = current->start;
        attr->nameLength = current->length;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    if (getCurrentTokenType(context) == LEX_ASSIGN_OPERATOR)
    {
        acceptCurrent(context);
        if (!parseExpression(context)) return 0;
    }

    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;

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
    const struct LEX_LexerToken *token = getCurrentToken(context);
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_PARAMETER);
    allocateAttributeForCurrent(context);

    if (
        (token->tokenType != LEX_KW_IN) &&
        (token->tokenType != LEX_KW_OUT) &&
        (token->tokenType != LEX_KW_REF))
    {
        ERR_raiseError(E_STX_PARAMETER_DIRECTION_EXPECTED);
        return 0;
    }
    attribute = getCurrentAttribute(context);
    attribute->parameterAttributes.direction = mapTokenTypeToDirection(token->tokenType);
    acceptCurrent(context);
    if (!parseType(context)) return 0;
    token = getCurrentToken(context);
    if (token->tokenType != LEX_IDENTIFIER)
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    attribute = getCurrentAttribute(context);
    attribute->name = token->start;
    attribute->nameLength = token->length;
    acceptCurrent(context);

    ascendToParent(context);
    return 1;
}

/**
 * <ParameterList> ::=
 * (<Parameter> (',' <Parameter>)*)?
 */
static int parseParameterList(struct SyntaxContext *context)
{
    descendNewNode(context, STX_PARAMETER_LIST);
    if (getCurrentTokenType(context) != LEX_RIGHT_PARENTHESIS)
    {
        if (!parseParameter(context)) return 0;
        while (getCurrentTokenType(context) != LEX_RIGHT_PARENTHESIS)
        {
            if (!expect(context, LEX_COMMA, E_STX_COMMA_EXPECTED)) return 0;
            if (!parseParameter(context)) return 0;
        }
    }

    ascendToParent(context);
    return 1;
}

static int isPrecedenceTokenType(enum LEX_TokenType type)
{
    switch (type)
    {
        case LEX_KW_ADDITIVE:
        case LEX_KW_MULTIPLICATIVE:
        case LEX_KW_RELATIONAL:
            return 1;
        default:
            return 0;
    }
    return 0;
}

/**
 * <FunctionDeclaration> ::=
 * ('function' | 'operator' precedenceType) <Type> identifier '(' <ParameterList> ')'
 * ((<Block> ( 'cleanup' <Block>)?) | ('external' string ':' string ';'))
 */
static int parseFunctionDeclaration(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attribute;
    const struct LEX_LexerToken *token;
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_FUNCTION:
            descendNewNode(context, STX_FUNCTION);
            allocateAttributeForCurrent(context);
            acceptCurrent(context);
        break;
        case LEX_KW_OPERATOR:
            descendNewNode(context, STX_OPERATOR_FUNCTION);
            allocateAttributeForCurrent(context);
            acceptCurrent(context);
            if (isPrecedenceTokenType(getCurrentTokenType(context)))
            {
                struct STX_NodeAttribute *attr = getCurrentAttribute(context);
                attr->functionAttributes.precedence = getCurrentTokenType(context);
                acceptCurrent(context);
            }
            else
            {
                ERR_raiseError(E_STX_PRECEDENCE_TYPE_EXPECTED);
                return 0;
            }
        break;
        default:
            ERR_raiseError(E_STX_FUNCTION_EXPECTED);
            return 0;

    }
    if (!parseType(context)) return 0;
    token = getCurrentToken(context);
    if (token->tokenType != LEX_IDENTIFIER)
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    attribute = getCurrentAttribute(context);
    attribute->name = token->start;
    attribute->nameLength = token->length;
    acceptCurrent(context);
    if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
    if (!parseParameterList(context)) return 0;
    if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
    switch (getCurrentTokenType(context))
    {
        case LEX_LEFT_BRACE:
        {
            attribute->functionAttributes.isExternal = 0;
            if (!parseBlock(context)) return 0;
            if (getCurrentTokenType(context) == LEX_KW_CLEANUP)
            {
                acceptCurrent(context);
                if (!parseBlock(context)) return 0;
            }
        }
        break;
        case LEX_KW_EXTERNAL:
        {
            acceptCurrent(context);
            attribute->functionAttributes.isExternal = 1;
            if (getCurrentTokenType(context) == LEX_STRING)
            {
                const struct LEX_LexerToken *token = getCurrentToken(context);
                attribute->functionAttributes.externalLocation = token->start;
                attribute->functionAttributes.externalLocationLength = token->length;
            }
            if (!expect(context, LEX_STRING, E_STX_STRING_EXPECTED)) return 0;
            if (!expect(context, LEX_COLON, E_STX_COLON_EXPECTED)) return 0;
            if (getCurrentTokenType(context) == LEX_STRING)
            {
                const struct LEX_LexerToken *token = getCurrentToken(context);
                attribute->functionAttributes.externalFileType = token->start;
                attribute->functionAttributes.externalFileTypeLength = token->length;
            }
            if (!expect(context, LEX_STRING, E_STX_STRING_EXPECTED)) return 0;
            if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
        }
        break;
        default:
        {
            ERR_raiseError(E_STX_BLOCK_OR_EXTERNAL_EXPECTED);
            return 0;
        }
    }

    ascendToParent(context);
    return 1;
}

static int parseDeclaration(struct SyntaxContext *context);

/**
 * <NamespaceDeclaration> ::=
 * 'namespace' identifier '{' <Declarations> '}'
 */
static int parseNamespaceDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_NAMESPACE);
    allocateAttributeForCurrent(context);

    if (!expect(context, LEX_KW_NAMESPACE, E_STX_NAMESPACE_EXPECTED)) return 0;
    if (getCurrentTokenType(context) == LEX_IDENTIFIER)
    {
        const struct LEX_LexerToken *token = getCurrentToken(context);
        struct STX_NodeAttribute *attr = getCurrentAttribute(context);
        attr->name = token->start;
        attr->nameLength = token->length;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
    {
        if (!parseDeclaration(context)) return 0;
    }
    if (!expect(context, LEX_RIGHT_BRACE, E_STX_RIGHT_BRACE_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
}

/**
 * QualifiedName ::=
 * identifier ( '::' identifier)*
 */

static int parseQualifiedname(struct SyntaxContext *context)
 {
    descendNewNode(context, STX_QUALIFIED_NAME);

    if (getCurrentTokenType(context) == LEX_IDENTIFIER)
    {
        const struct LEX_LexerToken *current = getCurrentToken(context);
        struct STX_NodeAttribute *attribute;
        descendNewNode(context, STX_QUALIFIED_NAME_PART);
        allocateAttributeForCurrent(context);
        attribute = getCurrentAttribute(context);
        attribute->name = current->start;
        attribute->nameLength = current->length;
        ascendToParent(context);
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    while (getCurrentTokenType(context) == LEX_SCOPE_SEPARATOR)
    {
        acceptCurrent(context);
        if (getCurrentTokenType(context) == LEX_IDENTIFIER)
        {
            const struct LEX_LexerToken *current = getCurrentToken(context);
            struct STX_NodeAttribute *attribute;
            descendNewNode(context, STX_QUALIFIED_NAME_PART);
            allocateAttributeForCurrent(context);
            attribute = getCurrentAttribute(context);
            attribute->name = current->start;
            attribute->nameLength = current->length;
            ascendToParent(context);
            acceptCurrent(context);
        }
        else
        {
            ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
            return 0;
        }
    }

    ascendToParent(context);
    return 1;
 }

/**
 * <UsingDeclaration> ::=
 * 'using' <QualifiedName> ';'
 */

static int parseUsingDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_USING);

    if (!expect(context, LEX_KW_USING, E_STX_USING_EXPECTED)) return 0;
    if (!parseQualifiedname(context)) return 0;
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
}

/**
 * StructDeclaration ::=
 * 'struct' identifier '{'  (<Type> identifier ';' )* '}'
 */
static int parseStructDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_STRUCT);

    if (!expect(context, LEX_KW_STRUCT, E_STX_STRUCT_EXPECTED)) return 0;
    if (getCurrentTokenType(context) == LEX_IDENTIFIER)
    {
        allocateAttributeForCurrent(context);
        struct STX_NodeAttribute *attr = getCurrentAttribute(context);
        const struct LEX_LexerToken *token = getCurrentToken(context);
        attr->name = token->start;
        attr->nameLength = token->length;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
    {
        descendNewNode(context, STX_FIELD);
        allocateAttributeForCurrent(context);
        if (!parseType(context)) return 0;
        if (getCurrentTokenType(context) == LEX_IDENTIFIER)
        {
            const struct LEX_LexerToken *token = getCurrentToken(context);
            struct STX_NodeAttribute *attr = getCurrentAttribute(context);
            attr->name = token->start;
            attr->nameLength = token->length;
            acceptCurrent(context);
        }
        else
        {
            ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
            return 0;
        }

        if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
        ascendToParent(context);
    }

    if (!expect(context, LEX_RIGHT_BRACE, E_STX_RIGHT_BRACE_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
}

/**
 * FuncPtrDeclaration ::=
 * 'funcptr' <Type> identifer '(' <ParameterList> ')' ';'
 */
static int parseFuncPtrDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_FUNCPTR);
    allocateAttributeForCurrent(context);
    if (!expect(context, LEX_KW_FUNCPTR, E_STX_FUNCPTR_EXPECTED)) return 0;
    if (!parseType(context)) return 0;
    if (getCurrentTokenType(context) == LEX_IDENTIFIER)
    {
        struct STX_NodeAttribute *attr = getCurrentAttribute(context);
        const struct LEX_LexerToken *token = getCurrentToken(context);
        attr->name = token->start;
        attr->nameLength = token->length;
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
        return 0;
    }
    if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
    if (!parseParameterList(context)) return 0;
    if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;


    ascendToParent(context);
    return 1;
}

/**
 * PlatfromDeclaration :=
 * 'platform' string (, string)* '{' <Declaration>* '}'
 */
static int parsePlatformDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_PLATFORM);
    {
        if (!expect(context, LEX_KW_PLATFORM, E_STX_PLATFORM_EXPECTED)) return 0;
        descendNewNode(context, STX_FORPLATFORM);
        {
            allocateAttributeForCurrent(context);
            if (getCurrentTokenType(context) == LEX_STRING)
            {
                const struct LEX_LexerToken *token = getCurrentToken(context);
                struct STX_NodeAttribute *attr = getCurrentAttribute(context);
                attr->name = token->start;
                attr->nameLength = token->length;
                acceptCurrent(context);
            }
            else
            {
                ERR_raiseError(E_STX_STRING_EXPECTED);
                return  0;
            }
        }
        ascendToParent(context);
        while (getCurrentTokenType(context) == LEX_COMMA)
        {
            acceptCurrent(context);
            descendNewNode(context, STX_FORPLATFORM);
            {
                allocateAttributeForCurrent(context);
                if (getCurrentTokenType(context) == LEX_STRING)
                {
                    const struct LEX_LexerToken *token = getCurrentToken(context);
                    struct STX_NodeAttribute *attr = getCurrentAttribute(context);
                    attr->name = token->start;
                    attr->nameLength = token->length;
                    acceptCurrent(context);
                }
                else
                {
                    ERR_raiseError(E_STX_STRING_EXPECTED);
                    return  0;
                }
            }
            ascendToParent(context);
        }
        descendNewNode(context, STX_DECLARATIONS);
        {
            if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
            while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
            {
                if (!parseDeclaration(context)) return 0;
            }
            if (!expect(context, LEX_RIGHT_BRACE, E_STX_RIGHT_BRACE_EXPECTED)) return 0;
        }
        ascendToParent(context);

    }
    ascendToParent(context);
    return 1;
}

/**
 * Declaration ::=
 * <VariableDeclaration> |
 * <FunctionDeclaration> |
 * <NamespaceDeclaration> |
 * <UsingDeclaration> |
 * <StructDeclaration> |
 * <FuncPtrDeclaration> |
 * <PlatfromDeclaration>
 */
static int parseDeclaration(struct SyntaxContext *context)
{
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_VARDECL:
            if (!parseVariableDeclaration(context)) return 0;
        break;
        case LEX_KW_FUNCTION:
        case LEX_KW_OPERATOR:
            if (!parseFunctionDeclaration(context)) return 0;
        break;
        case LEX_KW_NAMESPACE:
            if (!parseNamespaceDeclaration(context)) return 0;
        break;
        case LEX_KW_USING:
            if (!parseUsingDeclaration(context)) return 0;
        break;
        case  LEX_KW_STRUCT:
            if (!parseStructDeclaration(context)) return 0;
        break;
        case LEX_KW_FUNCPTR:
            if (!parseFuncPtrDeclaration(context)) return 0;
        break;
        case LEX_KW_PLATFORM:
            if (!parsePlatformDeclaration(context)) return 0;
        break;
        default:
            ERR_raiseError(E_STX_DECLARATION_EXPECTED);
            return 0;
        break;
    }
    return 1;
}

/**
 * Module ::=
 * 'module' {'exe' | 'lib' | 'dll' } ';'
 * <Declaration>*
 * 'main'
 * <Block>
 */

static int parseModule(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_MODULE);
    allocateAttributeForCurrent(context);

    if (!expect(context, LEX_KW_MODULE, E_STX_MODULE_EXPECTED)) return 0;
    current = getCurrentToken(context);
    if (
        (current->tokenType == LEX_KW_EXE) ||
        (current->tokenType == LEX_KW_DLL) ||
        (current->tokenType == LEX_KW_LIB))
    {
        attribute = getCurrentAttribute(context);
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
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_STX_MODULE_TYPE_EXPECTED);
        return 0;
    }
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_KW_MAIN)
    {
        if (!parseDeclaration(context)) return 0;
    }
    if (!expect(context, LEX_KW_MAIN, E_STX_MAIN_EXPECTED)) return 0;
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
    context.currentNodeIndex = tree->rootNodeIndex;
    context.latestComment = 0;

    parseModule(&context);

    result.tree = tree;
    {
        const struct LEX_LexerToken *current = getCurrentToken(&context);
        if (current)
        {
            result.line = current->beginLine;
            result.column = current->beginColumn;
        }
        else
        {
            result.line = 0;
            result.column = 0;
        }
    }

    return result;
}

struct STX_SyntaxTreeNode *STX_getRootNode(struct STX_SyntaxTree *tree)
{
    return &tree->nodes[tree->rootNodeIndex];
}

