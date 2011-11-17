#ifndef ERROR_H
#define ERROR_H

enum ERR_ErrorCode
{
    E_OK = 0,
    E_FILE_NOT_FOUND
};

void ERR_raiseError(enum ERR_ErrorCode errorCode);
int ERR_catchError(enum ERR_ErrorCode errorCode);
int ERR_isError();

#endif // ERROR_H
