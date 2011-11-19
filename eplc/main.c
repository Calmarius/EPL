#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "error.h"

typedef void (*NotificationCallback)(const char *msg);

static char *sourceCode = 0;

const char *readFileContents(const char *filename)
{
    FILE *f = fopen(filename,"rb");
    int size;

    if (!f)
    {
        ERR_raiseError(E_FILE_NOT_FOUND);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f) + 1;
    fseek(f, 0, SEEK_SET);
    sourceCode = malloc(size + 1);
    fread(sourceCode, size, 1, f);
    fclose(f);
    sourceCode[size] = 0;

    return sourceCode;

}

#define STRINGCASE(x) case x : return #x;

const char *tokenTypeToString(enum LEX_TokenType type)
{
    switch (type)
    {
        STRINGCASE(LEX_UNKNOWN)
        STRINGCASE(LEX_IDENTIFIER)
        STRINGCASE(LEX_SEMICOLON)
        STRINGCASE(LEX_LEFTBRACE)
        STRINGCASE(LEX_RIGHTBRACE)
        STRINGCASE(LEX_KW_EXE)
        STRINGCASE(LEX_KW_MAIN)
        STRINGCASE(LEX_KW_MODULE)
    }
    return "<UNKNOWN>";
}

#undef STRINGCASE

void compileFile(const char *fileName, NotificationCallback callback)
{
    const char *fileContent = readFileContents(fileName);
    struct LEX_LexerResult lexerResult;
    char buffer[200];

    if (ERR_isError())
    {
        return;
    }

    if (callback)
    {
        callback("File opened.\n");
    }

    lexerResult = LEX_tokenizeString(fileContent);
    if (ERR_isError())
    {
        if (ERR_catchError(E_INVALID_CHARACTER))
        {
            sprintf(buffer, "At line %d, column %d: Invalid character.\n", lexerResult.linePos, lexerResult.columnPos);
            callback(buffer);
        }
        return;
    }
    if (callback)
    {
        int i;
        struct LEX_LexerToken *tokens = lexerResult.tokens;
        callback("Source code tokenized.\n");
        sprintf(buffer, "    %d tokens found.\n", lexerResult.tokenCount);
        callback(buffer);
        for (i = 0; i < lexerResult.tokenCount; i++)
        {
            struct LEX_LexerToken *token = &tokens[i];
            sprintf(
                buffer,
                "    %-20s  %20.*s (%-5d:%-3d) - (%-5d:%-3d)\n",
                tokenTypeToString(token->tokenType),
                token->length > 20 ? 20 : token->length,
                token->start,
                token->beginLine,
                token->beginColumn,
                token->endLine,
                token->endColumn
                );
            callback(buffer);
        }

    }

}

void notificationCallback(const char *msg)
{
    printf("%s",msg);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: eplc filename\n");
        goto cleanup;
    }
    compileFile(argv[1], notificationCallback);
    if (ERR_catchError(E_FILE_NOT_FOUND))
    {
        fprintf(stderr, "%s not found. \n", argv[1]);
    }

cleanup:
    free(sourceCode);

    fgetc(stdin);
    return 0;
}
