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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "lexer.h"
#include "error.h"
#include "syntax.h"
#include "assocarray.h"
#include "semantic.h"

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
        STRINGCASE(LEX_HEXA_INTEGER)
        STRINGCASE(LEX_DECIMAL_INTEGER)
        STRINGCASE(LEX_OCTAL_INTEGER)
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
        STRINGCASE(LEX_STRING)
        STRINGCASE(LEX_PERIOD)
        STRINGCASE(LEX_SCOPE_SEPARATOR)
        STRINGCASE(LEX_DIVISION_OPERATOR)
        STRINGCASE(LEX_BLOCK_COMMENT)
        STRINGCASE(LEX_EOL_COMMENT)
        STRINGCASE(LEX_DOCUMENTATION_BLOCK_COMMENT)
        STRINGCASE(LEX_DOCUMENTATION_EOL_COMMENT)
        STRINGCASE(LEX_DOCUMENTATION_EOL_BACK_COMMENT)
        STRINGCASE(LEX_COLON)
        STRINGCASE(LEX_SHIFT_LEFT)
        STRINGCASE(LEX_SHIFT_RIGHT)
        STRINGCASE(LEX_CHARACTER)

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
        STRINGCASE(LEX_KW_CLEANUP)
        STRINGCASE(LEX_KW_NAMESPACE)
        STRINGCASE(LEX_KW_USING)
        STRINGCASE(LEX_KW_STRUCT)
        STRINGCASE(LEX_KW_FUNCPTR)
        STRINGCASE(LEX_KW_CASE)
        STRINGCASE(LEX_KW_CONTINUE)
        STRINGCASE(LEX_KW_SWITCH)
        STRINGCASE(LEX_KW_DEFAULT)
        STRINGCASE(LEX_KW_OPERATOR)
        STRINGCASE(LEX_KW_MULTIPLICATIVE)
        STRINGCASE(LEX_KW_ADDITIVE)
        STRINGCASE(LEX_KW_RELATIONAL)
        STRINGCASE(LEX_KW_NOT)
        STRINGCASE(LEX_KW_NEG)
        STRINGCASE(LEX_KW_DEREF)
        STRINGCASE(LEX_KW_EXTERNAL)
        STRINGCASE(LEX_KW_FOR)
        STRINGCASE(LEX_KW_STATICPTR)

        STRINGCASE(LEX_SPEC_EOF)
        STRINGCASE(LEX_SPEC_DELETED)
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
        STRINGCASE(STX_TP_NONE)
        STRINGCASE(STX_TP_BUFFER)
        STRINGCASE(STX_TP_HANDLE)
        STRINGCASE(STX_TP_LOCALPTR)
        STRINGCASE(STX_TP_POINTER)
        STRINGCASE(STX_TP_STATICPTR)
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

const char *attributeToString(struct STX_SyntaxTreeNode *node)
{
    static char buffer[500];
    char *ptr = buffer;
    const struct STX_NodeAttribute *attribute = STX_getNodeAttribute(node);

    if (!attribute)
    {
        return "";
    }
    switch (node->nodeType)
    {
        case STX_TYPE_PREFIX:
        {
            ptr += sprintf(ptr, "type = %s ", typePrefixTypeToString(attribute->typePrefixAttributes.type));
            if (attribute->typePrefixAttributes.type == STX_TP_BUFFER)
            {
                ptr += sprintf(ptr, "elements = %d ", attribute->typePrefixAttributes.elements);
            }
        }
        break;
        case STX_MODULE:
        {
            ptr += sprintf(
                ptr,
                "type = %s ",
                moduleTypeToString(attribute->moduleAttributes.type));
        }
        break;
        case STX_OPERATOR:
        {
            ptr += sprintf(
                ptr,
                "type = %s ",
                tokenTypeToString(attribute->operatorAttributes.type));
        }
        break;
        case STX_PARAMETER:
        {
            ptr += sprintf(
                ptr,
                "direction = %s ",
                parameterDirectionTypeToString(
                    attribute->parameterAttributes.direction));
        }
        break;
        case STX_BREAK:
        case STX_CONTINUE:
        {
            ptr += sprintf(
                ptr,
                "levels = %d associatedwith = #%d",
                attribute->breakContinueAttributes.levels,
                attribute->breakContinueAttributes.associatedNodeId
            );
        }
        break;
        case STX_LOOP_STATEMENT:
        {
            ptr += sprintf(
                ptr,
                "hasBreak = %d ",
                attribute->loopAttributes.hasBreak);
        }
        break;
        case STX_TERM:
        {
            ptr += sprintf(
                ptr,
                "termType = %s tokenType = %s ",
                termTypeToString(
                    attribute->termAttributes.termType),
                tokenTypeToString(
                    attribute->termAttributes.tokenType));
        }
        break;
        case STX_CASE:
        {
            if (attribute->caseAttributes.isDefault)
            {
                ptr += sprintf(ptr, "default ");
            }
            else
            {
                ptr += sprintf(
                    ptr,
                    "caseValue = %d ",
                    attribute->caseAttributes.caseValue
                );

            }
        }
        break;
        case STX_TYPE:
        {
            if (!attribute->typeAttributes.isPrimitive) break;
            // At this point the type is primitive.
            ptr += sprintf(
                ptr,
                "type = %s bits = %d ",
                STX_PrimitiveTypeTypeToString(attribute->typeAttributes.type),
                attribute->typeAttributes.bitCount
            );
            if (attribute->typeAttributes.attribute)
            {
                ptr += sprintf(
                    ptr,
                    "attribute = '%.*s' ",
                    attribute->typeAttributes.attributeLength,
                    attribute->typeAttributes.attribute
                );
            }
        }
        break;
        case STX_OPERATOR_FUNCTION:
        case STX_FUNCTION:
        {
            if (node->nodeType == STX_OPERATOR_FUNCTION)
            {
                ptr += sprintf(
                    ptr,
                    "precedenceLevel = %s ",
                    tokenTypeToString(
                        attribute->functionAttributes.precedence
                    )
                );
            }
            if (attribute->functionAttributes.isExternal)
            {
                ptr += sprintf(
                    ptr,
                    "location = %.*s type = %.*s ",
                    attribute->functionAttributes.externalLocationLength,
                    attribute->functionAttributes.externalLocation,
                    attribute->functionAttributes.externalFileTypeLength,
                    attribute->functionAttributes.externalFileType
                );

            }
        }
        break;
        default:
        break;
    }
    if (attribute->typeInformation.metaType != STX_TYT_NONE)
    {
        const struct STX_TypeInformation *typeInfo = &attribute->typeInformation;

        ptr += sprintf(ptr, "type = '");
        if (typeInfo->assignable)
        {
            ptr += sprintf(ptr, "left value of ");
        }
        switch (typeInfo->prefix)
        {
            case STX_TP_BUFFER:
                ptr += sprintf(ptr, "buffer of ");
            break;
            case STX_TP_HANDLE:
                ptr += sprintf(ptr, "handle of ");
            break;
            case STX_TP_LOCALPTR:
                ptr += sprintf(ptr, "localptr of ");
            break;
            case STX_TP_NONE:
            break;
            case STX_TP_POINTER:
                ptr += sprintf(ptr, "pointer of ");
            break;
            case STX_TP_STATICPTR:
                ptr += sprintf(ptr, "staticptr of ");
            break;
        }
        if (typeInfo->metaType == STX_TYT_SIMPLE)
        {
            switch (typeInfo->type)
            {
                case STX_STT_UNSIGNED_INT:
                    ptr += sprintf(ptr, "$u");
                break;
                case STX_STT_SIGNED_INT:
                    ptr += sprintf(ptr, "$i");
                break;
                case STX_STT_FLOAT:
                    ptr += sprintf(ptr, "$f");
                break;
            }
            if (typeInfo->bitCount)
            {
                ptr += sprintf(ptr, "%d", typeInfo->bitCount);
            }
            if (typeInfo->attribLength)
            {
                ptr += sprintf(ptr, "_%.*s", typeInfo->attribLength, typeInfo->attribs);
            }
        }
        ptr += sprintf(ptr, "' ");
    }
    if (attribute->name)
    {
        ptr += sprintf(
            ptr,
            "name = '%.*s' ",
            attribute->nameLength,
            attribute->name);
    }
    if (attribute->symbolDefinitionNodeId >= 0)
    {
        ptr += sprintf(
            ptr,
            "definer = #%d",
            attribute->symbolDefinitionNodeId
        );
    }
    if (attribute->comment)
    {
        ptr += sprintf(
            ptr,
            "comment = '%.*s' ",
            attribute->commentLength,
            attribute->comment);
    }
    *ptr = 0;
    return buffer;
}

int dumpTreeCallback(struct STX_SyntaxTreeNode *node, int level, void *userData)
{
    FILE *f = (FILE *)userData;
    fprintf(
        f,
        "#%d %*s %s %s (%d:%d) - (%d:%d) [%d - %d, <= %d  %d => {in: %d, defines: %d}]\n",
        node->id,
        level*4,
        "",
        STX_nodeTypeToString(node->nodeType),
        attributeToString(node),
        node->beginLine,
        node->beginColumn,
        node->endLine,
        node->endColumn,
        node->firstChildIndex,
        node->lastChildIndex,
        node->previousSiblingIndex,
        node->nextSiblingIndex,
        node->inScopeId,
        node->definesScopeId
    );
    return 1;
}

void compileFile(const char *fileName, NotificationCallback callback)
{
    const char *fileContent = readFileContents(fileName);
    struct LEX_LexerResult lexerResult;
    struct STX_ParserResult parserResult;
    struct SMC_CheckerResult checkerResult;
    char buffer[200];
    char *fn = malloc(strlen(fileName) + 10);

    setbuf(stdout, 0);

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
        if (ERR_catchError(E_LEX_INVALID_CHARACTER))
        {
            sprintf(buffer, "Invalid character.\n");
        }
        else if (ERR_catchError(E_LEX_INVALID_BUILT_IN_TYPE_LETTER))
        {
            sprintf(buffer, "Invalid built in type.\n");
        }
        else if (ERR_catchError(E_LEX_INVALID_OPERATOR))
        {
            sprintf(buffer, "Invalid operator\n");
        }
        else if (ERR_catchError(E_LEX_MISSING_EXPONENTIAL_PART))
        {
            sprintf(buffer, "Missing exponential part.\n");
        }
        else if (ERR_catchError(E_LEX_HEXA_FLOATING_POINT_NOT_ALLOWED))
        {
            sprintf(buffer, "Hexa floating point is not allowed.\n");
        }
        else if (ERR_catchError(E_LEX_INVALID_HEXA_LITERAL))
        {
            sprintf(buffer, "Invalid hexa literal.\n");
        }
        else if (ERR_catchError(E_LEX_INVALID_DECIMAL_NUMBER))
        {
            sprintf(buffer, "Invalid decimal number.\n");
        }
        callback(buffer);
        goto cleanup;
    }
    if (callback)
    {
        int i;
        struct LEX_LexerToken *tokens = lexerResult.tokens;
        sprintf(fn, "%s.tokens", fileName);
        FILE *f = fopen(fn, "w+t");

        callback("Source code tokenized.\n");
        sprintf(buffer, "    %d tokens found.\n", lexerResult.tokenCount);
        callback(buffer);
        for (i = 0; i < lexerResult.tokenCount; i++)
        {
            struct LEX_LexerToken *token = &tokens[i];
            fprintf(
                f,
                "    %-40s  %20.*s (%-5d:%-3d) - (%-5d:%-3d)\n",
                tokenTypeToString(token->tokenType),
                token->length > 20 ? 20 : token->length,
                token->start,
                token->beginLine,
                token->beginColumn,
                token->endLine,
                token->endColumn
                );
        }
        fclose(f);
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
        else if (ERR_catchError(E_STX_LOOP_EXPECTED))
        {
            sprintf(buffer, "loop expected. \n");
        }
        else if (ERR_catchError(E_STX_ASSIGNMENT_OR_EXPRESSION_STATEMENT_EXPECTED))
        {
            sprintf(buffer, "Assignment or expression statement expected. \n");
        }
        else if (ERR_catchError(E_STX_UNEXPECTED_END_OF_FILE))
        {
            sprintf(buffer, "Unexpected end of file. \n");
        }
        else if (ERR_catchError(E_STX_NAMESPACE_EXPECTED))
        {
            sprintf(buffer, "namespace expected. \n");
        }
        else if (ERR_catchError(E_STX_USING_EXPECTED))
        {
            sprintf(buffer, "using expected. \n");
        }
        else if (ERR_catchError(E_STX_PERIOD_EXPECTED))
        {
            sprintf(buffer, ". expected. \n");
        }
        else if (ERR_catchError(E_STX_STRUCT_EXPECTED))
        {
            sprintf(buffer, "struct expected. \n");
        }
        else if (ERR_catchError(E_STX_FUNCPTR_EXPECTED))
        {
            sprintf(buffer, "funcptr expected. \n");
        }
        else if (ERR_catchError(E_STX_CASE_EXPECTED))
        {
            sprintf(buffer, "case expected. \n");
        }
        else if (ERR_catchError(E_STX_COLON_EXPECTED))
        {
            sprintf(buffer, ": expected. \n");
        }
        else if (ERR_catchError(E_STX_BREAK_OR_CONTINUE_EXPECTED))
        {
            sprintf(buffer, "break or continue expected. \n");
        }
        else if (ERR_catchError(E_STX_SWITCH_EXPECTED))
        {
            sprintf(buffer, "switch expected. \n");
        }
        else if (ERR_catchError(E_STX_CASE_OR_DEFAULT_EXPECTED))
        {
            sprintf(buffer, "case or default expected. \n");
        }
        else if (ERR_catchError(E_STX_DECLARATION_EXPECTED))
        {
            sprintf(buffer, "declaration expected. \n");
        }
        else if (ERR_catchError(E_STX_UNEXPECTED_END_OF_FILE))
        {
            sprintf(buffer, "Unexpected end of file. \n");
        }
        else if (ERR_catchError(E_STX_PRECEDENCE_TYPE_EXPECTED))
        {
            sprintf(buffer, "Precedence type expected.");
        }
        else if (ERR_catchError(E_STX_BLOCK_OR_IF_STATEMENT_EXPECTED))
        {
            sprintf(buffer, "Block or if statement expected.");
        }
        else if (ERR_catchError(E_STX_CORRUPT_TOKEN))
        {
            sprintf(buffer, "Corrupt token (this error should never happen)");
        }
        else
        {
            sprintf(buffer, "Unhandled syntax error.\n");
        }
        callback(buffer);
        goto cleanup;
    }
    printf("Syntax checking finished.\n");
    sprintf(fn,"%s.rawtree", fileName);
    FILE *f = fopen(fn, "w+t");
    STX_transversePreorder(parserResult.tree, dumpTreeCallback, f);
    fclose(f);
    // Semantic checking
    checkerResult = SMC_checkSyntaxTree(parserResult.tree);
    if (ERR_isError())
    {
        const struct STX_NodeAttribute *attr = STX_getNodeAttribute(checkerResult.lastNode);
        struct STX_SyntaxTreeNode *node = checkerResult.lastNode;
        sprintf(
            buffer,
            "[%d; %d] - [%d; %d] %.*s (node: %s): ",
            node->beginLine,
            node->beginColumn,
            node->endLine,
            node->endColumn,
            attr ? attr->nameLength : 0,
            attr ? attr->name : "",
            STX_nodeTypeToString(node->nodeType)
        );
        callback(buffer);
        if (ERR_catchError(E_SMC_CORRUPT_SYNTAX_TREE))
        {
            sprintf(buffer, "Syntax tree is corrupt!\n");
        }
        else if (ERR_catchError(E_SMC_REDEFINITION_OF_SYMBOL))
        {
            sprintf(buffer, "Redefinition of symbol!\n");
        }
        else if (ERR_catchError(E_SMC_TOO_FEW_PARAMETERS))
        {
            sprintf(buffer, "Too few parameters given to this function. \n");
        }
        else if (ERR_catchError(E_SMC_TOO_MANY_PARAMETERS))
        {
            sprintf(buffer, "Too many parameters given to this function. \n");
        }
        else if (ERR_catchError(E_SMC_EMPTY_PLATFORM_BLOCK))
        {
            sprintf(buffer, "Platform block is empty. \n");
        }
        else if (ERR_catchError(E_SMC_BREAK_IS_NOT_IN_LOOP_OR_CASE_BLOCK))
        {
            sprintf(buffer, "Break is not in loop or case block. \n");
        }
        else if (ERR_catchError(E_SMC_UNDEFINED_SYMBOL))
        {
            sprintf(buffer, "Undefined symbol. \n");
        }
        else if (ERR_catchError(E_SMC_NOT_AN_OPERATOR))
        {
            sprintf(buffer, "The symbol is used like an operator, but it's not an operator. \n");
        }
        else if (ERR_catchError(E_SMC_NOT_A_NAMESPACE))
        {
            sprintf(buffer, "The symbol is not a namespace. \n");
        }
        else if (ERR_catchError(E_SMC_AMBIGUOS_NAME))
        {
            sprintf(buffer, "Ambiguous symbol name. \n");
        }
        else
        {
            sprintf(buffer, "Unknown error. \n");
        }

        callback(buffer);
        goto cleanup;
    }
    sprintf(fn,"%s.tree", fileName);
    f = fopen(fn, "w+t");
    STX_transversePreorder(parserResult.tree, dumpTreeCallback, f);
    fclose(f);
cleanup:
    free(fn);
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
