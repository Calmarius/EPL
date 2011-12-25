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
    {"additive", 8, LEX_KW_ADDITIVE},
    {"break", 5, LEX_KW_BREAK},
    {"buffer", 6, LEX_KW_BUFFER},
    {"case", 4, LEX_KW_CASE},
    {"cast", 4, LEX_KW_CAST},
    {"cleanup", 7, LEX_KW_CLEANUP},
    {"continue", 8, LEX_KW_CONTINUE},
    {"default", 7, LEX_KW_DEFAULT},
    {"deref", 5, LEX_KW_DEREF},
    {"dll", 3, LEX_KW_DLL},
    {"else", 4, LEX_KW_ELSE},
    {"exe", 3, LEX_KW_EXE},
    {"external", 8, LEX_KW_EXTERNAL},
    {"funcptr", 7, LEX_KW_FUNCPTR},
    {"function", 8, LEX_KW_FUNCTION},
    {"handle", 6, LEX_KW_HANDLE},
    {"if", 2, LEX_KW_IF},
    {"in", 2, LEX_KW_IN},
    {"inc", 3, LEX_KW_INC},
    {"lib", 3, LEX_KW_LIB},
    {"localptr", 8, LEX_KW_LOCALPTR},
    {"loop", 4, LEX_KW_LOOP},
    {"main", 4, LEX_KW_MAIN},
    {"module", 6, LEX_KW_MODULE},
    {"multiplicative", 14, LEX_KW_MULTIPLICATIVE},
    {"namespace", 9, LEX_KW_NAMESPACE},
    {"neg", 3, LEX_KW_NEG},
    {"next", 4, LEX_KW_NEXT},
    {"not", 3, LEX_KW_NOT},
    {"of", 2, LEX_KW_OF},
    {"operator", 8, LEX_KW_OPERATOR},
    {"out", 3, LEX_KW_OUT},
    {"pointer", 7, LEX_KW_POINTER},
    {"ref", 3, LEX_KW_REF},
    {"relational", 10, LEX_KW_RELATIONAL},
    {"return", 6, LEX_KW_RETURN},
    {"struct", 6, LEX_KW_STRUCT},
    {"switch", 6, LEX_KW_SWITCH},
    {"to", 2, LEX_KW_TO},
    {"using", 5, LEX_KW_USING},
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

static char getCurrentCharacter(struct LexerContext *context)
{
    return *context->current;
}

static int compareBinaryString(
    const char *str1,
    int len1,
    const char *str2,
    int len2
)
{
    int len = len1 < len2 ? len1 : len2;
    int d = memcmp(str1, str2, len);
    if (!d)
    {
        d = len1 - len2;
    }
    return d;
}

static int scanIdentifier(struct LexerContext *context)
{
    if (isLetter(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    while (isAlphaNumeric(getCurrentCharacter(context)))
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
            int d = compareBinaryString(
                context->currentToken->start,
                context->currentToken->length,
                kttp->keywordText,
                kttp->keywordLength
            );
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

static int scanBuiltInType(struct LexerContext *context)
{
    if (getCurrentCharacter(context) == '$')
    {
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_LEX_IMPOSSIBLE_ERROR);
        return 0;
    }
    switch (getCurrentCharacter(context))
    {
        case 'u':
        case 'i':
        case 'f':
            acceptCurrent(context);
        break;
        default:
            ERR_raiseError(E_LEX_INVALID_BUILT_IN_TYPE_LETTER);
            return 0;
    }
    while (isDecimal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    return 1;
}

static int scanExponentialPart(struct LexerContext *context)
{
    int c;
    switch(getCurrentCharacter(context))
    {
        case '-':
        case '+':
            acceptCurrent(context);
        break;
    }
    c = 0;
    while (isDecimal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
        c++;
    }
    if (!c)
    {
        ERR_raiseError(E_LEX_MISSING_EXPONENTIAL_PART);
        return 0;
    }
    return 1;
}

static int scanFractionalPart(struct LexerContext *context)
{
    char c;
    while (isDecimal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrentCharacter(context);
    if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return scanExponentialPart(context);
    }
    return 1;
}

static int scanDecimalNumber(struct LexerContext *context)
{
    char c;
    while (isDecimal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrentCharacter(context);
    if (c == '.')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return scanFractionalPart(context);
    }
    else if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return scanExponentialPart(context);
    }
    return 1;
}

static int scanHexadecimalPart(struct LexerContext *context)
{
    while(isHexa(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    if (getCurrentCharacter(context) == '.')
    {
        ERR_raiseError(E_LEX_HEXA_FLOATING_POINT_NOT_ALLOWED);
        return 0;
    }
    return 1;
}

static int scanOctalNumber(struct LexerContext *context)
{
    char c;
    if (getCurrentCharacter(context) == '0')
    {
        acceptCurrent(context);
    }
    else
    {
        ERR_raiseError(E_LEX_IMPOSSIBLE_ERROR);
        return 0;
    }
    if ((getCurrentCharacter(context) | 0x20) == 'x')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_HEXA_NUMBER);
        return scanHexadecimalPart(context);
    }
    while (isOctal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    c = getCurrentCharacter(context);
    if (isDecimal(c))
    {
        setCurrentTokenType(context, LEX_DECIMAL_NUMBER);
        return scanDecimalNumber(context);
    }
    else if (c == '.')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return scanFractionalPart(context);
    }
    else if ((c | 0x20) == 'e')
    {
        acceptCurrent(context);
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        return scanExponentialPart(context);
    }
    return 1;
}

static int scanNumber(struct LexerContext *context)
{
    char c = getCurrentCharacter(context);
    if (c == '0')
    {
        setCurrentTokenType(context, LEX_OCTAL_NUMBER);
        if (!scanOctalNumber(context)) return 0;
    }
    else if (('1' <= c) && (c <= '9'))
    {
        if (!scanDecimalNumber(context)) return 0;
    }
    else
    {
        ERR_raiseError(E_LEX_IMPOSSIBLE_ERROR);
        return 0;
    }
    return 1;
}

static int scanString(struct LexerContext *context)
{
    char c = getCurrentCharacter(context);
    if (c != '"')
    {
        ERR_raiseError(E_LEX_QUOTE_EXPECTED);
        return 0;
    }
    acceptCurrent(context);
    while(getCurrentCharacter(context) != '"')
    {
        acceptCurrent(context);
    }
    acceptCurrent(context); // accept the last quote.
    return 1;
}

static int doTokenization(struct LexerContext *context)
{
    while (getCurrentCharacter(context))
    {
        char c = getCurrentCharacter(context);

        if (isWhitespace(c))
        {
            ignoreCurrent(context);
        }
        else if (isLetter(c))
        {
            startNewToken(context, LEX_IDENTIFIER);
            if (!scanIdentifier(context)) return 0;
            finishCurrentToken(context);
        }
        else if (isDecimal(c))
        {
            startNewToken(context, LEX_DECIMAL_NUMBER);
            if (!scanNumber(context)) return 0;
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
                    if (!scanBuiltInType(context)) return 0;
                    finishCurrentToken(context);
                break;
                case '"':
                    startNewToken(context, LEX_STRING);
                    if (!scanString(context)) return 0;
                    finishCurrentToken(context);
                break;
                case '+':
                    startNewToken(context, LEX_ADD_OPERATOR);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '-':
                    startNewToken(context, LEX_SUBTRACT_OPERATOR);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case ',':
                    startNewToken(context, LEX_COMMA);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '(':
                    startNewToken(context, LEX_LEFT_PARENTHESIS);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case ')':
                    startNewToken(context, LEX_RIGHT_PARENTHESIS);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '[':
                    startNewToken(context, LEX_LEFT_BRACKET);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '/':
                    startNewToken(context, LEX_DIVISION_OPERATOR);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '*')
                    {
                        setCurrentTokenType(context, LEX_BLOCK_COMMENT);
                        acceptCurrent(context);
                        if (getCurrentCharacter(context) == '*')
                        {
                            setCurrentTokenType(context, LEX_DOCUMENTATION_BLOCK_COMMENT);
                            acceptCurrent(context);
                        }
                        for (;;)
                        {
                            if (getCurrentCharacter(context) == '*')
                            {
                                acceptCurrent(context);
                                if (getCurrentCharacter(context) == '/')
                                {
                                    acceptCurrent(context);
                                    break;
                                }
                            }
                            else
                            {
                                acceptCurrent(context);
                            }
                        }
                    }
                    else if (getCurrentCharacter(context) == '/')
                    {
                        setCurrentTokenType(context, LEX_EOL_COMMENT);
                        acceptCurrent(context);
                        if (getCurrentCharacter(context) == '/')
                        {
                            setCurrentTokenType(context, LEX_DOCUMENTATION_EOL_COMMENT);
                            acceptCurrent(context);
                        }
                        if (getCurrentCharacter(context) == '<')
                        {
                            setCurrentTokenType(context, LEX_DOCUMENTATION_EOL_BACK_COMMENT);
                            acceptCurrent(context);
                        }
                        while ((getCurrentCharacter(context) != '\n') && (getCurrentCharacter(context) != '\r'))
                        {
                            acceptCurrent(context);
                        }
                    }
                    finishCurrentToken(context);
                break;
                case ']':
                    startNewToken(context, LEX_RIGHT_BRACKET);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '*':
                    startNewToken(context, LEX_MULTIPLY_OPERATOR);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case '.':
                    startNewToken(context, LEX_PERIOD);
                    acceptCurrent(context);
                    finishCurrentToken(context);
                break;
                case  '>':
                    startNewToken(context, LEX_GREATER_THAN);
                    acceptCurrent(context);
                    switch (getCurrentCharacter(context))
                    {
                        case '=':
                        {
                            setCurrentTokenType(context, LEX_GREATER_EQUAL_THAN);
                            acceptCurrent(context);
                        }
                        break;
                        case '>':
                        {
                            setCurrentTokenType(context, LEX_SHIFT_RIGHT);
                            acceptCurrent(context);
                        }
                        break;
                    }
                    finishCurrentToken(context);
                break;
                case  '<':
                    startNewToken(context, LEX_LESS_THAN);
                    acceptCurrent(context);
                    switch (getCurrentCharacter(context))
                    {
                        case '=':
                        {
                            setCurrentTokenType(context, LEX_LESS_EQUAL_THAN);
                            acceptCurrent(context);
                        }
                        break;
                        case '<':
                        {
                            setCurrentTokenType(context, LEX_SHIFT_LEFT);
                            acceptCurrent(context);
                        }
                        break;
                    }
                    finishCurrentToken(context);
                break;
                case '=':
                    startNewToken(context, LEX_EQUAL);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '=')
                    {
                        acceptCurrent(context);
                    }
                    else
                    {
                        ERR_raiseError(E_LEX_INVALID_OPERATOR);
                        return 0;
                    }
                    finishCurrentToken(context);
                break;
                case '!':
                    startNewToken(context, LEX_NOT_EQUAL);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '=')
                    {
                        acceptCurrent(context);
                    }
                    else
                    {
                        ERR_raiseError(E_LEX_INVALID_OPERATOR);
                        return 0;
                    }
                    finishCurrentToken(context);
                break;
                case ':':
                    startNewToken(context, LEX_COLON);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '=')
                    {
                        setCurrentTokenType(context, LEX_ASSIGN_OPERATOR);
                        acceptCurrent(context);
                    }
                    else if (getCurrentCharacter(context) == ':')
                    {
                        setCurrentTokenType(context, LEX_SCOPE_SEPARATOR);
                        acceptCurrent(context);
                    }
                    finishCurrentToken(context);
                break;
                default:
                    ERR_raiseError(E_LEX_INVALID_CHARACTER);
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

    startNewToken(&lexerContext, LEX_SPEC_EOF);
    finishCurrentToken(&lexerContext);

    lexerResult.tokenCount = lexerContext.tokenCount;
    lexerResult.linePos = getCurrentLine(&lexerContext);
    lexerResult.columnPos = lexerContext.currentColumn;

    return lexerResult;
}
