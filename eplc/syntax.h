#ifndef SYNTAX_H
#define SYNTAX_H

#include "lexer.h"

struct LEX_LexerToken;

/**
 * A list of node types.
 */
enum STX_NodeType
{
    STX_ROOT,
    STX_MODULE,
    STX_DECLARATIONS,
    STX_BLOCK,
    STX_TYPE,
    STX_VARDECL,
    STX_TYPE_PREFIX,
    STX_PARAMETER,
    STX_ARGUMENT_LIST,
    STX_FUNCTION,
    STX_EXPRESSION_STATEMENT,
    STX_TERM,
    STX_RETURN_STATEMENT,
    STX_EXPRESSION,
    STX_OPERATOR,
    STX_IF_STATEMENT,
    STX_LOOP_STATEMENT,
    STX_ASSIGNMENT,
    STX_NAMESPACE,
    STX_USING,
    STX_QUALIFIED_NAME,
    STX_QUALIFIED_NAME_PART,
    STX_STRUCT,
    STX_FIELD,
    STX_COMMENT,
    STX_FUNCPTR,
    STX_SWITCH,
    STX_CASE,
    STX_CONTINUE,
    STX_BREAK,
    STX_OPERATOR_FUNCTION,
    STX_PLATFORM,
    STX_FOR_PLATFORMS,
    STX_PARAMETER_LIST,
    STX_PLATFORM_LIST,
};

/**
 * A list of module types.
 */
enum STX_ModuleAttribute
{
    STX_MOD_EXE,
    STX_MOD_DLL,
    STX_MOD_LIB
};

/**
 * A list of type prefixes.
 */
enum STX_TypePrefix
{
    STX_TP_NONE,
    STX_TP_POINTER,
    STX_TP_LOCALPTR,
    STX_TP_BUFFER,
    STX_TP_HANDLE,
    STX_TP_STATICPTR
};

/**
 * A list of parameter directions.
 */
enum STX_ParameterDirection
{
    STX_PD_IN,
    STX_PD_OUT,
    STX_PD_REF
};

/**
 * A list of term types (some of them is unused.)
 */
enum STX_TermType
{
   STX_TT_SIMPLE,
   STX_TT_FUNCTION_CALL,
   STX_TT_CAST_EXPRESSION,
   STX_TT_PARENTHETICAL,
   STX_TT_UNARY_OPERATOR,
   STX_TT_ARRAY_SUBSCRIPT
};

/**
 * A list of basic primitive types.
 */
enum STX_PrimitiveTypeType
{
    STX_STT_SIGNED_INT,
    STX_STT_UNSIGNED_INT,
    STX_STT_FLOAT
};

/**
 * A list of meta types
 */
enum STX_TypeOfType
{
    STX_TYT_NONE, ///< undefined type
    STX_TYT_SIMPLE, ///< Primitive type
    STX_TYT_USERTYPE, ///< User defined type (a TYPE node)
};

/**
 * Stores information abount on type.
 */
struct STX_TypeInformation
{
    union
    {
        struct STX_SyntaxTreeNode *typeNode; ///< The node of the type.
        struct
        {
            enum STX_PrimitiveTypeType type; ///< type of the primitive type
            int bitCount; ///< bit count of the type
            const char *attribs; ///< attribute string of the type
            int attribLength; ///< length of the string.
        }; ///< or a primitive type
    };
    enum STX_TypeOfType metaType; ///< Type of the type.
    enum STX_TypePrefix prefix; ///< Prefix of the type
    int assignable; ///< Nonzero if this is an assignable reference type.
};

/**
 * Stores the attributes of a single node
 */
struct STX_NodeAttribute
{
    union
    {
        struct
        {
            enum STX_ModuleAttribute type; ///< type of the mopdul
        } moduleAttributes; ///< For the MODULE node
        struct
        {
            enum STX_TypePrefix type; ///< prefix type
            int elements; ///< element count of a buffer
        } typePrefixAttributes; ///< for the TYPE_PREFIX node
        struct
        {
            enum STX_PrimitiveTypeType type; ///< Type of the primitive type
            int bitCount; ///< bit count of it
            const char *attribute; ///< attribute string
            int attributeLength; ///< length of the attribute string.
            int isPrimitive; ///< Nonzero if the type is primitive.
        } typeAttributes; ///< for the the TYPE node
        struct
        {
            enum STX_ParameterDirection direction; ///< direction of the parameter
        } parameterAttributes; ///< for the PARAMETER node
        struct
        {
            enum LEX_TokenType type; ///< The operator token.
        } operatorAttributes; ///< for the OPERATOR node
        struct
        {
            enum STX_TermType termType; ///<  Type of the term
            enum LEX_TokenType tokenType; ///< Token type of the term.
        } termAttributes; ///< for the TERM node
        struct
        {
            int caseValue; ///< The value in the case label.
            int isDefault; ///< Nonzero if the case bock is the 'default' block
        } caseAttributes; ///< for the CASE node
        struct
        {
            enum LEX_TokenType precedence; ///< Precedence of the declared operator
            const char *externalLocation; ///< The name of the file the function is loaded from.
            int externalLocationLength; ///< Length of the file name
            const char *externalFileType; ///< The type of the file the function is loaded from.
            int externalFileTypeLength; ///< Length of the file type.
            int isExternal; ///< Nonzero if the function is external.
        } functionAttributes; ///< for the FUNCTION and OPERATOR_FUNCTION node.
        struct
        {
            int levels; ///< The numbers of levels to break or continue.
            int associatedNodeId; ///< The id of the node the statement break or continue.
        } breakContinueAttributes; ///< for the BREAK and CONTINUE nodes.
        struct
        {
            int hasBreak; ///< Nonzero if the loop has a break node in it. It's an error not having one.
        } loopAttributes; ///< for the LOOP node.
    };
     /** Associated type information. (used mainly in TERM and OPERATOR nodes)
      * during type compatibility checking.
      */
    struct STX_TypeInformation typeInformation;
    /**
     * Name of the node. It's meaning depends on the node
     */
    const char *name;
    int nameLength; ///< Length of the name
    /**
     * Comment of the node. The contents of the documentation comments before the node, or
     * documentation back comments after the node is set in this field.
     */
    const char *comment;
    int commentLength; ///< The length of the comment.
    int symbolDefinitionNodeId; ///< Id of the node that defined this node.

    struct STX_SyntaxTree *belongsTo; ///< The syntax tree the node belong to.

};

/**
 * Stores a single syntax tree node.
 */
struct STX_SyntaxTreeNode
{
    int id; ///< id of the node.
    int parentIndex; ///< Id of the parent node
    int firstChildIndex; ///< Id of the first child.
    int lastChildIndex; ///< Id of the last child.
    int nextSiblingIndex; ///< Id of the next sibling.
    int previousSiblingIndex; ///< Id of the previous sibling.
    struct STX_NodeAttribute attribute; ///< Attribute of the node.
    int beginLine; ///< Line of the beginning character position of the node.
    int beginColumn; ///< Column of the beginning character position of the node.
    int endLine; ///< Line of the first character after the node
    int endColumn; ///< Column of the first character after the node
    enum STX_NodeType nodeType; ///< Type of the node.
    struct STX_SyntaxTree *belongsTo; ///< Reference to the syntax tree the node belongs to.
    int inScopeId; ///< The id of the scope the node is in.
    int definesScopeId; ///< The if of the scope the defines.
};

/**
 * Stores the syntax tree itself.
 */
struct STX_SyntaxTree
{
    struct STX_SyntaxTreeNode *nodes; ///< Dynamic array for the nodes
    int nodesAllocated; ///< Count of allocated nodes
    int nodeCount; ///< Count of nodes

    int rootNodeIndex; ///< Index of the root node (usually 0.)

};

/**
 * Stores the result of the parser.
 */
struct STX_ParserResult
{
    struct STX_SyntaxTree *tree; ///< The resulting syntax tree

    int line; ///< The line of the character where the parsing finished.
    int column; ///< The column of the character where the parsing finished.
};

/**
 * Stores a tree transversal iterator.
 */
struct STX_TreeIterator
{
    struct STX_SyntaxTreeNode *current; ///< Current node
    struct STX_SyntaxTreeNode *iteratorRoot; ///< The root node of the subtree the iterator transverses
    struct STX_SyntaxTreeNode *previous; ///< The previous node
    int isSkipSubTree; ///< Nonzero if the iterator must not enter the subtree of the current node.
};

/**
 * Builds the syntax tree
 *
 * @param [in] tokens The array of tokens to build the tree from.
 * @param [in] tokenCount The count of tokens in the array.
 *
 * @return The parser result which stores the syntax tree. On error the syntax
 *      tree will be invalid. Use the global ERR module to query the error.
 *      The line and column in the result refers to the location of the error.
 */
struct STX_ParserResult STX_buildSyntaxTree(
    const struct LEX_LexerToken *tokens,
    int tokenCount
);

/**
 * Callback function to transverse the syntax tree.
 *
 * @param [in,out] node the current node being transversed.
 * @param [in] level The level of the node in the tree (also the level of the recursion).
 * @param [in] userData an user provided data.
 *
 * @retval Nonzero The transversal will continue
 * @retval 0 The transversal will stop.
 */
typedef int (*STX_TransverseCallback)(
    struct STX_SyntaxTreeNode *node,
    int level,
    void *userData
);

/**
 * Destroys the syntax tree an releases the allocated resources.
 *
 * @param [in] tree The syntax tree.
 */
void STX_destroySyntaxTree(struct STX_SyntaxTree *tree);

/**
 * Trasverses the tree preorder way.
 *
 * @param [in,out] tree The tree to transverse.
 * @param [in] callback The callback function to call on the nodes.
 * @param [in] userData An user provided data that will be passed to the callback.
 *
 * @retval Nonzero If the transversal is finished successfully.
 * @retval 0 If the callback terminated the transversal.
 */
int STX_transversePreorder(struct STX_SyntaxTree *tree, STX_TransverseCallback callback, void *userData);

/**
 * @param [in] node subject.
 *
 * @return The node's attributes.
 */
struct STX_NodeAttribute *STX_getNodeAttribute(struct STX_SyntaxTreeNode *node);

/**
 * @param [in] node subject.
 *
 * @return The node's parent node. Returns 0 if the node don't have a parent.
 */
struct STX_SyntaxTreeNode *STX_getParentNode(const struct STX_SyntaxTreeNode *node);

/**
 * @param [in] node subject.
 *
 * @return The node's first child node. Returns 0 if the node don't have a first child.
 */
struct STX_SyntaxTreeNode *STX_getFirstChild(const struct STX_SyntaxTreeNode *node);

/**
 * @param [in] node subject.
 *
 * @return The node's last child node. Returns 0 if the node don't have a last child.
 */
struct STX_SyntaxTreeNode *STX_getLastChild(const struct STX_SyntaxTreeNode *node);

/**
 * @param [in] node subject.
 *
 * @return The node's next sibling node. Returns 0 if the node don't have a next sibling.
 */
struct STX_SyntaxTreeNode *STX_getNext(const struct STX_SyntaxTreeNode *node);

/**
 * @param [in] node subject.
 *
 * @return The node's previous sibling node. Returns 0 if the node don't have a previous sibling.
 */
struct STX_SyntaxTreeNode *STX_getPrevious(const struct STX_SyntaxTreeNode *node);

/**
 * Initializes an iterator for transversal starting from the subtree whose root is the given node.
 *
 * @param [in,out] iterator Iterator to initialize.
 * @param [in] node The root node of the syntax tree to transverse.
 */
void STX_initializeTreeIterator(struct STX_TreeIterator *iterator, struct STX_SyntaxTreeNode *node);

/**
 * @param [in,out] iterator The iterator
 *
 * @return The next node of the iteration using preorder transversal.
 */
struct STX_SyntaxTreeNode *STX_getNextPreorder(struct STX_TreeIterator *iterator);

/**
 * @param [in,out] iterator The iterator
 *
 * @return The next node of the iteration using postorder transversal.
 */
struct STX_SyntaxTreeNode *STX_getNextPostorder(struct STX_TreeIterator *iterator);

/**
 * Sets the iterator to skip the subtree of the current node. The next iteration
 * will clear this state.
 *
 * @param [in,out] iterator to set the state on.
 * @param [in] skip Nonzero means setting the skipping, zero means clearing it.
 */
void STX_setSkipSubtree(struct STX_TreeIterator *iterator, int skip);

/**
 * Removes the node from the syntax tree. It won't deallocate it!
 *
 * @param [in,out] tree The syntax tree of the node.
 * @param [in] node The node to delete.
 */
void STX_removeNode(struct STX_SyntaxTree *tree, struct STX_SyntaxTreeNode *node);

/**
 * @param [in] nodeType A node type.
 *
 * @return The node type, converted to string.
 */
const char *STX_nodeTypeToString(enum STX_NodeType nodeType);

/**
 * @param [in] nodeType A meta primitive type.
 *
 * @return The meta primitive type, converted to string.
 */
const char * STX_PrimitiveTypeTypeToString(enum STX_PrimitiveTypeType type);

/**
 * Appends child to a node.
 *
 * @param [in,out] tree The syntax tree of the nodes.
 * @param [in,out] node The node to add the child node to.
 * @param [in,out] child The new child node.
 */
void STX_appendChild(
    struct STX_SyntaxTree *tree,
    struct STX_SyntaxTreeNode *node,
    struct STX_SyntaxTreeNode *child);

/**
 * Removes all child nodes from a node. It won't deallocate them.
 *
 * @param node Subject.
 */
void STX_removeAllChildren(struct STX_SyntaxTreeNode *node);

#endif // SYNTAX_H
