#ifndef ERROR_H
#define ERROR_H

enum ERR_ErrorCode
{
    E_OK = 0,
    E_FILE_NOT_FOUND,
    E_LEX_INVALID_CHARACTER,
    E_LEX_IMPOSSIBLE_ERROR,
    E_LEX_INVALID_BUILT_IN_TYPE_LETTER,
    E_LEX_INVALID_OPERATOR,
    E_LEX_MISSING_EXPONENTIAL_PART,
    E_LEX_HEXA_FLOATING_POINT_NOT_ALLOWED,
    E_LEX_QUOTE_EXPECTED,

    E_STX_MODULE_EXPECTED,
    E_STX_MODULE_TYPE_EXPECTED,
    E_STX_SEMICOLON_EXPECTED,
    E_STX_MAIN_EXPECTED,
    E_STX_TYPE_EXPECTED,
    E_STX_VARDECL_EXPECTED,
    E_STX_IDENTIFIER_EXPECTED,
    E_STX_OF_EXPECTED,
    E_STX_LEFT_BRACKET_EXPECTED,
    E_STX_INTEGER_NUMBER_EXPECTED,
    E_STX_RIGHT_BRACKET_EXPECTED,
    E_STX_TO_EXPECTED,
    E_STX_PARAMETER_DIRECTION_EXPECTED,
    E_STX_LEFT_PARENTHESIS_EXPECTED,
    E_STX_RIGHT_PARENTHESIS_EXPECTED,
    E_STX_COMMA_EXPECTED,
    E_STX_FUNCTION_EXPECTED,
    E_STX_LEFT_BRACE_EXPECTED,
    E_STX_RIGHT_BRACE_EXPECTED,
    E_STX_RETURN_EXPECTED,
    E_STX_TERM_EXPECTED,
    E_STX_IF_EXPECTED,
    E_STX_UNKNOWN_STATEMENT,
    E_STX_LOOP_EXPECTED,
    E_STX_ASSIGNMENT_OR_EXPRESSION_STATEMENT_EXPECTED,
    E_STX_UNEXPECTED_END_OF_FILE,
    E_STX_NAMESPACE_EXPECTED,
    E_STX_USING_EXPECTED,
    E_STX_PERIOD_EXPECTED,
    E_STX_STRUCT_EXPECTED,
    E_STX_FUNCPTR_EXPECTED,
    E_STX_CASE_EXPECTED,
    E_STX_COLON_EXPECTED,
    E_STX_BREAK_OR_CONTINUE_EXPECTED,
    E_STX_SWITCH_EXPECTED,
    E_STX_CASE_OR_DEFAULT_EXPECTED,
    E_STX_DECLARATION_EXPECTED,
    E_STX_PRECEDENCE_TYPE_EXPECTED,
    E_STX_STRING_EXPECTED,
    E_STX_BLOCK_OR_EXTERNAL_EXPECTED,
    E_STX_PLATFORM_EXPECTED,
    E_STX_BLOCK_OR_IF_STATEMENT_EXPECTED,
    E_STX_CORRUPT_TOKEN,

    E_SMC_CORRUPT_SYNTAX_TREE,
    E_SMC_REDEFINITION_OF_SYMBOL,
    E_SMC_TOO_FEW_PARAMETERS,
    E_SMC_TOO_MANY_PARAMETERS,
    E_SMC_EMPTY_PLATFORM_BLOCK,
    E_SMC_BREAK_IS_NOT_IN_LOOP_OR_CASE_BLOCK,
    E_SMC_CONTINUE_IS_NOT_IN_LOOP_OR_CASE_BLOCK,
    E_SMC_UNDEFINED_SYMBOL,
    E_SMC_NOT_AN_OPERATOR,
    E_SMC_NOT_A_NAMESPACE,
    E_SMC_AMBIGUOS_NAME,
};

void ERR_raiseError(enum ERR_ErrorCode errorCode);
int ERR_catchError(enum ERR_ErrorCode errorCode);
int ERR_isError();
void ERR_clearErrors();

#endif // ERROR_H
