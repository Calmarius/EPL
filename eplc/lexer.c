#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "error.h"

struct KeywordTokenTypePair
{
    const char *keywordText;
    int keywordLength;
    enum LEX_TokenType tokenType;
};

// this array must be ordered by the keyword names!
static struct KeywordTokenTypePair keywordMapping[] =
{
    {"else", 4, LEX_KW_ELSE},
    {"exe", 3, LEX_KW_EXE},
    {"if", 2, LEX_KW_IF},
    {"inc", 3, LEX_KW_INC},
    {"loop", 4, LEX_KW_LOOP},
    {"main", 4, LEX_KW_MAIN},
    {"module", 6, LEX_KW_MODULE},
    {"next", 4, LEX_KW_NEXT},
    {"vardecl", 7, LEX_KW_VARDECL},
};

struct LexerContext
{
    const char *current;
    struct LEX_LexerResult *result;
    int tokensAllocated;
    int tokenCount;
    int currentColumn;
    int lfCount;
    int crCount;
    struct LEX_LexerToken *currentToken;
};

static int getCurrentLine(struct LexerContext *lexerContext)
{
    return
        (lexerContext->lfCount > lexerContext->crCount ?
            lexerContext->lfCount :
            lexerContext->crCount);
}

static void startNewToken(struct LexerContext *lexerContext, enum LEX_TokenType type)
{
    assert(!lexerContext->currentToken);
    if (lexerContext->tokenCount == lexerContext->tokensAllocated)
    {
        if (lexerContext->tokensAllocated)
        {
            lexerContext->tokensAllocated *= 2;
        }
        else
        {
            lexerContext->tokensAllocated = 10;
        }
        lexerContext->result->tokens =
            realloc(
                lexerContext->result->tokens,
                lexerContext->tokensAllocated * sizeof(struct LEX_LexerToken));
    }

    lexerContext->currentToken =
        &lexerContext->result->tokens[lexerContext->tokenCount++];
    lexerContext->currentToken->beginLine = getCurrentLine(lexerContext);
    lexerContext->currentToken->beginColumn = lexerContext->currentColumn;
    lexerContext->currentToken->tokenType = type;
    lexerContext->currentToken->start = lexerContext->current;
    lexerContext->currentToken->length = 0;
}

static void finishCurrentToken(struct LexerContext *lexerContext)
{
    assert(lexerContext->currentToken);

    lexerContext->currentToken->endLine = getCurrentLine(lexerContext);
    lexerContext->currentToken->endColumn = lexerContext->currentColumn;
    lexerContext->currentToken = 0;
}

static void setCurrentTokenType(
    struct LexerContext *lexerContext,
    enum LEX_TokenType newType)
{
    assert(lexerContext->currentToken);
    lexerContext->currentToken->tokenType = newType;
}

static int isLetter(char c)
{
    return
        (('a' <= c) && (c <= 'z')) ||
        (('A' <= c) && (c <= 'Z')) ||
        (c == '_');
}

static int isOctal(char c)
{
    return
        (('0' <= c) && (c <= '7'));
}

static int isDecimal(char c)
{
    return
        (('0' <= c) && (c <= '9'));
}

static int isHexa(char c)
{
    return
        (('0' <= c) && (c <= '9')) ||
        (('A' <= c) && (c <= 'F')) ||
        (('a' <= c) && (c <= 'f'));
}

static int isWhitespace(char c)
{
    return
        (c == ' ') ||
        (c == '\n') ||
        (c == '\r') ||
        (c == '\t') ||
        (c == '\v') ||
        (c == '\f');
}

static int isAlphaNumeric(char c)
{
    return isLetter(c) || isDecimal(c);
}

static void advance(struct LexerContext *context)
{
    char c = *context->current;
    context->currentColumn++;

    if (c == '\r')
    {
        context->crCount++;
        context->currentColumn = 1;
    }
    if (c == '\n')
    {
        context->lfCount++;
        context->currentColumn = 1;
    }
    context->current++;
}

static void acceptCurrent(struct LexerContext *context)
{
    assert(context->currentToken);
    context->currentToken->length++;
    advance(context);
}

static void ignoreCurrent(struct LexerContext *context)
{
    advance(context);
}

static char getCurrent(struct LexerContext *context)
{
    return *context->current;
}

static int parseIdentifier(struct LexerContext *context)
{
    if (isLetter(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    while (isAlphaNumeric(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    // set token type if the current token is a keyword, do binary search in keyword.
    {
        const int N = sizeof(keywordMapping) / sizeof(keywordMapping[0]);
        int left = 0;
        int right = N - 1;
        while (right >= left)
        {
            int middle = (left + right) >> 1;
            assert(middle >= 0);
            assert(middle < N);
            struct KeywordTokenTypePair *kttp = &keywordMapping[middle];
            int d = strncmp(
                context->currentToken->start,
                kttp->keywordText,
                kttp->keywordLength);
            if (d < 0)
            {
                right = middle - 1;
            }
            else if (d > 0)
            {
                left = middle + 1;
            }
            else
            {
                setCurrentTokenType(context, kttp->tokenType);
                break;
            }
        }
    }


    return 1;
}

static int parseBuiltInType(struct LexerContext *context)
{
    if (getCurrent(context) == '$')
    {
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_IMPOSSIBLE_ERROR);
        return 0;
    }
    switch (getCurrent(context))
    {
        case 'u':
        case 'i':
        case 'f':
            acceptCurrent(context);
        break;
        default:
            ERR_raiseError(E_INVALID_BUILT_IN_TYPE_LETTER);
            return 0;
    }
    while (isDecimal(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    return 1;
}

static int parseExponentialPart(struct LexerContext *context)
{
    int c;
    switch(getCurrent(context))
    {
        case '-':
        case '+':
            acceptCurrent(context);
        break;
    }
    c = 0;
    while (isDecimal(getCurrent(context)))
    {
        acceptCurrent(context);
        c++;
    }
    if (!c)
    {
        ERR_raiseError(E_MISSING_EXPONENTIAL_PART);
        return 0;
    }
    return 1;
}

static int parseFractionalPart(struct LexerContext *context)
{
    char c;
    while (isDecimal(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrent(context);
    if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return parseExponentialPart(context);
    }
    return 1;
}

static int parseDecimalNumber(struct LexerContext *context)
{
    char c;
    while (isDecimal(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrent(context);
    if (c == '.')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return parseFractionalPart(context);
    }
    else if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return parseExponentialPart(context);
    }
    return 1;
}

static int parseHexadecimalPart(struct LexerContext *context)
{
    while(isHexa(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    if (getCurrent(context) == '.')
    {
        ERR_raiseError(E_HEXA_FLOATING_POINT_NOT_ALLOWED);
        return 0;
    }
    return 1;
}

static int parseOctalNumber(struct LexerContext *context)
{
    char c;
    if (getCurrent(context) == '0')
    {
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_IMPOSSIBLE_ERROR);
        return 0;
    }
    if ((getCurrent(context) | 0x20) == 'x')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_HEXA_NUMBER);
        return parseHexadecimalPart(context);
    }
    while (isOctal(getCurrent(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrent(context);
    if (isDecimal(c))
    {
        setCurrentTokenType(context, LEX_DECIMAL_NUMBER);
        return parseDecimalNumber(context);
    }
    else if (c == '.')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return parseFractionalPart(context);
    }
    else if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return parseExponentialPart(context);
    }
    return 1;
}

static int parseNumber(struct LexerContext *context)
{
    char c = getCurrent(context);
    if (c == '0')
    {
        setCurrentTokenType(context, LEX_OCTAL_NUMBER);
        if (!parseOctalNumber(context)) return 0;
    }
    else if (('1' <= c) && (c <= '9'))
    {
        if (!parseDecimalNumber(context)) return 0;
    }
    else
    {
        ERR_raiseError(E_IMPOSSIBLE_ERROR);
        return 0;
    }
    return 1;
}


static int doTokenization(struct LexerContext *context)
{
    while (getCurrent(context))
    {
        char c = getCurrent(context);

        if (isWhitespace(c))
        {
            ignoreCurrent(context);
        }
        else if (isLetter(c))
        {
            startNewToken(context, LEX_IDENTIFIER);
            if (!parseIdentifier(context)) return 0;
            finishCurrentToken(context);
        }
        else if (isDecimal(c))
        {
            startNewToken(context, LEX_DECIMAL_NUMBER);
            if (!parseNumber(context)) return 0;
            finishCurrentToken(context);
        }
        else
        {
            switch (c)
            {
                case ';':
                    startNewToken(context, LEX_SEMICOLON);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '{':
                    startNewToken(context, LEX_LEFT_BRACE);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '}':
                    startNewToken(context, LEX_RIGHT_BRACE);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '$':
                    startNewToken(context, LEX_BUILT_IN_TYPE);
                    if (!parseBuiltInType(context)) return 0;
                    finishCurrentToken(context);
                break;
                case '+':
                    startNewToken(context, LEX_ADD_OPERATOR);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '(':
                    startNewToken(context, LEX_LEFT_BRACE);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case ')':
                    startNewToken(context, LEX_RIGHT_BRACE);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '[':
                    startNewToken(context, LEX_LEFT_BRACKET);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case ']':
                    startNewToken(context, LEX_RIGHT_BRACKET);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '=':
                    startNewToken(context, LEX_EQUALITY);
                    acceptCurrent(context);
                    if (getCurrent(context) == '=')
                    {
                        acceptCurrent(context);
                    }
                    else
                    {
                        ERR_raiseError(E_INVALID_OPERATOR);
                        return 0;
                    }
                    finishCurrentToken(context);
                break;
                case ':':
                    startNewToken(context, LEX_ASSIGN_OPERATOR);
                    acceptCurrent(context);
                    if (getCurrent(context) == '=')
                    {
                        acceptCurrent(context);
                    }
                    else
                    {
                        ERR_raiseError(E_INVALID_OPERATOR);
                        return 0;
                    }
                    finishCurrentToken(context);
                break;
                default:
                    ERR_raiseError(E_INVALID_CHARACTER);
                    return 0;
            }
        }
    }
    return 1;
}

void LEX_cleanUpLexerResult(struct LEX_LexerResult *lexerResult)
{
    free(lexerResult->tokens);
}

struct LEX_LexerResult LEX_tokenizeString(const char *code)
{
    struct LEX_LexerResult lexerResult;
    struct LexerContext lexerContext;

    lexerResult.tokenCount = 0;
    lexerResult.tokens = 0;

    lexerContext.current = code;
    lexerContext.result = &lexerResult;
    lexerContext.tokensAllocated = 0;
    lexerContext.tokenCount = 0;
    lexerContext.currentColumn = 1;
    lexerContext.lfCount = 1;
    lexerContext.crCount = 1;
    lexerContext.currentToken = 0;

    doTokenization(&lexerContext);

    lexerResult.tokenCount = lexerContext.tokenCount;
    lexerResult.linePos = getCurrentLine(&lexerContext);
    lexerResult.columnPos = lexerContext.currentColumn;

    return lexerResult;
}
