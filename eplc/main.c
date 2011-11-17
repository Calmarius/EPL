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

    fseek(f, SEEK_END, 0);
    size = ftell(f) + 1;
    fseek(f, SEEK_SET, 0);
    sourceCode = malloc(size);
    fread(sourceCode, size, 1, f);
    fclose(f);

    return sourceCode;

}

void compileFile(const char *fileName, NotificationCallback callback)
{
    const char *fileContent = readFileContents(fileName);
    struct LEX_LexerResult lexerResult;

    if (ERR_isError())
    {
        return;
    }

    if (callback)
    {
        callback("File opened.\n");
    }

    lexerResult = LEX_tokenizeString(fileContent);

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
    return 0;
}
