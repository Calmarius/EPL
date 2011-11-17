#ifndef ERROR_H
#define ERROR_H

enum ErrorCode
{
    E_OK = 0,

};

void ERR_raiseError(enum ErrorCode errorCode);
enum ErrorCode ERR_catchError();

#endif // ERROR_H
