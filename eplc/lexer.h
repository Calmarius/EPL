#ifndef LEXER_H
#define LEXER_H

/**
 * Stores the token types.
 */
enum LEX_TokenType
{
    LEX_UNKNOWN, ///< unknown token
    LEX_IDENTIFIER, ///< identifier token
    LEX_SEMICOLON, ///< ;
    LEX_LEFT_BRACE, ///< {
    LEX_RIGHT_BRACE, ///< }
    LEX_BUILT_IN_TYPE, ///< eg. $i32
    LEX_ASSIGN_OPERATOR, ///< :=
    LEX_FLOAT_NUMBER, ///< eg. 3.1415926536
    LEX_HEXA_INTEGER,  ///< eg. 0x789ABC
    LEX_DECIMAL_INTEGER, ///< eg. 666
    LEX_OCTAL_INTEGER, ///< eg. 0377
    LEX_ADD_OPERATOR, ///< +
    LEX_SUBTRACT_OPERATOR, ///< @verbatim - @endverbatim
    LEX_MULTIPLY_OPERATOR, ///< *
    LEX_LEFT_PARENTHESIS, ///< (
    LEX_RIGHT_PARENTHESIS, ///< )
    LEX_LEFT_BRACKET, ///< [
    LEX_RIGHT_BRACKET, ///< ]
    LEX_COMMA,  /// ,
    LEX_LESS_THAN, ///< <
    LEX_GREATER_THAN, ///< >
    LEX_EQUAL, ///< ==
    LEX_LESS_EQUAL_THAN, ///< <=
    LEX_GREATER_EQUAL_THAN, ///< >=
    LEX_NOT_EQUAL, ///< !=
    LEX_STRING, ///< eg. "Hello world!"
    LEX_PERIOD, ///< . (period)
    LEX_SCOPE_SEPARATOR, ///< ::
    LEX_DIVISION_OPERATOR, ///< /
    LEX_BLOCK_COMMENT, ///< eg. /*Comment*/
    LEX_EOL_COMMENT, ///< eg. // EOL comment
    LEX_DOCUMENTATION_BLOCK_COMMENT, ///< eg. /** Doc comment! */
    LEX_DOCUMENTATION_EOL_COMMENT, ///< eg. /// Doc EOL
    LEX_DOCUMENTATION_EOL_BACK_COMMENT, ///< eg. ///<
    LEX_COLON, ///< :
    LEX_SHIFT_RIGHT, ///< >>
    LEX_SHIFT_LEFT, ///< <<
    LEX_CHARACTER, ///< eg. #32

    LEX_KW_ELSE, ///< else (in if statement)
    LEX_KW_EXE, ///< exe (module type: executable)
    LEX_KW_MAIN, ///< main (Starts the main program block)
    LEX_KW_MODULE, ///< module (Starts the source code)
    LEX_KW_IF, ///< if  (If statement)
    LEX_KW_INC, ///< inc (Increment operator)
    LEX_KW_LOOP, ///< loop (Loop statement)
    LEX_KW_NEXT, ///< next (Begins increment block of the loop statement)
    LEX_KW_VARDECL, ///< vardecl (Begins variable declaration)
    LEX_KW_BREAK, ///< break (Break statement, breaks out from a loop)
    LEX_KW_DLL, ///< dll (Module type: shared library)
    LEX_KW_LIB, ///< lib (Module type: static library)
    LEX_KW_HANDLE, ///< handle (Pointer type: opaque pointer dereferencing is not allowed.)
    LEX_KW_POINTER, ///< pointer (Pointer type: pointer to data on heap.)
    LEX_KW_LOCALPTR, ///< localptr (Pointer type: pointer to data on stack.)
    LEX_KW_BUFFER, ///< buffer (Analogous to the C array: fixed amount of elements of the same type)
    LEX_KW_OF, ///< of (Linking word, like 'buffer[123] of $i')
    LEX_KW_TO, ///< to (Linking word, like 'staticptr to $u8')
    LEX_KW_IN, ///< in (Parameter direction for incoming arguments)
    LEX_KW_OUT, ///< out (Parameter direction for outgoing arguments)
    LEX_KW_REF, ///< ref (Parameter direction for transitive arguments, and an unary operator to get the address of a variable)
    LEX_KW_FUNCTION, ///< function (Starts function declaration)
    LEX_KW_RETURN, ///< return (Return statement)
    LEX_KW_CAST, ///< cast (Starts a type cast)
    LEX_KW_CLEANUP, ///< cleanup (Start the cleanup block of a function.)
    LEX_KW_NAMESPACE, ///< namespace (Starts namespace declaration)
    LEX_KW_USING, ///< using (Starts using declaration)
    LEX_KW_STRUCT, ///< struct (Starts struct delcaration)
    LEX_KW_FUNCPTR, ///< funcptr (Starts function pointer
    LEX_KW_CASE, ///< case (Case block in a switch statement)
    LEX_KW_CONTINUE, ///< continue (Continue statement)
    LEX_KW_SWITCH, ///< switch (Switch statement)
    LEX_KW_DEFAULT, ///< default (Default block of the switch statement)
    LEX_KW_OPERATOR, ///< operator (Starts declaration of an operator function)
    LEX_KW_MULTIPLICATIVE, ///< multiplicative (Precedence type in operator declarations)
    LEX_KW_ADDITIVE, ///< additive (Precedence type in operator declarations)
    LEX_KW_RELATIONAL, ///< relational (Precedence type in operator declarations)
    LEX_KW_NOT, ///< not (Logical not)
    LEX_KW_NEG, ///< neg (Arithmetic minus)
    LEX_KW_DEREF, ///< deref (Pointer dereference operator)
    LEX_KW_EXTERNAL, ///< external (Marks a function external: its code is in anouther module)
    LEX_KW_FOR, ///< for (Starts platform declaration)
    LEX_KW_STATICPTR, ///< staticptr (Pointer type: pointer to static data, writing through static pointer is not allowed)

    LEX_SPEC_EOF, ///< The last token means end of file.
    LEX_SPEC_DELETED, ///< The token which is marked as deleted.
};

/**
 * Stores info about a lexer token.
 */
struct LEX_LexerToken
{
    const char *start; ///< First character of the token.
    int length; ///< Length of the token.
    int beginLine; ///< Line of the first character.
    int beginColumn; ///< Column of the first character.
    int endLine; ///< Line of the character after the last character.
    int endColumn; ///< Column of the character after the last character.
    enum LEX_TokenType tokenType; ///< The type of the token.
};

/**
 * This struct stores the result of the lexer.
 */
struct LEX_LexerResult
{
    int tokenCount; ///< The count of recognized tokens.
    struct LEX_LexerToken *tokens; ///< The array of tokoens.
    int columnPos; ///< Column pos of the last successfully parsed character.
    int linePos; ///< Line of the last successfully parsed character.
    char **strings; ///< Array of binary strings.
    int stringCount; ///< Count of binary strings.
};

/**
 * Parses the source code into tokens
 *
 * @param [in] code The code to be parsed.
 *
 * @return The tokens
 */
struct LEX_LexerResult LEX_tokenizeString(const char *code);
/**
 * Cleans up the lexer result
 *
 * @param [in,out] lexerResult Lexer result to clean up.
 */
void LEX_cleanUpLexerResult(struct LEX_LexerResult *lexerResult);

#endif // LEXER_H
