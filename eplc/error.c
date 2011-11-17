#include "error.h"

#define ERROR_BUFFER_SIZE 100

static enum ErrorCode errors[ERROR_BUFFER_SIZE] = {0};
static int errorPtr = 0;

void ERR_raiseError(enum ErrorCode errorCode)
{
    errors[errorPtr++] = errorCode;
}

enum ErrorCode ERR_catchError()
{
    if (!errorPtr)
    {
        return E_OK;
    }
    return errors[--errorPtr];
}

