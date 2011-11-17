#ifndef LEXER_H
#define LEXER_H

enum LEX_TokenType
{
    LEX_UNKNOWN
};


struct LEX_LexerToken
{
    const char *position;
    int length;
    enum LEX_TokenType tokenType;
};

struct LEX_LexerResult
{
    int tokenCount;
    struct LEX_LexerToken *tokens;
};

struct LEX_LexerResult LEX_tokenizeString(const char *code);

#endif // LEXER_H
