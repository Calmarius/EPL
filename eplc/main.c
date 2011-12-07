#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "lexer.h"
#include "error.h"
#include "syntax.h"

typedef void (*NotificationCallback)(const char *msg);

static char *sourceCode = 0;

const char *readFileContents(const char *filename)
{
    FILE *f = fopen(filename,"rb");
    int size;
    int read;

    if (!f)
    {
        ERR_raiseError(E_FILE_NOT_FOUND);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    sourceCode = malloc(size + 1);
    read = fread(sourceCode, size, 1, f);
    assert(read);
    fclose(f);
    sourceCode[size] = 0;

    return sourceCode;

}

#define STRINGCASE(x) case x : return #x;

const char *tokenTypeToString(enum LEX_TokenType type)
{
    switch (type)
    {
        STRINGCASE(LEX_UNKNOWN)
        STRINGCASE(LEX_IDENTIFIER)
        STRINGCASE(LEX_SEMICOLON)
        STRINGCASE(LEX_LEFT_BRACE)
        STRINGCASE(LEX_RIGHT_BRACE)
        STRINGCASE(LEX_ASSIGN_OPERATOR)
        STRINGCASE(LEX_BUILT_IN_TYPE)
        STRINGCASE(LEX_FLOAT_NUMBER)
        STRINGCASE(LEX_HEXA_NUMBER)
        STRINGCASE(LEX_DECIMAL_NUMBER)
        STRINGCASE(LEX_OCTAL_NUMBER)
        STRINGCASE(LEX_ADD_OPERATOR)
        STRINGCASE(LEX_LEFT_PARENTHESIS)
        STRINGCASE(LEX_RIGHT_PARENTHESIS)
        STRINGCASE(LEX_MULTIPLY_OPERATOR)
        STRINGCASE(LEX_LEFT_BRACKET)
        STRINGCASE(LEX_RIGHT_BRACKET)
        STRINGCASE(LEX_COMMA)
        STRINGCASE(LEX_LESS_EQUAL_THAN)
        STRINGCASE(LEX_LESS_THAN)
        STRINGCASE(LEX_GREATER_EQUAL_THAN)
        STRINGCASE(LEX_GREATER_THAN)
        STRINGCASE(LEX_EQUAL)
        STRINGCASE(LEX_NOT_EQUAL)
        STRINGCASE(LEX_SUBTRACT_OPERATOR)

        STRINGCASE(LEX_KW_EXE)
        STRINGCASE(LEX_KW_MAIN)
        STRINGCASE(LEX_KW_MODULE)
        STRINGCASE(LEX_KW_IF)
        STRINGCASE(LEX_KW_LOOP)
        STRINGCASE(LEX_KW_NEXT)
        STRINGCASE(LEX_KW_VARDECL)
        STRINGCASE(LEX_KW_INC)
        STRINGCASE(LEX_KW_ELSE)
        STRINGCASE(LEX_KW_BREAK)
        STRINGCASE(LEX_KW_DLL)
        STRINGCASE(LEX_KW_LIB)
        STRINGCASE(LEX_KW_HANDLE)
        STRINGCASE(LEX_KW_POINTER)
        STRINGCASE(LEX_KW_LOCALPTR)
        STRINGCASE(LEX_KW_BUFFER)
        STRINGCASE(LEX_KW_OF)
        STRINGCASE(LEX_KW_TO)
        STRINGCASE(LEX_KW_IN)
        STRINGCASE(LEX_KW_OUT)
        STRINGCASE(LEX_KW_REF)
        STRINGCASE(LEX_KW_RETURN)
        STRINGCASE(LEX_KW_FUNCTION)
        STRINGCASE(LEX_KW_CAST)
    }
    return "<UNKNOWN>";
}

const char *nodeTypeToString(enum STX_NodeType nodeType)
{
    switch (nodeType)
    {
        STRINGCASE(STX_ROOT)
        STRINGCASE(STX_MODULE)
        STRINGCASE(STX_BLOCK)
        STRINGCASE(STX_DECLARATIONS)
        STRINGCASE(STX_TYPE)
        STRINGCASE(STX_VARDECL)
        STRINGCASE(STX_TYPE_PREFIX)
        STRINGCASE(STX_PARAMETER)
        STRINGCASE(STX_ARGUMENT_LIST)
        STRINGCASE(STX_FUNCTION)
        STRINGCASE(STX_EXPRESSION_STATEMENT)
        STRINGCASE(STX_TERM)
        STRINGCASE(STX_RETURN_STATEMENT)
        STRINGCASE(STX_EXPRESSION)
        STRINGCASE(STX_OPERATOR)
        STRINGCASE(STX_IF_STATEMENT)
    }
    return "<UNKNOWN>";
}

const char *moduleTypeToString(enum STX_ModuleAttribute moduleType)
{
    switch (moduleType)
    {
        STRINGCASE(STX_MOD_DLL)
        STRINGCASE(STX_MOD_LIB)
        STRINGCASE(STX_MOD_EXE)
    }
    return "<UNKNOWN>";
}

const char *typePrefixTypeToString(enum STX_TypePrefix moduleType)
{
    switch (moduleType)
    {
        STRINGCASE(STX_TP_BUFFER)
        STRINGCASE(STX_TP_HANDLE)
        STRINGCASE(STX_TP_LOCALPTR)
        STRINGCASE(STX_TP_POINTER)
    }
    return "<UNKNOWN>";
}

const char *parameterDirectionTypeToString(enum STX_ParameterDirection direction)
{
    switch (direction)
    {
        STRINGCASE(STX_PD_IN)
        STRINGCASE(STX_PD_OUT)
        STRINGCASE(STX_PD_REF)
    }
    return "<UNKNOWN>";
}

const char *termTypeToString(enum STX_TermType type)
{
    switch (type)
    {
        STRINGCASE(STX_TT_CAST_EXPRESSION)
        STRINGCASE(STX_TT_SIMPLE)
        STRINGCASE(STX_TT_FUNCTION_CALL)
        STRINGCASE(STX_TT_PARENTHETICAL)
        STRINGCASE(STX_TT_UNARY_OPERATOR)
        STRINGCASE(STX_TT_ARRAY_SUBSCRIPT)
    }
    return "<UNKNOWN>";
}

#undef STRINGCASE

const char *attributeToString(const struct STX_SyntaxTreeNode *node)
{
    static char buffer[500];
    char *ptr = buffer;
    const struct STX_NodeAttribute *attribute = STX_getNodeAttribute(node);

    if (!attribute)
    {
        return "";
    }
    if (node->nodeType == STX_TYPE_PREFIX)
    {
        ptr += sprintf(ptr, "type = %s ", typePrefixTypeToString(attribute->typePrefixAttributes.type));
        if (attribute->typePrefixAttributes.type == STX_TP_BUFFER)
        {
            ptr += sprintf(ptr, "elements = %d ", attribute->typePrefixAttributes.elements);
        }
    }
    else if (node->nodeType == STX_MODULE)
    {
        ptr += sprintf(
            ptr,
            "type = %s ",
            moduleTypeToString(attribute->moduleAttributes.type));
    }
    else if (node->nodeType == STX_OPERATOR)
    {
        ptr += sprintf(
            ptr,
            "type = %s ",
            tokenTypeToString(attribute->operatorAttributes.type));
    }
    else if (node->nodeType == STX_PARAMETER)
    {
        ptr += sprintf(
            ptr,
            "direction = %s ",
            parameterDirectionTypeToString(
                attribute->parameterAttributes.direction));
    }
    else if (node->nodeType == STX_TERM)
    {
        ptr += sprintf(
            ptr,
            "termType = %s tokenType = %s ",
            termTypeToString(
                attribute->termAttributes.termType),
            tokenTypeToString(
                attribute->termAttributes.tokenType));
    }
    if (attribute->name)
    {
        ptr += sprintf(
            ptr,
            "name = '%.*s' ",
            attribute->nameLength,
            attribute->name);
    }
    *ptr = 0;
    return buffer;
}

void dumpTreeCallback(struct STX_SyntaxTreeNode *node, int level, void *userData)
{
    NotificationCallback callback = (NotificationCallback)userData;
    char buffer[500];
    sprintf(
        buffer,
        "%*s %s %s [#%d, %d - %d, <= %d  %d =>]\n",
        level*4,
        "",
        nodeTypeToString(node->nodeType),
        attributeToString(node),
        node->id,
        node->firstChildIndex,
        node->lastChildIndex,
        node->previousSiblingIndex,
        node->nextSiblingIndex);
    callback(buffer);
}

void compileFile(const char *fileName, NotificationCallback callback)
{
    const char *fileContent = readFileContents(fileName);
    struct LEX_LexerResult lexerResult;
    struct STX_ParserResult parserResult;
    char buffer[200];

    if (ERR_isError())
    {
        return;
    }

    if (callback)
    {
        callback("File opened.\n");
    }

    lexerResult = LEX_tokenizeString(fileContent);
    if (ERR_isError())
    {
        sprintf(buffer, "At line %d, column %d:", lexerResult.linePos, lexerResult.columnPos);
        callback(buffer);
        if (ERR_catchError(E_INVALID_CHARACTER))
        {
            sprintf(buffer, "Invalid character.\n");
        }
        else if (ERR_catchError(E_INVALID_BUILT_IN_TYPE_LETTER))
        {
            sprintf(buffer, "Invalid built in type.\n");
        }
        else if (ERR_catchError(E_INVALID_OPERATOR))
        {
            sprintf(buffer, "Invalid operator\n");
        }
        else if (ERR_catchError(E_MISSING_EXPONENTIAL_PART))
        {
            sprintf(buffer, "Missing exponential part.\n");
        }
        else if (ERR_catchError(E_HEXA_FLOATING_POINT_NOT_ALLOWED))
        {
            sprintf(buffer, "Hexa floating point is not allowed.\n");
        }
        callback(buffer);
        goto cleanup;
    }
    if (callback)
    {
        int i;
        struct LEX_LexerToken *tokens = lexerResult.tokens;
        callback("Source code tokenized.\n");
        sprintf(buffer, "    %d tokens found.\n", lexerResult.tokenCount);
        callback(buffer);
        for (i = 0; i < lexerResult.tokenCount; i++)
        {
            struct LEX_LexerToken *token = &tokens[i];
            sprintf(
                buffer,
                "    %-20s  %20.*s (%-5d:%-3d) - (%-5d:%-3d)\n",
                tokenTypeToString(token->tokenType),
                token->length > 20 ? 20 : token->length,
                token->start,
                token->beginLine,
                token->beginColumn,
                token->endLine,
                token->endColumn
                );
            callback(buffer);
        }
    }
    // Syntax analysis
    parserResult = STX_buildSyntaxTree(lexerResult.tokens, lexerResult.tokenCount);

    if (ERR_isError())
    {
        sprintf(buffer, "At line %d, column %d: ", parserResult.line, parserResult.column);
        callback(buffer);
        if (ERR_catchError(E_STX_MAIN_EXPECTED))
        {
            sprintf(buffer, "main expected. \n");
        }
        else if (ERR_catchError(E_STX_MODULE_EXPECTED))
        {
            sprintf(buffer, "module expected. \n");
        }
        else if (ERR_catchError(E_STX_MODULE_TYPE_EXPECTED))
        {
            sprintf(buffer, "exe, dll or lib expected. \n");
        }
        else if (ERR_catchError(E_STX_SEMICOLON_EXPECTED))
        {
            sprintf(buffer, "; expected. \n");
        }
        else if (ERR_catchError(E_STX_TYPE_EXPECTED))
        {
            sprintf(buffer, "data type expected. \n");
        }
        else if (ERR_catchError(E_STX_IDENTIFIER_EXPECTED))
        {
            sprintf(buffer, "identifier expected. \n");
        }
        else if (ERR_catchError(E_STX_VARDECL_EXPECTED))
        {
            sprintf(buffer, "variable declaration expected. \n");
        }
        else if (ERR_catchError(E_STX_OF_EXPECTED))
        {
            sprintf(buffer, "of expected. \n");
        }
        else if (ERR_catchError(E_STX_LEFT_BRACKET_EXPECTED))
        {
            sprintf(buffer, "[ expected. \n");
        }
        else if (ERR_catchError(E_STX_RIGHT_BRACKET_EXPECTED))
        {
            sprintf(buffer, "] expected. \n");
        }
        else if (ERR_catchError(E_STX_INTEGER_NUMBER_EXPECTED))
        {
            sprintf(buffer, "integer number expected. \n");
        }
        else if (ERR_catchError(E_STX_TO_EXPECTED))
        {
            sprintf(buffer, "to expected. \n");
        }
        else if (ERR_catchError(E_STX_PARAMETER_DIRECTION_EXPECTED))
        {
            sprintf(buffer, "parameter direction expected. \n");
        }
        else if (ERR_catchError(E_STX_LEFT_PARENTHESIS_EXPECTED))
        {
            sprintf(buffer, "( expected. \n");
        }
        else if (ERR_catchError(E_STX_RIGHT_PARENTHESIS_EXPECTED))
        {
            sprintf(buffer, ") expected. \n");
        }
        else if (ERR_catchError(E_STX_COMMA_EXPECTED))
        {
            sprintf(buffer, ", expected. \n");
        }
        else if (ERR_catchError(E_STX_FUNCTION_EXPECTED))
        {
            sprintf(buffer, "function expected. \n");
        }
        else if (ERR_catchError(E_STX_LEFT_BRACE_EXPECTED))
        {
            sprintf(buffer, "{ expected. \n");
        }
        else if (ERR_catchError(E_STX_RIGHT_BRACE_EXPECTED))
        {
            sprintf(buffer, "} expected. \n");
        }
        else if (ERR_catchError(E_STX_RETURN_EXPECTED))
        {
            sprintf(buffer, "return expected. \n");
        }
        else if (ERR_catchError(E_STX_TERM_EXPECTED))
        {
            sprintf(buffer, "term expected. \n");
        }
        else if (ERR_catchError(E_STX_IF_EXPECTED))
        {
            sprintf(buffer, "if expected. \n");
        }
        else if (ERR_catchError(E_STX_UNKNOWN_STATEMENT))
        {
            sprintf(buffer, "unknown statement. \n");
        }
        else
        {
            sprintf(buffer, "Unhandled syntax error.\n");
        }
        callback(buffer);
        goto cleanup;
    }
    STX_transversePreorder(parserResult.tree, dumpTreeCallback, callback);
cleanup:
    LEX_cleanUpLexerResult(&lexerResult);
}

void notificationCallback(const char *msg)
{
    printf("%s",msg);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: eplc filename\n");
        goto cleanup;
    }
    compileFile(argv[1], notificationCallback);
    if (ERR_catchError(E_FILE_NOT_FOUND))
    {
        fprintf(stderr, "%s not found. \n", argv[1]);
    }

cleanup:
    free(sourceCode);

    fgetc(stdin);

    return 0;
}
