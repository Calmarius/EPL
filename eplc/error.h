#ifndef ERROR_H
#define ERROR_H

enum ERR_ErrorCode
{
    E_OK = 0,
    E_FILE_NOT_FOUND,
    E_INVALID_CHARACTER,
    E_IMPOSSIBLE_ERROR,
    E_INVALID_BUILT_IN_TYPE_LETTER,
    E_INVALID_OPERATOR,
    E_MISSING_EXPONENTIAL_PART,
    E_HEXA_FLOATING_POINT_NOT_ALLOWED,
    E_STX_MODULE_EXPECTED,
    E_STX_MODULE_TYPE_EXPECTED,
    E_STX_SEMICOLON_EXPECTED,
    E_STX_MAIN_EXPECTED,
    E_STX_TYPE_EXPECTED,
    E_STX_VARDECL_EXPECTED,
    E_STX_IDENTIFIER_EXPECTED
};

void ERR_raiseError(enum ERR_ErrorCode errorCode);
int ERR_catchError(enum ERR_ErrorCode errorCode);
int ERR_isError();
void ERR_clearErrors();

#endif // ERROR_H
