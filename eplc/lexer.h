#ifndef LEXER_H
#define LEXER_H

#include "dynarray.h"

enum LEX_TokenType
{
    LEX_UNKNOWN,
    LEX_IDENTIFIER,
    LEX_SEMICOLON,
    LEX_LEFTBRACE,
    LEX_RIGHTBRACE,
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

#endif // LEXER_H
