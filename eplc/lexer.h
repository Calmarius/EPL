#ifndef LEXER_H
#define LEXER_H

#include "dynarray.h"

enum LEX_TokenType
{
    LEX_UNKNOWN,
    LEX_IDENTIFIER,
    LEX_SEMICOLON,
    LEX_LEFT_BRACE,
    LEX_RIGHT_BRACE,
    LEX_BUILT_IN_TYPE,
    LEX_ASSIGN_OPERATOR,
    LEX_FLOAT_NUMBER,
    LEX_HEXA_NUMBER,
    LEX_DECIMAL_NUMBER,
    LEX_OCTAL_NUMBER,
    LEX_ADD_OPERATOR,
    LEX_LEFT_PARENTHESIS,
    LEX_RIGHT_PARENTHESIS,
    LEX_LEFT_BRACKET,
    LEX_RIGHT_BRACKET,
    LEX_EQUALITY,

    LEX_KW_ELSE,
    LEX_KW_EXE,
    LEX_KW_MAIN,
    LEX_KW_MODULE,
    LEX_KW_IF,
    LEX_KW_INC,
    LEX_KW_LOOP,
    LEX_KW_NEXT,
    LEX_KW_VARDECL,
    LEX_KW_BREAK,
    LEX_KW_DLL,
    LEX_KW_LIB,
};


struct LEX_LexerToken
{
    const char *start;
    int length;
    int beginLine;
    int beginColumn;
    int endColumn;
    int endLine;
    enum LEX_TokenType tokenType;
};

struct LEX_LexerResult
{
    int tokenCount;
    struct LEX_LexerToken *tokens;
    int columnPos;
    int linePos;
};

/**
 * Parses the source code into tokens
 *
 * @param [in] code The code to be parsed.
 *
 * @return The tokens
 */
struct LEX_LexerResult LEX_tokenizeString(const char *code);
/**
 * Cleans up the lexer result
 *
 * @param [in,out] lexerResult Lexer result to clean up.
 */
void LEX_cleanUpLexerResult(struct LEX_LexerResult *lexerResult);

#endif // LEXER_H
