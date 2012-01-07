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
    LEX_SUBTRACT_OPERATOR,
    LEX_MULTIPLY_OPERATOR,
    LEX_LEFT_PARENTHESIS,
    LEX_RIGHT_PARENTHESIS,
    LEX_LEFT_BRACKET,
    LEX_RIGHT_BRACKET,
    LEX_COMMA,
    LEX_LESS_THAN,
    LEX_GREATER_THAN,
    LEX_EQUAL,
    LEX_LESS_EQUAL_THAN,
    LEX_GREATER_EQUAL_THAN,
    LEX_NOT_EQUAL,
    LEX_STRING,
    LEX_PERIOD,
    LEX_SCOPE_SEPARATOR,
    LEX_DIVISION_OPERATOR,
    LEX_BLOCK_COMMENT,
    LEX_EOL_COMMENT,
    LEX_DOCUMENTATION_BLOCK_COMMENT,
    LEX_DOCUMENTATION_EOL_COMMENT,
    LEX_DOCUMENTATION_EOL_BACK_COMMENT,
    LEX_COLON,
    LEX_SHIFT_RIGHT,
    LEX_SHIFT_LEFT,
    LEX_CHARACTER,

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
    LEX_KW_HANDLE,
    LEX_KW_POINTER,
    LEX_KW_LOCALPTR,
    LEX_KW_BUFFER,
    LEX_KW_OF,
    LEX_KW_TO,
    LEX_KW_IN,
    LEX_KW_OUT,
    LEX_KW_REF,
    LEX_KW_FUNCTION,
    LEX_KW_RETURN,
    LEX_KW_CAST,
    LEX_KW_CLEANUP,
    LEX_KW_NAMESPACE,
    LEX_KW_USING,
    LEX_KW_STRUCT,
    LEX_KW_FUNCPTR,
    LEX_KW_CASE,
    LEX_KW_CONTINUE,
    LEX_KW_SWITCH,
    LEX_KW_DEFAULT,
    LEX_KW_OPERATOR,
    LEX_KW_MULTIPLICATIVE,
    LEX_KW_ADDITIVE,
    LEX_KW_RELATIONAL,
    LEX_KW_NOT,
    LEX_KW_NEG,
    LEX_KW_DEREF,
    LEX_KW_EXTERNAL,
    LEX_KW_FOR,

    LEX_SPEC_EOF,
    LEX_SPEC_DELETED,
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
    char **strings;
    int stringCount;
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
