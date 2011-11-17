#ifndef ERROR_H
#define ERROR_H

enum ErrorCode
{
    E_OK = 0,

};

void raiseError(ErrorCode errorCode);
ErrorCode catchError();

#endif // ERROR_H
