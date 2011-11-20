#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "error.h"

struct KeywordTokenTypePair
{
    const char *keywordText;
    enum LEX_TokenType tokenType;
};

// this array must be ordered by the keyword names!
static struct KeywordTokenTypePair keywordMapping[] =
{
    {"exe", LEX_KW_EXE},
    {"main", LEX_KW_MAIN},
    {"module", LEX_KW_MODULE}
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
            int d = strncmp(context->currentToken->start, kttp->keywordText, context->currentToken->length);
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
                    startNewToken(context, LEX_LEFTBRACE);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '}':
                    startNewToken(context, LEX_RIGHTBRACE);
                    acceptCurrent(context);
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
