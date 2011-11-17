#include "lexer.h"

struct LEX_LexerResult LEX_tokenizeString(const char *code)
{
    struct LEX_LexerResult lexerResult;

    lexerResult.tokenCount = 0;
    lexerResult.tokens = 0;

    return lexerResult;
}
