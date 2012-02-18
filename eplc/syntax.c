/**
 * Copyright (c) 2012, Csirmaz Dávid
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @file
 * This module builds a raw syntax tree from the tokens using recursive descent parsing.
 *
 * Semantics are not checked by this module. So syntax tree can be non-sense after this phase.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "syntax.h"
#include "lexer.h"
#include "error.h"

/**
 * Stores data about the parsing.
 */
struct SyntaxContext
{
    struct STX_SyntaxTree *tree; ///< Stores the syntax tree being built.

    const struct LEX_LexerToken *tokens; ///< The array with the tokens.
    int tokenCount; ///< Count of tokens in that array.
    int tokensRemaining; ///< Remaining tokens

    const struct LEX_LexerToken  *current; ///< current token
    int currentNodeIndex; ///< Index of the curent node.

    const struct LEX_LexerToken *latestComment; ///< The latest comment token.
};

static struct STX_SyntaxTreeNode *allocateNode(struct STX_SyntaxTree *tree);
static void initializeNode(struct STX_SyntaxTreeNode *node);

/**
 * Recursive function of the preorder transversal.
 *
 * @param [in,out] tree The tree to transverse.
 * @param [in] level The level of the recursion.
 * @param [in,out] node The current node.
 * @param [in] callback A callback function which is caller for every node.
 * @param [in] userData an user profiled data, will be passed to the callback.
 *
 * @retval 0 if callback returns 0. This also stops the transversal.
 * @retval 1 otherwise.
 */
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

/**
 * Initializes the syntax tree.
 *
 * @param [in,out] tree The tree to initialize.
 */
static void initializeSyntaxTree(struct STX_SyntaxTree *tree)
{
    struct STX_SyntaxTreeNode *node;
    memset(tree, 0, sizeof(struct STX_SyntaxTree));
    node = allocateNode(tree);
    initializeNode(node);
    tree->rootNodeIndex = node->id;
    node->nodeType = STX_ROOT;
}

void STX_removeAllChildren(struct STX_SyntaxTreeNode *node)
{
    node->firstChildIndex = -1;
    node->lastChildIndex = -1;
}

void STX_appendChild(
    struct STX_SyntaxTree *tree,
    struct STX_SyntaxTreeNode *node,
    struct STX_SyntaxTreeNode *child)
{
    assert(tree == node->belongsTo);
    assert(tree == child->belongsTo);
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

/**
 * Allocates node from the tree's node pool.
 *
 * @param [in,out] tree The tree to allocate the node from.
 *
 * @return The allocated node:
 *
 * @todo Handle memory allocation errors.
 */
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
    node->inScopeId = -1;
    node->definesScopeId = -1;
    node->id = tree->nodeCount;
    node->belongsTo = tree;
    tree->nodeCount++;
    return node;
}

/**
 * Initializes a node
 *
 * @param node The node to initialize.
 */
static void initializeNode(struct STX_SyntaxTreeNode *node)
{
    node->parentIndex = -1;
    node->firstChildIndex = -1;
    node->lastChildIndex = -1;
    node->nextSiblingIndex = -1;
    node->previousSiblingIndex = -1;
    node->beginColumn = 0;
    node->beginLine = 0;
    node->endColumn = 0;
    node->endLine = 0;

    memset(&node->attribute, 0, sizeof(node->attribute));
    node->attribute.symbolDefinitionNodeId = -1;
}

/**
 * @param context context
 *
 * @return The current token.
 */
static const struct LEX_LexerToken *getCurrentToken(struct SyntaxContext *context)
{
    return context->current;
}

/**
 * @param context context
 *
 * @return The type of the current token. LEX_SPEC_EOF if there is no
 *      current token (read past the last)
 */
static enum LEX_TokenType getCurrentTokenType(struct SyntaxContext *context)
{
    if (!context->current)
    {
        return LEX_SPEC_EOF;
    }
    return context->current->tokenType;
}

/**
 * @param tokenType subject.
 *
 * @retval true The token is a comment token.
 * @retval false otherwise.
 */
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
static struct STX_NodeAttribute *getCurrentAttribute(struct SyntaxContext *context);
static void ascendToParent(struct SyntaxContext *context);

/**
 * Advances to the next token, and makes it current.
 *
 * @param [in,out] context The context.
 */
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

/**
 * @param [in] tokenType subject
 *
 * @retval true The tokenType is a documentation comment token type.
 * @retval false otherwise.
 */
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

/**
 * @param [in] tokenType subject
 *
 * @retval true The tokenType is a documentation back comment token type.
 * @retval false otherwise.
 */
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

/**
 * @param [in] context context.
 *
 * @return The current node.
 */
static struct STX_SyntaxTreeNode *getCurrentNode(struct SyntaxContext *context)
{
    return &context->tree->nodes[context->currentNodeIndex];
}

/**
 * Skips comments. Documentation comments are set as attributes on the
 * appropriate nodes.
 *
 * @param [in,out] context context.
 */
static void skipComments(struct SyntaxContext *context)
{
    // Skip tokens while the current is a comment.
    while (isCommentTokenType(getCurrentTokenType(context)))
    {
        if (isForwardDocumentationCommentType(getCurrentTokenType(context)))
        {
            // On forward documentation the latest comment, it will set as attribute
            // on the next node.
            context->latestComment = getCurrentToken(context);
        }
        else if (isBackDocumentationCommentType(getCurrentTokenType(context)))
        {
            struct STX_SyntaxTreeNode *currentNode = getCurrentNode(context);
            struct STX_NodeAttribute *attr;
            const struct LEX_LexerToken *token = getCurrentToken(context);

            // Back comments are set as attribute on the current node.
            attr = getCurrentAttribute(context);
            currentNode->attribute = *attr;
            attr->comment = token->start;
            attr->commentLength = token->length;
        }
        // Move to the next token.
        advance(context);
    }
}

/**
 * Accepts current token, moves to the next token (after skipped the comment tokens)
 *
 * @param context context.
 */
static void acceptCurrent(struct SyntaxContext *context)
{
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    const struct LEX_LexerToken *token = getCurrentToken(context);
    node->endColumn = token->endColumn;
    node->endLine = token->endLine;
    advance(context);
    skipComments(context);
}

/**
 * Expects the given token type. If the current token is not the expected one
 * it raises an error. If it's of the expected type, accepts it.
 *
 * It can raise E_STX_UNEXPECTED_END_OF_FILE if no there are no more tokens.
 *
 * @param [in,out] context The context.
 * @param [in] type the expected token type.
 * @param [in] errorToRaise The error code to raise if the token type is not as expected.
 *
 * @retval 1 if the current token is the same type as expected.
 * @retval 0 on error, and the given error code is raised.
 */
static int expect(
    struct SyntaxContext *context,
    enum LEX_TokenType type,
    enum ERR_ErrorCode errorToRaise)
{
    const struct LEX_LexerToken *token;

    token = getCurrentToken(context);

    if (!token)
    {
        // We are at the end of file.
        ERR_raiseError(E_STX_UNEXPECTED_END_OF_FILE);
        return 0;
    }
    if (token->tokenType == type)
    {
        // It's of the expected type, so accept it.
        acceptCurrent(context);
        return 1;
    }
    else
    {
        // Not the expected one: raise error.
        ERR_raiseError(errorToRaise);
        return 0;
    }
}

/**
 * Sets the current token's parent as current.
 *
 * This function usually called at the end of the expansion
 * of a grammar rule.
 *
 * @param context context.
 */
static void ascendToParent(struct SyntaxContext *context)
{
    struct STX_SyntaxTreeNode *node = getCurrentNode(context);
    struct STX_SyntaxTreeNode *parentNode;
    assert(node->parentIndex != -1);
    context->currentNodeIndex = getCurrentNode(context)->parentIndex;
    parentNode = getCurrentNode(context);
    parentNode->endColumn = node->endColumn;
    parentNode->endLine = node->endLine;
}

/**
 * Appends new child to the current node and makes it current.
 *
 * This function is usually called at the beginning of the expansion
 * of a grammar rule.
 *
 * @param [in,out] context context.
 * @param [in] type the type of the new node.
 */
static void descendNewNode(struct SyntaxContext *context, enum STX_NodeType type)
{
    struct STX_SyntaxTreeNode *node = allocateNode(context->tree);
    const struct LEX_LexerToken *token = getCurrentToken(context);
    initializeNode(node);
    node->nodeType = type;
    node->beginColumn = token->beginColumn;
    node->beginLine = token->beginLine;
    STX_appendChild(context->tree, getCurrentNode(context), node);
    context->currentNodeIndex = node->id;
    if (context->latestComment)
    {
        // Sets the comment attribute on the node, if preceded by a comment token.
        struct STX_NodeAttribute *attr;
        attr = getCurrentAttribute(context);
        attr->comment = context->latestComment->start;
        attr->commentLength = context->latestComment->length;
        context->latestComment = 0;
    }
}

/**
 * @param token subject.
 *
 * @retval true if the token is number.
 * @retval false if not.
 */
static int isNumber(const struct LEX_LexerToken *token)
{
    switch (token->tokenType)
    {
        case LEX_OCTAL_INTEGER:
        case LEX_DECIMAL_INTEGER:
        case LEX_HEXA_INTEGER:
        case LEX_FLOAT_NUMBER:
            return 1;
        default:
            return 0;
    }
    return 0;
}

/**
 * @param token subject.
 *
 * @retval true The token is an unary operator
 * @retval false otherwise.
 */
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
 * Parses argument list. The argument list is used in function calls.
 *
 @verbatim
  <ArgumentList> ::=
  '(' <Expression>? (',' <Expression>)* ')'
 @endverbatim
 *
 * @param context context
 *
 * @return Nonzero on success, zero on error.
 *
 */
static int parseArgumentList(struct SyntaxContext *context)
{
    descendNewNode(context, STX_ARGUMENT_LIST);
    if (!expect(context, LEX_LEFT_PARENTHESIS, E_STX_LEFT_PARENTHESIS_EXPECTED)) return 0;
    if (getCurrentTokenType(context) != LEX_RIGHT_PARENTHESIS)
    {
        if (!parseExpression(context)) return 0;
        while (getCurrentTokenType(context) == LEX_COMMA)
        {
            acceptCurrent(context);
            if (!parseExpression(context)) return 0;
        }
    }
    if (!expect(context, LEX_RIGHT_PARENTHESIS, E_STX_RIGHT_PARENTHESIS_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
}

static int parseType(struct SyntaxContext *context);

/**
 * Gets the attribute of the current node.
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static struct STX_NodeAttribute *getCurrentAttribute(struct SyntaxContext *context)
{
    return &getCurrentNode(context)->attribute;
}

static int parseQualifiedName(struct SyntaxContext *context);

/**
 * Parses a term of an expression.
 *
 * The term can be:
 *
 * - Number
 * - Qualified name
 * - expression in an unary operator.
 * - Parenthesized expression
 * - typecast expression
 *
 * Followed by an arbitrary amount of array subscript and argument lists.
 *
 *
 @verbatim
  <Term> ::=
  ( number |
  <QualifiedName>  |
  unaryOperator '(' <Expression> ')' |
  '(' <Expression> ')' |
  'cast' <Type> '(' <Expression> ')' )
  (( '[' <Expression> ']') | ( '(' <ArgumentList>? ')' ))*
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseTerm(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_TERM);

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
        if (!parseQualifiedName(context)) return 0;
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
                if (!parseArgumentList(context)) return 0;
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

/**
 * @param token subject
 *
 * @retval Nonzero for infix operator tokens
 * @retval 0 otherwise.
 */
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


struct STX_NodeAttribute *STX_getNodeAttribute(struct STX_SyntaxTreeNode *node)
{
    return &node->attribute;
}

void STX_removeNode(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *node)
{
    assert(tree == node->belongsTo);
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
}

/**
 * Parses an expression.
 *
 * Expression is a sequence of terms separated by infix operators.
 * Infix operator can be a predefined symbol or an user defined operator function.
 * Precedence is not considered in this phase. (Semantic checker will reorder the tree)
 *
 @verbatim
  <Expression> ::=
  <Term> ((infixOperator | <QualifiedName>) <Term>)*
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseExpression(struct SyntaxContext *context)
{
    descendNewNode(context, STX_EXPRESSION);
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
                if (!parseQualifiedName(context)) return 0;
            }
            else
            {
                attribute = getCurrentAttribute(context);
                attribute->operatorAttributes.type = getCurrentToken(context)->tokenType;
                acceptCurrent(context);
            }
        }
        ascendToParent(context);
        if (!parseTerm(context)) return 0;
    }
    ascendToParent(context);
    return 1;
}

/**
 * Parses a return statement.
 *
@verbatim
  <ReturnStatement> ::=
  'return' <Expression>? ';'
@endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
 * Parses an if statement.
 *
 * In this language { and } is mandatory in then and else blocks.
 * The else branch can be another if statement.
 *
 @verbatim
  <IfStatement> ::=
  'if' '(' <Expression> ')' <Block> ('else' (<Block> | <IfStatement>) )?
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
        switch (getCurrentTokenType(context))
        {
            case LEX_LEFT_BRACE:
                if (!parseBlock(context)) return 0;
            break;
            case LEX_KW_IF:
                if (!parseIfStatement(context)) return 0;
            break;
            default:
                ERR_raiseError(E_STX_BLOCK_OR_IF_STATEMENT_EXPECTED);
                return 0;
            break;
        }
    }

    ascendToParent(context);
    return 1;
}

/**
 * Parses the loop-next statement.
 *
 * This is an infinity loop language construct. Inorder to exit from it,
 * you must use break statement. It can have optional next block which
 * is executed after the statements in the loop block. The continue statement
 * jumps to the next block.
 *
 * The 'loop' block should contain what you repeat, the 'next' block should
 * contain the statements the prepare the next iteration like: i := i + 1;
 *
 @verbatim
  <LoopNextStatement> ::=
  'loop' <Block> ('next' <Block>)?
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseLoopNextStatement(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_LOOP_STATEMENT);
    attr = getCurrentAttribute(context);
    attr->loopAttributes.hasBreak = 0;

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

/**
 * Sets the current node's type.
 *
 * @param context context.
 * @param nodeType The new type of the node.
 */
static void setCurrentNodeType(struct SyntaxContext *context, enum STX_NodeType nodeType)
{
    context->tree->nodes[context->currentNodeIndex].nodeType = nodeType;
}

/**
 * Parses an expression statement.
 *
 * It can be an expression alone, but it can be an assignment too.
 * If it's used as assignment the left side of the assignment must
 * evaluate to an assignable reference the same type of the right side of
 * the expression.
 *
 @verbatim
 <ExpressionStatement> ::=
 <Expression> (':=' <Expression>)? ';'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
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

/**
 * @param [in] tokenType subject
 *
 * @retval Nonzero if it's an integer number token type.
 * @retval 0 otherwise.
 */
static int isIntegerNumberToken(enum LEX_TokenType tokenType)
{
    return
        (tokenType == LEX_DECIMAL_INTEGER) ||
        (tokenType == LEX_OCTAL_INTEGER) ||
        (tokenType == LEX_HEXA_INTEGER);
}

/**
 * Gets the integer value of an integer number token.
 *
 * @param [in] token subject.
 *
 * @return The integer value of the token.
 */
static int getIntegerValue(const struct LEX_LexerToken *token)
{
    const char *current = token->start;
    const char *end = token->start + token->length;
    int value = 0;
    int radix = 0;

    switch (token->tokenType)
    {
        case LEX_DECIMAL_INTEGER:
            radix = 10;
        break;
        case LEX_OCTAL_INTEGER:
            radix = 8;
        break;
        case LEX_HEXA_INTEGER:
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
 * Parses on case block of the switch statement.
 *
 * In this language the { } is mandatory around the commands, and
 * a 'break' and 'continue' must follow it, to show your intent.
 *
 @verbatim
  <CaseBlock> ::=
  (('case' integer_number ) | 'default') ':' <Block> ('break' | 'continue' ) ';'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */

static int parseCaseBlock(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_CASE);
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
 * Parses the switch statement.
 *
 * The compiled should optimize this construct by minimizing the checks. (like binary search)
 *
 @verbatim
  <SwitchStatement> ::=
  'switch' '(' <Expression> ')' '{' <CaseBlock>* '}'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
 * Parses break and continue statement.
 *
 * An optional integer may follow it which determines the levels of the loop or case block
 * to continue or break.
 * For exmaple if the control is in two nested loops, a 'break 2' statement would break
 * the outer loop.
 *
 @verbatim
  <BreakStatement> ::=
   'break' integer? ';'
  <ContinueStatement> ::=
   'continue' integer? ';'
 @endverbatim
 *
 * @param context context.
 * @param type the type of the token. Can be LEX_KW_BREAK or LEX_KW_CONTINUE.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseBreakContinueStatement(struct SyntaxContext *context, enum LEX_TokenType type)
{
    struct STX_NodeAttribute *attr;

    switch (type)
    {
        case LEX_KW_BREAK:
        {
            descendNewNode(context, STX_BREAK);
        }
        break;
        case LEX_KW_CONTINUE:
        {
            descendNewNode(context, STX_CONTINUE);
        }
        break;
        default:
        {
            assert(0);
        }
        break;
    }
    attr = getCurrentAttribute(context);
    attr->breakContinueAttributes.associatedNodeId = -1;
    if (!expect(context, type, E_STX_BREAK_OR_CONTINUE_EXPECTED)) return 0;
    if (isIntegerNumberToken(getCurrentTokenType(context)))
    {
        int level = getIntegerValue(getCurrentToken(context));
        attr->breakContinueAttributes.levels = level;
        acceptCurrent(context);
    }
    else
    {
        attr->breakContinueAttributes.levels = 1;
    }
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;
    ascendToParent(context);
    return 1;
}

/**
 * Parses statements.
 *
 @verbatim
  <Statement> ::=
  <ReturnStatement> |
  <IfStatement> |
  <LoopNextStatement>
  <VariableDeclaration> |
  <SimpleStatement> |
  <SwitchStatement> |
  <BreakStatement> |
  <ContinueStatement> |
  <Block>
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
        case LEX_KW_BREAK:
        case LEX_KW_CONTINUE:
            if (
                !parseBreakContinueStatement(context, getCurrentTokenType(context))
            )
            {
                return 0;
            }
        break;
        case LEX_LEFT_BRACE:
            if (!parseBlock(context)) return 0;
        break;
        default:
            if (!parseSimpleStatement(context)) return 0;
        break;
    }
    return 1;
}

/**
 * Parses a block
 *
 @verbatim
  <Block> ::=
  '{' <Statement>* '}'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
 * Parses a type prefix.
 *
 * Type prefix can be:
 *
 * - handle of: an opaque pointer to a type that cannot be dereferenced.
 *      deref() operator is invalid on an expression of this type.
 *      pointers and staticptrs can be casted to this type.
 * - buffer[N] of: an array of N elements of a type.
 *      The array subscript operator can be applied on it.
 * - pointer to: a memory address of a heap object of a type.
 *      Pointers of this type is created when allocating memory.
 * - localptr to: a memory address of a stack object.
 *      Pointers of this type is created when ref() operator is applied
 *      to a local variable in a function. Functions cannot return localptrs.
 *      Localptrs cannot be stored in structures. This is a protection against
 *      stack corruption, as references to the stack become invalid when the
 *      function returns.
 * - staticptr to: a memory address of a static read only data.
 *      Zero terminated string literals of this langage has the type
 *      'staticptr to $u8' for example. The deref() operator on a pointer of
 *      this type does not provide assignable reference. pointers can be casted
 *      to staticptrs
 *
 @verbatim
    TypePrefix ::=
        (
           (
               'handle' |
               'buffer' '[' integer_number ']'
           ) 'of'
       ) |
       (
          ('pointer' | 'localptr' | 'staticptr') 'to'
       )
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 *
 */
static int parseTypePrefix(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_TYPE_PREFIX);
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
        (token->tokenType == LEX_KW_LOCALPTR) ||
        (token->tokenType == LEX_KW_STATICPTR))
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
            case LEX_KW_STATICPTR:
                attribute->typePrefixAttributes.type = STX_TP_STATICPTR;
            break;
            default:
                assert(0); //< something is really screwed up.
        }
    }

    ascendToParent(context);
    return 1;

}

/**
 * Parses a type token. An sets the fields of the given attribute.
 *
 * @param [in] attribute The attribute to set.
 *
 * @retval Nonzero on success.
 * @retval Zero on failure, and an error is raised. E_STX_CORRUPT_TOKEN if the
 *      token is malformed.
 */
static int parseTypeToken(struct STX_NodeAttribute *attribute)
{
    const char *str = attribute->name;
    int remLength = attribute->nameLength;
    int bitCount = 0;

    if (*str != '$')
    {
        ERR_raiseError(E_STX_CORRUPT_TOKEN);
        return 0;
    }
    str++; remLength--; // accept '$'
    if (!remLength)
    {
        ERR_raiseError(E_STX_CORRUPT_TOKEN);
        return 0;
    }
    switch (*str)
    {
        case 'i':
            attribute->typeAttributes.type = STX_STT_SIGNED_INT;
        break;
        case 'u':
            attribute->typeAttributes.type = STX_STT_UNSIGNED_INT;
        break;
        case 'f':
            attribute->typeAttributes.type = STX_STT_FLOAT;
        break;
        default:
            ERR_raiseError(E_STX_CORRUPT_TOKEN);
            return 0;
        break;
    }
    str++; remLength--; // accept type letter
    attribute->typeAttributes.bitCount = 0;
    if (!remLength)
    {
        return 1;
    }
    for(; remLength; str++, remLength--)
    {
        if (*str == '_') break;
        if (('0' <= *str) && (*str <= '9'))
        {
            bitCount *= 10;
            bitCount += *str - '0';
        }
        else
        {
            ERR_raiseError(E_STX_CORRUPT_TOKEN);
            return 0;
        }
    }
    attribute->typeAttributes.bitCount = bitCount;
    if (remLength)
    {
        str++; remLength--; // accept '_'
        attribute->typeAttributes.attributeLength = remLength;
        attribute->typeAttributes.attribute = str;
    }
    else
    {
        attribute->typeAttributes.attributeLength = 0;
        attribute->typeAttributes.attribute = 0;
    }
    return 1;
}

/**
 * Parses a type. This node appear in variable, parameter, struct etc. declarations.
 *
 * Types can be another type after a type prefix, a built in type, or an user
 * defined type (struct).
 *
 @verbatim
  Type ::=
      (
           <TypePrefix> <Type>
       ) |
      built_in_type |
     identifier
 @endverbatim
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 *
 */
static int parseType(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token;
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_TYPE);
    token = getCurrentToken(context);
    attr = getCurrentAttribute(context);
    attr->typeAttributes.isPrimitive = 0;

    if (token->tokenType == LEX_BUILT_IN_TYPE)
    {
        attr->name = token->start;
        attr->nameLength = token->length;
        attr->typeAttributes.isPrimitive = 1;
        if (!parseTypeToken(attr)) return 0;
        acceptCurrent(context);
    }
    else if (token->tokenType == LEX_IDENTIFIER)
    {
        if (!parseQualifiedName(context)) return 0;
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
 * Parses variable declaration.
 *
 * One declaration can declare a single variable.
 *
 @verbatim
  <VariableDeclaration> ::= 'vardecl' <Type> identifier ( ':=' <Expression> )? ';'
 @endverbatim
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 *
 */
static int parseVariableDeclaration(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;
    struct STX_NodeAttribute *attr;

    descendNewNode(context, STX_VARDECL);
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
 * Parses a formal parameter.
 *
 * In this language formal parameters have directions:
 *
 * - in: incoming parameter. Inside the function, incoming parameters are
 *      not assignable references. Modification of them is not allowed. When the
 *      function is called, constants can be passed as incoming arguments.
 *      For assignable references. The compiler implementation may decide, whether
 *      it copies the value to the stack or copies only the address of it.
 * - ref: transitive parameter. Its an assignable reference, the function is allowed
 *      to modify it. The caller must initialize this parameter before passing it.
 *      Only assignable references can be passed as argument. The address of
 *      the referenced data is copied on the stack.
 * - out: outgoing parameter. Similar to the transitive one, but the caller don't
 *      need to initialize the variable before he passes it to the function. The
 *      function MUST assign the outgoing parameter before exiting.
 *
 * @todo Optional parameters are not supported yet. The 'nothing' keyword
 *      which is a reference of the memory address zero should be added.
 *      And a 'exist' unary operator which return nonzero if its argument is
 *      not 'nothing'.
 *
 @verbatim
  <Parameter> ::=
  ( 'in' | 'out' | 'ref') <Type> identifier
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 *
 */
static int parseParameter(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *token = getCurrentToken(context);
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_PARAMETER);

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
 * Parses a parameter list.
 *
 * Its used in function, operator function declarations.
 *
 @verbatim
  <ParameterList> ::=
  (<Parameter> (',' <Parameter>)*)?
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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

/**
 * @param type subject.
 *
 * @retval True if type is a precedence token type.
 * @retval False otherwise.
 */
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
 * Parses a function declaration or operator function declaration.
 *
 * In this language functions can have an optional cleanup block.
 * This cleanup block is executed when a return statement called.
 *
 * External functions can be referenced by the external keyword.
 * The first string is the location of the external file the function is in.
 * The second string is the format identifier. The compiler uses this
 * identifier to handle the file. For example "EPL" can be used
 * to refer other EPL modules. "DLL" can be used to refer to DLLs.
 *
 * Operator functions can be declared. Operator function can take
 * two arguments and act as an infix operator. Precedence type must
 * be provided, it can be relational, additive and multiplicative.
 *
  @verbatim
  <FunctionDeclaration> ::=
  ('function' | 'operator' precedence_type) <Type> identifier '(' <ParameterList> ')'
  ((<Block> ( 'cleanup' <Block>)?) | ('external' string ':' string ';'))
  @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseFunctionDeclaration(struct SyntaxContext *context)
{
    struct STX_NodeAttribute *attribute;
    const struct LEX_LexerToken *token;
    switch (getCurrentTokenType(context))
    {
        case LEX_KW_FUNCTION:
            descendNewNode(context, STX_FUNCTION);
            acceptCurrent(context);
        break;
        case LEX_KW_OPERATOR:
            descendNewNode(context, STX_OPERATOR_FUNCTION);
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
 * Parses a namesoace declaration
 *
 @verbatim
  <NamespaceDeclaration> ::=
  'namespace' identifier '{' <Declaration>* '}'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseNamespaceDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_NAMESPACE);

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
 * Parses a qualified name.
 *
 * Qualified name part are separated by the :: (scope resolution) operator.
 *
 @verbatim
  QualifiedName ::=
  identifier ( '::' identifier)*
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */

static int parseQualifiedName(struct SyntaxContext *context)
 {
    descendNewNode(context, STX_QUALIFIED_NAME);
    struct STX_NodeAttribute *attribute;
    const char *startPos;
    const char *lastTokenPos;
    int lastTokenLength;

    if (getCurrentTokenType(context) == LEX_IDENTIFIER)
    {
        const struct LEX_LexerToken *current = getCurrentToken(context);
        descendNewNode(context, STX_QUALIFIED_NAME_PART);
        attribute = getCurrentAttribute(context);
        lastTokenPos =
        startPos =
        attribute->name = current->start;
        lastTokenLength =
        attribute->nameLength = current->length;
        acceptCurrent(context);
        ascendToParent(context);
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
            descendNewNode(context, STX_QUALIFIED_NAME_PART);
            attribute = getCurrentAttribute(context);
            attribute->name = current->start;
            attribute->nameLength = current->length;
            lastTokenPos = current->start;
            lastTokenLength = current->length;
            acceptCurrent(context);
            ascendToParent(context);
        }
        else
        {
            ERR_raiseError(E_STX_IDENTIFIER_EXPECTED);
            return 0;
        }
    }
    // assign the complete name to the qualified name node.

    attribute = getCurrentAttribute(context);
    attribute->name = startPos;
    attribute->nameLength = (size_t)(lastTokenPos + lastTokenLength) - (size_t)(startPos);

    ascendToParent(context);
    return 1;
 }

/**
 * Parses using declaration.
 *
 * This using declaration brings the symbols to the current scope of the used namespaces.
 * But not it's subnamespaces.
 *
 @verbatim
  <UsingDeclaration> ::=
  'using' <QualifiedName> ';'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */

static int parseUsingDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_USING);

    if (!expect(context, LEX_KW_USING, E_STX_USING_EXPECTED)) return 0;
    if (!parseQualifiedName(context)) return 0;
    if (!expect(context, LEX_SEMICOLON, E_STX_SEMICOLON_EXPECTED)) return 0;

    ascendToParent(context);
    return 1;
}

/**
 * Parses a struct declaration.
 *
 * The identifier becomes the name of the new type, which can be used.
 *
 @verbatim
  StructDeclaration ::=
  'struct' identifier '{'  (<Type> identifier ';' )* '}'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseStructDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_STRUCT);

    if (!expect(context, LEX_KW_STRUCT, E_STX_STRUCT_EXPECTED)) return 0;
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
    if (!expect(context, LEX_LEFT_BRACE, E_STX_LEFT_BRACE_EXPECTED)) return 0;
    while (getCurrentTokenType(context) != LEX_RIGHT_BRACE)
    {
        descendNewNode(context, STX_FIELD);
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
 * Parses a funcptr declaration.
 *
 * This declares a function pointer type.
 *
 @verbatim
  FuncPtrDeclaration ::=
  'funcptr' <Type> identifer '(' <ParameterList> ')' ';'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parseFuncPtrDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_FUNCPTR);
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
 * Parses a platform declaration.
 *
 * Several platform names are enumerated after the 'for'.
 * If all of them are given as compiler arguments the declarations in the block
 * is considered. This construct is useful compiling the same code for different
 * platforms.
 *
 * The platform declaration does not create a namespace.
 *
 @verbatim
  PlatfromDeclaration :=
  'for' string (, string)* '{' <Declaration>* '}'
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */
static int parsePlatformDeclaration(struct SyntaxContext *context)
{
    descendNewNode(context, STX_FOR_PLATFORMS);
    {
        descendNewNode(context, STX_PLATFORM_LIST);
        {
            if (!expect(context, LEX_KW_FOR, E_STX_PLATFORM_EXPECTED)) return 0;
            descendNewNode(context, STX_PLATFORM);
            {
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
                descendNewNode(context, STX_PLATFORM);
                {
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
        }
        ascendToParent(context);
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
 * Parses a declaration.
 *
 @verbatim
  Declaration ::=
  <VariableDeclaration> |
  <FunctionDeclaration> |
  <NamespaceDeclaration> |
  <UsingDeclaration> |
  <StructDeclaration> |
  <FuncPtrDeclaration> |
  <PlatfromDeclaration>
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
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
        case LEX_KW_FOR:
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
 * Parses the module.
 *
 * This is the main function where the parsing starts.
 *
 * There module types are supported:
 *
 * - exe: executable file
 * - lib: static library
 * - dll: shared library
 *
 @verbatim
  Module ::=
  'module' {'exe' | 'lib' | 'dll' } ';'
  <Declaration>*
  'main'
  <Block>
 @endverbatim
 *
 * @param context context.
 *
 * @return Nonzero on success, zero on error.
 */

static int parseModule(struct SyntaxContext *context)
{
    const struct LEX_LexerToken *current;
    struct STX_NodeAttribute *attribute;

    descendNewNode(context, STX_MODULE);

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

#define STRINGCASE(x) case x : return #x;

const char *STX_nodeTypeToString(enum STX_NodeType nodeType)
{
    switch (nodeType)
    {
        STRINGCASE(STX_ROOT)
        STRINGCASE(STX_MODULE)
        STRINGCASE(STX_BLOCK)
        STRINGCASE(STX_DECLARATIONS)
        STRINGCASE(STX_TYPE)
        STRINGCASE(STX_VARDECL)
        STRINGCASE(STX_TYPE_PREFIX)
        STRINGCASE(STX_PARAMETER)
        STRINGCASE(STX_ARGUMENT_LIST)
        STRINGCASE(STX_FUNCTION)
        STRINGCASE(STX_EXPRESSION_STATEMENT)
        STRINGCASE(STX_TERM)
        STRINGCASE(STX_RETURN_STATEMENT)
        STRINGCASE(STX_EXPRESSION)
        STRINGCASE(STX_OPERATOR)
        STRINGCASE(STX_IF_STATEMENT)
        STRINGCASE(STX_LOOP_STATEMENT)
        STRINGCASE(STX_ASSIGNMENT)
        STRINGCASE(STX_NAMESPACE)
        STRINGCASE(STX_USING)
        STRINGCASE(STX_QUALIFIED_NAME)
        STRINGCASE(STX_QUALIFIED_NAME_PART)
        STRINGCASE(STX_STRUCT)
        STRINGCASE(STX_FIELD)
        STRINGCASE(STX_COMMENT)
        STRINGCASE(STX_FUNCPTR)
        STRINGCASE(STX_SWITCH)
        STRINGCASE(STX_CASE)
        STRINGCASE(STX_CONTINUE)
        STRINGCASE(STX_BREAK)
        STRINGCASE(STX_OPERATOR_FUNCTION)
        STRINGCASE(STX_PLATFORM)
        STRINGCASE(STX_FOR_PLATFORMS)
        STRINGCASE(STX_PARAMETER_LIST)
        STRINGCASE(STX_PLATFORM_LIST)


    }
    return "<UNKNOWN>";
}

const char * STX_PrimitiveTypeTypeToString(enum STX_PrimitiveTypeType type)
{
    switch (type)
    {
        STRINGCASE(STX_STT_FLOAT)
        STRINGCASE(STX_STT_SIGNED_INT)
        STRINGCASE(STX_STT_UNSIGNED_INT)
    }
    return "<UNKNOWN>";
}

#undef STRINGCASE

struct STX_SyntaxTreeNode *STX_getParentNode(const struct STX_SyntaxTreeNode *node)
{
    if (node->parentIndex < 0) return 0;
    return &node->belongsTo->nodes[node->parentIndex];
}

struct STX_SyntaxTreeNode *STX_getFirstChild(const struct STX_SyntaxTreeNode *node)
{
    if (node->firstChildIndex < 0) return 0;
    return &node->belongsTo->nodes[node->firstChildIndex];
}

struct STX_SyntaxTreeNode *STX_getLastChild(const struct STX_SyntaxTreeNode *node)
{
    if (node->lastChildIndex < 0) return 0;
    return &node->belongsTo->nodes[node->lastChildIndex];
}

struct STX_SyntaxTreeNode *STX_getNext(const struct STX_SyntaxTreeNode *node)
{
    if (node->nextSiblingIndex < 0) return 0;
    return &node->belongsTo->nodes[node->nextSiblingIndex];
}

struct STX_SyntaxTreeNode *STX_getPrevious(const struct STX_SyntaxTreeNode *node)
{
    if (node->previousSiblingIndex < 0) return 0;
    return &node->belongsTo->nodes[node->previousSiblingIndex];
}

void STX_initializeTreeIterator(struct STX_TreeIterator *iterator, struct STX_SyntaxTreeNode *node)
{
    iterator->current = 0;
    iterator->previous = 0;
    iterator->iteratorRoot = node;
}

struct STX_SyntaxTreeNode *STX_getNextPostorder(struct STX_TreeIterator *iterator)
{
    struct STX_SyntaxTreeNode *next;

    if (!iterator->current)
    {
        // This is the first iteration, go down to the leftmost leaf.
        next = iterator->iteratorRoot;
        while (STX_getFirstChild(next))
        {
            next = STX_getFirstChild(next);
        }
        iterator->current = next;
        return next;
    }
    // At this point this is not the first loop.
    if (iterator->iteratorRoot == iterator->current)
    {
        // We are at the iterator root. We are ready.
        return 0;
    }
    iterator->previous = iterator->current;
    next = STX_getNext(iterator->current);
    if (next)
    {
        // Next sibling exists, go down to it's first leaf
        while (STX_getFirstChild(next))
        {
            next = STX_getFirstChild(next);
        }
        iterator->current = next;
        return next;
    }
    else
    {
        // No next sibling the next is the parent node.
        next = STX_getParentNode(iterator->current);
        iterator->current = next;
        return next;
    }
}

struct STX_SyntaxTreeNode *STX_getNextPreorder(struct STX_TreeIterator *iterator)
{
    struct STX_SyntaxTreeNode *nextNode;
    struct STX_SyntaxTreeNode *parentNode;

    if (!iterator->current)
    {
        // In the first loop transverse the iterator root.
        iterator->current = iterator->iteratorRoot;
        iterator->isSkipSubTree = 0;
        return iterator->iteratorRoot;
    }

    nextNode = STX_getFirstChild(iterator->current);
    iterator->previous = iterator->current;
    if (nextNode && !iterator->isSkipSubTree)
    {
        // Current node has child nodes
        iterator->current = nextNode;
        iterator->isSkipSubTree = 0;
        return nextNode;
    }
    else
    {
        // Current node don't have child nodes.
        // So move to the next node or the next node of the parent
        parentNode = iterator->current;
        while (parentNode)
        {
            nextNode = STX_getNext(parentNode);
            if (nextNode)
            {
                iterator->current = nextNode;
                iterator->isSkipSubTree = 0;
                return nextNode;
            }
            parentNode = STX_getParentNode(parentNode);
        }
        iterator->isSkipSubTree = 0;
        return 0;
    }
}

void STX_setSkipSubtree(struct STX_TreeIterator *iterator, int skip)
{
    iterator->isSkipSubTree = skip;
}


