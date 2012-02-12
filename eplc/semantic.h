#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "syntax.h"

/**
 * A struct which stores the result of the semantic checker.
 */
struct SMC_CheckerResult
{
    /**
     * The node the checker stopped (at usually the one where the error happened.)
     */
    struct STX_SyntaxTreeNode *lastNode;
};

/**
 * Checks the syntax tree provided.
 *
 * @return The result which contains the node the checker stopped on.
 */
struct SMC_CheckerResult SMC_checkSyntaxTree(struct STX_SyntaxTree *syntaxTree);

#endif // SEMANTIC_H
