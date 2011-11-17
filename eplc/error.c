#include "error.h"

const int ERROR_BUFFER_SIZE = 100;

static ErrorCode errors[ERROR_BUFFER_SIZE] = {0};
static int errorPtr = 0;

void raiseError(ErrorCode errorCode)
{
    errors[errorPtr++] = errorCode;
}

ErrorCode catchError()
{
    if (!errorPtr)
    {
        return E_OK;
    }
    return errors[--errorPtr];
}

