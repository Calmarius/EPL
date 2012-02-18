/**
 * Copyright (c) 2012, Csirmaz Dávid
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Global error handling module.
 *
 * Error codes are handled in this module.
 */

#include "error.h"

/// Maximum number of raised error that can exist.
#define ERROR_BUFFER_SIZE 100

/**
 * An array that stores the error codes.
 */
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

void ERR_clearErrors()
{
    int i;
    for (i = 0; i < ERROR_BUFFER_SIZE; i++)
    {
        errors[i] = E_OK;
    }
}



