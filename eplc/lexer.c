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
    {"for", 3, LEX_KW_FOR},
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
    {"staticptr", 9, LEX_KW_STATICPTR},
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
    int stringsAllocated;
    int currentStringLength;
    int currentStringAllocated;
    char *currentString;
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
    if (getCurrentCharacter(context) == '_' )
    {
        acceptCurrent(context);
        while (isLetter(getCurrentCharacter(context)))
        {
            acceptCurrent(context);
        }
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
                    if (isDecimal(getCurrentCharacter(context)))
                    {
                        setCurrentTokenType(context, LEX_DECIMAL_NUMBER);
                        if (!scanNumber(context)) return 0;
                    }
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
                case '#':
                    startNewToken(context, LEX_CHARACTER);
                    acceptCurrent(context);
                    while (isDecimal(getCurrentCharacter(context)))
                    {
                        acceptCurrent(context);
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
    int i;
    for (i = 0; i < lexerResult->stringCount; i++)
    {
        free(lexerResult->strings[i]);
    }
    free(lexerResult->strings);
    free(lexerResult->tokens);
}

static void mergeAdjacentStrings(struct LexerContext *context)
{
    int i, j = 0;
    int inSeriesOfStrings = 0;
    struct LEX_LexerToken *stringToken;
    struct LEX_LexerResult *result = context->result;
    for (i = 0; i < context->tokenCount; i++)
    {
        struct LEX_LexerToken *token = &result->tokens[i];
        struct LEX_LexerToken *previousToken;
        switch (token->tokenType)
        {
            case LEX_STRING:
            case LEX_CHARACTER:
            {
                if (!inSeriesOfStrings)
                {
                    inSeriesOfStrings = 1;
                    stringToken = token;
                }
                else
                {
                    token->tokenType = LEX_SPEC_DELETED;
                }
            }
            break;
            default:
                if (inSeriesOfStrings)
                {
                    inSeriesOfStrings = 0;
                    stringToken->endColumn = previousToken->endColumn;
                    stringToken->endLine = previousToken->endLine;
                    stringToken->length =
                        (size_t)(previousToken->start + previousToken->length) -
                        (size_t)(stringToken->start);
                }
        }
        previousToken = token;
    }
    i = 0;
    j = 0;
    for (i = 0; i < context->tokenCount; i++)
    {
        struct LEX_LexerToken *token = &result->tokens[i];
        if (token->tokenType != LEX_SPEC_DELETED)
        {
            result->tokens[j++] = *token;
        }
    }
    context->tokenCount = j;
}

static void createBinaryString(struct LexerContext *context)
{
    assert(!context->currentStringAllocated);
    context->currentStringAllocated = 20;
    context->currentString = malloc(context->currentStringAllocated * sizeof(char));
    context->currentStringLength = 0;
}

static void finalizeBinaryString(struct LexerContext *context, const char **string, int *length)
{
    struct LEX_LexerResult *result = context->result;

    assert(context->currentStringAllocated);
    assert(string);
    assert(length);
    if (result->stringCount == context->stringsAllocated)
    {
        if (context->stringsAllocated)
        {
            context->stringsAllocated <<= 1;
        }
        else
        {
            context->stringsAllocated = 20;
        }
        result->strings = realloc(result->strings, context->stringsAllocated * sizeof(*result->strings));
    }
    result->strings[result->stringCount++] = context->currentString;
    context->currentStringAllocated = 0;
    *string = context->currentString;
    *length = context->currentStringLength;

}

static void addToBinaryString(struct LexerContext *context, char c)
{
    assert(context->currentStringAllocated);

    if (context->currentStringLength == context->currentStringAllocated)
    {
        context->currentStringAllocated <<= 1;
        context->currentString = realloc(context->currentString, context->currentStringAllocated * sizeof(*context->currentString));
    }
    context->currentString[context->currentStringLength++] = c;
}

static void addUtf8CharacterToBinaryString(struct LexerContext *context, int ch)
{
    if (ch < (1 << 7) )
    {
        addToBinaryString(context, (char)ch);
    }
    else if (ch < (1 << 11))
    {
        addToBinaryString(context, 0xC0 | ((ch & 0x000007C0) >> 6) );
        addToBinaryString(context, 0x80 | (ch & 0x0000003F) );
    }
    else if (ch < (1 << 16))
    {
        addToBinaryString(context, 0xE0 | ((ch & 0x0000F000) >> 12) );
        addToBinaryString(context, 0x80 | ((ch & 0x00000FC0) >> 6) );
        addToBinaryString(context, 0x80 | (ch & 0x0000003F) );
    }
    else if (ch < (1 << 21))
    {
        addToBinaryString(context, 0xF0 | ((ch & 0x00070000) >> 18) );
        addToBinaryString(context, 0x80 | ((ch & 0x0003F000) >> 12) );
        addToBinaryString(context, 0x80 | ((ch & 0x00000FC0) >> 6) );
        addToBinaryString(context, 0x80 | (ch & 0x0000003F) );
    }
    else if (ch < (1 << 26))
    {
        addToBinaryString(context, 0xF8 | ((ch & 0x03000000) >> 24) );
        addToBinaryString(context, 0x80 | ((ch & 0x00FC0000) >> 18) );
        addToBinaryString(context, 0x80 | ((ch & 0x0003F000) >> 12) );
        addToBinaryString(context, 0x80 | ((ch & 0x00000FC0) >> 6) );
        addToBinaryString(context, 0x80 | (ch & 0x0000003F) );
    }
    else if (ch < (1 << 31))
    {
        addToBinaryString(context, 0xFC | ((ch & 0xC0000000) >> 30) );
        addToBinaryString(context, 0x80 | ((ch & 0x3F000000) >> 24) );
        addToBinaryString(context, 0x80 | ((ch & 0x00FC0000) >> 18) );
        addToBinaryString(context, 0x80 | ((ch & 0x0003F000) >> 12) );
        addToBinaryString(context, 0x80 | ((ch & 0x00000FC0) >> 6) );
        addToBinaryString(context, 0x80 | (ch & 0x0000003F) );
    }
}

static void createBinaryStrings(struct LexerContext *context)
{
    int i;
    for (i = 0; i < context->tokenCount; i++)
    {
        struct LEX_LexerToken *token;
        const char *c;
        int inString = 0;
        int inCharacter = 0;
        int characterCode = 0;

        token = &context->result->tokens[i];
        if (token->tokenType != LEX_STRING) continue;
        createBinaryString(context);
        for (c = token->start; c != token->start + token->length; c++)
        {
            if (*c == '\"')
            {
                inString = !inString;
                continue;
            }
            if (inString)
            {
                addToBinaryString(context, *c);
            }
            else
            {
                if (inCharacter)
                {
                    if (('0' <= *c) && (*c <= '9'))
                    {
                        characterCode *= 10;
                        characterCode += *c - '0';
                    }
                    else
                    {
                        inCharacter = 0;
                        addUtf8CharacterToBinaryString(context, characterCode);
                    }
                }
                if (!inCharacter)
                {
                    if (*c == '#')
                    {
                        inCharacter = 1;
                        characterCode = 0;
                    }
                }
            }
        }
        if (inCharacter)
        {
            addUtf8CharacterToBinaryString(context, characterCode);
        }
        finalizeBinaryString(context, &token->start, &token->length);

    }
}

struct LEX_LexerResult LEX_tokenizeString(const char *code)
{
    struct LEX_LexerResult lexerResult;
    struct LexerContext lexerContext;

    lexerResult.tokenCount = 0;
    lexerResult.stringCount = 0;
    lexerResult.tokens = 0;
    lexerResult.strings = 0;

    lexerContext.current = code;
    lexerContext.result = &lexerResult;
    lexerContext.tokensAllocated = 0;
    lexerContext.tokenCount = 0;
    lexerContext.currentColumn = 1;
    lexerContext.lfCount = 1;
    lexerContext.crCount = 1;
    lexerContext.currentToken = 0;
    lexerContext.stringsAllocated = 0;
    lexerContext.currentStringAllocated = 0;
    lexerContext.currentStringLength = 0;

    doTokenization(&lexerContext);

    mergeAdjacentStrings(&lexerContext);

    createBinaryStrings(&lexerContext);

    startNewToken(&lexerContext, LEX_SPEC_EOF);
    finishCurrentToken(&lexerContext);

    lexerResult.tokenCount = lexerContext.tokenCount;
    lexerResult.linePos = getCurrentLine(&lexerContext);
    lexerResult.columnPos = lexerContext.currentColumn;

    return lexerResult;
}
