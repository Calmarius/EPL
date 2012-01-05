#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "syntax.h"

struct SMC_CheckerResult
{
    struct STX_SyntaxTreeNode *lastNode;
};

struct SMC_CheckerResult SMC_checkSyntaxTree(struct STX_SyntaxTree *syntaxTree);

#endif // SEMANTIC_H
