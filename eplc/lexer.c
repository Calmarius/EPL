/**
 * Lexer module
 *
 * Generates tokens from the source code.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "error.h"

/**
 * A struct decribing a keyword-token mapping.
 */
struct KeywordTokenTypePair
{
    /// Pointer to the keyword text.
    const char *keywordText;
    /// Length of the keyword text.
    int keywordLength;
    /// The mapped token type.
    enum LEX_TokenType tokenType;
};

/**
 * Array of keyword-token mappings.
 *
 * IMPORTANT: binary search is performed on this array to find the token.
 *      Keep the lexical order of the keywords!
 */
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

/**
 * This struct stores the context of the lexer.
 *
 * The lfCount and crCount used to count lines. The higher one is the current line.
 * Line endings in most platforms are LF, CR, CR LF or  LF CR etc. So this trick is useful in
 * most of the time.
 *
 *
 */
struct LexerContext
{
    const char *current; ///< pointer to the current char.
    /// The lexer result which is populated during the scanning.
    struct LEX_LexerResult *result;
    int tokensAllocated; ///< Allocated tokens. (needed in a dynamic array.)
    int tokenCount; ///< Count of tokens.  (needed for a dynamic array.)
    int currentColumn; ///< The current column of the current charater.
    int lfCount; ///< Count of LF characters.
    int crCount; ///< Count of CR characters.
    struct LEX_LexerToken *currentToken; ///< Current token being scanned.
    int stringsAllocated; ///< Count of allocated binary string
    int currentStringLength; ///< Length of the current binary string.
    int currentStringAllocated; ///< Allocated length of the current string.
    char *currentString; ///< Pointer to the current string.
};

/**
 * Returns the current line the scanner is in.
 *
 * @param lexerContext context.
 *
 * @return current line.
 */
static int getCurrentLine(struct LexerContext *lexerContext)
{
    return
        (lexerContext->lfCount > lexerContext->crCount ?
            lexerContext->lfCount :
            lexerContext->crCount);
}

/**
 * Starts a new token.
 *
 * @param lexerContext context.
 * @param type The type of the new token.
 */
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

/**
 * Finishes the current token.
 *
 * @param lexerContext context
 */
static void finishCurrentToken(struct LexerContext *lexerContext)
{
    assert(lexerContext->currentToken);

    lexerContext->currentToken->endLine = getCurrentLine(lexerContext);
    lexerContext->currentToken->endColumn = lexerContext->currentColumn;
    lexerContext->currentToken = 0;
}

/**
 * Sets the type of the current token.
 *
 * @param lexerContext context.
 * @param newType The new type of the token.
 */
static void setCurrentTokenType(
    struct LexerContext *lexerContext,
    enum LEX_TokenType newType)
{
    assert(lexerContext->currentToken);
    lexerContext->currentToken->tokenType = newType;
}

/**
 * Notation: LETTER
 *
 * @param c character to check.
 *
 * @return Nonzero if the character is a latin letter or underscore, zero otherwise.
 */
static int isLetter(char c)
{
    return
        (('a' <= c) && (c <= 'z')) ||
        (('A' <= c) && (c <= 'Z')) ||
        (c == '_');
}

/**
 * Notation: OCTAL
 *
 * @param c character to check
 *
 * @return Nonzero if the character is an octal digit, zero otherwise.
 */
static int isOctal(char c)
{
    return
        (('0' <= c) && (c <= '7'));
}

/**
 * Notation: DECIMAL
 *
 * @param c character to check
 *
 * @return Nonzero if the character is an decimal digit, zero otherwise.
 */
static int isDecimal(char c)
{
    return
        (('0' <= c) && (c <= '9'));
}

/**
 * Notation: HEXA
 *
 * @param c character to check
 *
 * @return Nonzero if the character is a hexadecimal digit, zero otherwise.
 *      lower and uppercase letters 10+ values allowed.
 */
static int isHexa(char c)
{
    return
        (('0' <= c) && (c <= '9')) ||
        (('A' <= c) && (c <= 'F')) ||
        (('a' <= c) && (c <= 'f'));
}

/**
 * Notation: SPACE
 *
 * @param c character to check
 *
 * @return Nonzero if the character is a white space character:
 *      - space
 *      - tab
 *      - line feed
 *      - vertical tab
 *      - form feed
 *      - carriage return.
 */
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

/**
 * Notation: ALNUM = LETTER | DECIMAL
 *
 * @param c character to check.
 *
 * @return Nonzero if the character is a letter or a decimal digit, zero otherwise.
 */
static int isAlphaNumeric(char c)
{
    return isLetter(c) || isDecimal(c);
}

/**
 * Moves the cursor forward with one character. Sets crCount, lfCount and currentColumn.
 *
 * @param context context.
 */
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

/**
 * Accepts current character and advances to the next character.
 *
 * @param context context
 */
static void acceptCurrent(struct LexerContext *context)
{
    assert(context->currentToken);
    context->currentToken->length++;
    advance(context);
}
/**
 * Ignores current character and moves the cursor to the next.
 *
 * @param context context
 */
static void ignoreCurrent(struct LexerContext *context)
{
    advance(context);
}

/**
 * @param context context.
 *
 * @return The current character.
 */
static char getCurrentCharacter(struct LexerContext *context)
{
    return *context->current;
}

/**
 * Compares two non-zero terminated strings.
 *
 * @param str1,len1 The pointer to the first string and its length.
 * @param str2,len2 The pointer to the second string and its length.
 *
 * @return
 *      - Zero if the two strings are equal.
 *      - Negative if the first is less.
 *      - Positive if the first is bigger.
 */
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

/**
 * Scans indetifier. It may change the current token type, if keyword is found.
 * identifier ::= LETTER ALNUM*
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */
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

/**
 * Scans primitive type. (eg. $i32)
 * built_in_type ::= '$' ('u' | 'i' | 'f') DECIMAL* ( '_' LETTER* )?
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */
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

/**
 * Scans a number.
 *      octal_integer ::= '0' OCTAL*
 *      hexa_integer ::= '0' 'x' HEXA*
 *      decimal_integer ::= DECIMAL+
 *      float_number ::= DECIMAL+ ('.' DECIMAL+)? ( ('e' | 'E') ('+' | '-') DECIMAL+)?
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */

static int scanNumber(struct LexerContext *context)
{
    char c = getCurrentCharacter(context);
    int octalTried = 0;

    if (c == '0')
    {
        // This can be an octal or a hexadecimal number at this point.
        acceptCurrent(context);
        octalTried = 1;
        if (getCurrentCharacter(context) == 'x')
        {
            // Now we are sure this is a hexadecimal character.
            acceptCurrent(context);
            setCurrentTokenType(context, LEX_HEXA_INTEGER);
            // Scan one or more hexadecimal characters.
            if (!isHexa(getCurrentCharacter(context)))
            {
                ERR_raiseError(E_LEX_INVALID_HEXA_LITERAL);
                return 0;
            }
            while (isHexa(getCurrentCharacter(context)))
            {
                acceptCurrent(context);
            }
            return 1;
        }
        // Its not hexadecimal its an octal.
        // So read zero or more octal numbers.
        setCurrentTokenType(context, LEX_OCTAL_INTEGER);
        while (isOctal(getCurrentCharacter(context)))
        {
            acceptCurrent(context);
        }
        // If the next character cannot be a part of a decimal integer
        // or floating point number. The current token is accepted as an octal number.
        c = getCurrentCharacter(context);
        if
        (
            !isDecimal(c) &&
            (c != '.') &&
            ((c | 0x20) != 'e')
        )
        {
            return 1;
        }
    }
    // At this point the number is decimal at least.
    setCurrentTokenType(context, LEX_DECIMAL_INTEGER);
    // read one or more decimal digits. If we tried octal mode we already read
    // at least one 0.
    if (!octalTried && !isDecimal(getCurrentCharacter(context)))
    {
        ERR_raiseError(E_LEX_INVALID_DECIMAL_NUMBER);
        return 0;
    }
    while (isDecimal(getCurrentCharacter(context)))
    {
        acceptCurrent(context);
    }
    if (getCurrentCharacter(context) == '.')
    {
        // Period read, fraction part coming with at least one decimal number
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        acceptCurrent(context);
        if (!isDecimal(getCurrentCharacter(context)))
        {
            ERR_raiseError(E_LEX_INVALID_DECIMAL_NUMBER);
            return 0;
        }
        while (isDecimal(getCurrentCharacter(context)))
        {
            acceptCurrent(context);
        }
    }
    if ((getCurrentCharacter(context) | 0x20) == 'e')
    {
        // E read. Exponential part coming.
        setCurrentTokenType(context, LEX_FLOAT_NUMBER);
        acceptCurrent(context);
        // accept an optional sign.
        switch (getCurrentCharacter(context))
        {
            case '-':
            case '+':
                acceptCurrent(context);
            break;
            default:
            break;
        }
        // accept at least one decimal numbers.
        if (!isDecimal(getCurrentCharacter(context)))
        {
            ERR_raiseError(E_LEX_INVALID_DECIMAL_NUMBER);
            return 0;
        }
        while (isDecimal(getCurrentCharacter(context)))
        {
            acceptCurrent(context);
        }
    }
    return 1;
}

/**
 * Scans a string.
 * string ::= '"' non-"* '"'
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */
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

/**
 * The main function that does the tokenization.
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */
static int doTokenization(struct LexerContext *context)
{
    while (getCurrentCharacter(context))
    {
        char c = getCurrentCharacter(context);

        if (isWhitespace(c))
        {
            // Ignore any whitespace.
            ignoreCurrent(context);
        }
        else if (isLetter(c))
        {
            // If read letter try to scan an identifier.
            startNewToken(context, LEX_IDENTIFIER);
            if (!scanIdentifier(context)) return 0;
            finishCurrentToken(context);
        }
        else if (isDecimal(c))
        {
            // If read an integer, try to scan a decimal number.
            startNewToken(context, LEX_DECIMAL_INTEGER);
            if (!scanNumber(context)) return 0;
            finishCurrentToken(context);
        }
        else
        {
            // Special character handling goes here.
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
                        // If a decimal digit follows directly the -. We read a negative number.
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
                    // '/' means division operator
                    startNewToken(context, LEX_DIVISION_OPERATOR);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '*')
                    {
                        // '/*' means the beginning of the block comment.
                        setCurrentTokenType(context, LEX_BLOCK_COMMENT);
                        acceptCurrent(context);
                        if (getCurrentCharacter(context) == '*')
                        {
                            // '/**' is a documentation block comment.
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
                                    // '*/' character sequence closes the comment.
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
                        // '//' starts an EOL comment.
                        setCurrentTokenType(context, LEX_EOL_COMMENT);
                        acceptCurrent(context);
                        if (getCurrentCharacter(context) == '/')
                        {
                            // '///' starts an EOL documentation comment
                            setCurrentTokenType(context, LEX_DOCUMENTATION_EOL_COMMENT);
                            acceptCurrent(context);
                        }
                        if (getCurrentCharacter(context) == '<')
                        {
                            // '//<' starts an EOL documentation back comment.
                            setCurrentTokenType(context, LEX_DOCUMENTATION_EOL_BACK_COMMENT);
                            acceptCurrent(context);
                        }
                        while ((getCurrentCharacter(context) != '\n') && (getCurrentCharacter(context) != '\r'))
                        {
                            // read characters until we reached end of the line.
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
                            // >=
                            setCurrentTokenType(context, LEX_GREATER_EQUAL_THAN);
                            acceptCurrent(context);
                        }
                        break;
                        case '>':
                        {
                            // >>
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
                            // <=
                            setCurrentTokenType(context, LEX_LESS_EQUAL_THAN);
                            acceptCurrent(context);
                        }
                        break;
                        case '<':
                        {
                            // <<
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
                        // == equality
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
                        // #32 is for space for example
                        acceptCurrent(context);
                    }
                    finishCurrentToken(context);
                break;
                case '!':
                    startNewToken(context, LEX_NOT_EQUAL);
                    acceptCurrent(context);
                    if (getCurrentCharacter(context) == '=')
                    {
                        // !=
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
                        // :=
                        setCurrentTokenType(context, LEX_ASSIGN_OPERATOR);
                        acceptCurrent(context);
                    }
                    else if (getCurrentCharacter(context) == ':')
                    {
                        // ::
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

/**
 * Merges adjacent strings and character literals into a single token.
 *
 * @param context context.
 *
 * @return Nonzero on success.
 */
static void mergeAdjacentStrings(struct LexerContext *context)
{
    int i, j = 0;
    int inSeriesOfStrings = 0;
    struct LEX_LexerToken *stringToken;
    struct LEX_LexerResult *result = context->result;
    // For each token.
    for (i = 0; i < context->tokenCount; i++)
    {
        struct LEX_LexerToken *token = &result->tokens[i];
        struct LEX_LexerToken *previousToken;
        switch (token->tokenType)
        {
            case LEX_STRING:
            case LEX_CHARACTER:
            {
                // On string or character.
                if (!inSeriesOfStrings)
                {
                    // If we are not in series of strings
                    // start one
                    inSeriesOfStrings = 1;
                    // Remember the first token in the series.
                    stringToken = token;
                }
                else
                {
                    // If we are in a series of strings, mark the current token
                    // deleted.
                    token->tokenType = LEX_SPEC_DELETED;
                }
            }
            break;
            default:
                if (inSeriesOfStrings)
                {
                    // If we are in a series of strings but read a non-string.
                    // Update the end position of the first token.
                    inSeriesOfStrings = 0;
                    stringToken->endColumn = previousToken->endColumn;
                    stringToken->endLine = previousToken->endLine;
                    // update the length of the first token.
                    stringToken->length =
                        (size_t)(previousToken->start + previousToken->length) -
                        (size_t)(stringToken->start);
                    // Now the first token's string contains all adjacent strings.
                }
        }
        previousToken = token;
    }
    // Now delete all previously marked tokens.
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
