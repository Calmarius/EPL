#include "error.h"

#define ERROR_BUFFER_SIZE 100

static enum ERR_ErrorCode errors[ERROR_BUFFER_SIZE] = {0};

void ERR_raiseError(enum ERR_ErrorCode errorCode)
{
    int i;
    for (i = 0; i < ERROR_BUFFER_SIZE; i++)
    {
        if (!errors[i])
        {
            errors[i] = errorCode;
            return;
        }
    }
}

int ERR_catchError(enum ERR_ErrorCode errorCode)
{
    int i;
    for (i = 0; i < ERROR_BUFFER_SIZE; i++)
    {
        if (errors[i] == errorCode)
        {
            errors[i] = E_OK;
            return 1;
        }
    }
    return 0;
}

int ERR_isError()
{
    int i;
    for (i = 0; i < ERROR_BUFFER_SIZE; i++)
    {
        if (errors[i])
        {
            return 1;
        }
    }
    return 0;
}



