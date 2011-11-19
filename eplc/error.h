#ifndef ERROR_H
#define ERROR_H

enum ERR_ErrorCode
{
    E_OK = 0,
    E_FILE_NOT_FOUND,
    E_INVALID_CHARACTER
};

void ERR_raiseError(enum ERR_ErrorCode errorCode);
int ERR_catchError(enum ERR_ErrorCode errorCode);
int ERR_isError();
void ERR_clearErrors();

#endif // ERROR_H
