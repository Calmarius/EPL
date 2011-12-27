#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "assocarray.h"

#define ELEMENTS 3

struct AssocBlock
{
    struct ASSOC_KeyValuePair keys[ELEMENTS * 2];
    struct AssocBlock *pointers[ELEMENTS * 2 + 1];
    int elementCount;
    struct AssocBlock *parent;
};

static struct AssocBlock *createBlock(struct AssocBlock *parent)
{
    struct AssocBlock *block = malloc(sizeof(*block));
    memset(block, 0, sizeof(*block));
    block->parent = parent;
    return block;
}

static void cleanupBlock(struct AssocBlock *block)
{
    int i;
    for (i = 0; i <= block->elementCount; i++)
    {
        if (block->pointers[i]) cleanupBlock(block->pointers[i]);
    }
    free(block);
}

static int addToBlock(
    struct ASSOC_Array *array,
    struct AssocBlock *block,
    struct ASSOC_KeyValuePair *kvp,
    struct AssocBlock *newBlockPtr,
    int needRecursion
);

static void checkBlockForSplit(
    struct ASSOC_Array *array,
    struct AssocBlock *block)
{
    int middleIndex;
    struct AssocBlock *newBlock;
    struct AssocBlock *parentBlock;
    int i;

    if (block->elementCount <= ELEMENTS) return;
    // at this point we need to split the block.
    // Pick the middle element
    middleIndex = block->elementCount >> 1;

    // Create a new block and put the elements and pointert to it after the middle.
    newBlock = createBlock(block->parent);
    newBlock->pointers[0] = block->pointers[middleIndex + 1];
    for (i = middleIndex + 1; i < block->elementCount; i++)
    {
        newBlock->keys[newBlock->elementCount] = block->keys[i];
        newBlock->pointers[newBlock->elementCount + 1] = block->pointers[i + 1];
        newBlock->elementCount++;
    }
    // truncate the block before the middle.
    block->elementCount = middleIndex;


    // Get the parent node.
    if (!block->parent)
    {
        // create new root node
        parentBlock = createBlock(0);
        array->root = parentBlock;
        parentBlock->pointers[0] = block;
        block->parent = parentBlock;
        newBlock->parent = parentBlock;
    }
    else
    {
        parentBlock = block->parent;
    }

    // Set the parent block of the children of the new block.
    for (i = 0; i <= newBlock->elementCount; i++)
    {
        if (newBlock->pointers[i])
        {
            newBlock->pointers[i]->parent = newBlock;
        }
    }

    // add the middle node to the parent block
    addToBlock(array, parentBlock, &block->keys[middleIndex], newBlock, 0);


}

static int addToBlock(
    struct ASSOC_Array *array,
    struct AssocBlock *block,
    struct ASSOC_KeyValuePair *kvp,
    struct AssocBlock *newBlockPtr,
    int needRecursion
)
{
    int insertPos;
    int i;

    // find the position where to insert.
    for (insertPos = 0; insertPos < block->elementCount; insertPos++)
    {
        int result = strcmp(kvp->key, block->keys[insertPos].key);
        if (result < 0)
        {
            break;
        }
        else if (!result)
        {
            return 0;
        }
    }
    // if the block has leaf nodes then, try to add it to the leaf node.
    if (needRecursion && (block->pointers[insertPos]))
    {
        return addToBlock(array, block->pointers[insertPos], kvp, newBlockPtr, 1);
    }
    for (i = block->elementCount; i > insertPos; i--)
    {
        block->keys[i] = block->keys[i - 1];
        block->pointers[i + 1] = block->pointers[i];
    }
    block->keys[insertPos] = *kvp;
    block->pointers[insertPos + 1] = newBlockPtr;
    if (newBlockPtr)
    {
        newBlockPtr->parent = block;
    }
    block->elementCount++;

    checkBlockForSplit(array, block);

    return 1;
}

static int transverseBlock(
    struct AssocBlock *block,
    int level,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
)
{
    int i;
    for (i = 0; i < block->elementCount; i++)
    {
        if (!callback(&block->keys[i], level, i, userData)) return 0;
    }
    for (i = 0; i <= block->elementCount; i++)
    {
        if (block->pointers[i])
        {
            if (!transverseBlock(block->pointers[i], level + 1, callback, userData))
            {
                return 0;
            }
        }
    }

    return 1;

}

void dumpNode(struct AssocBlock *block, int level)
{
    int i;

    if (!block) return;
    printf("%*s[", level * 4, "");
    for (i = 0; i < block->elementCount; i++)
    {
        if (i) printf(", ");
        printf("%s", block->keys[i].key);
    }
    printf("]          this: %p, parent: %p\n", block, block->parent);
    for (i = 0; i <= block->elementCount; i++)
    {
        dumpNode(block->pointers[i], level + 1);
    }
}

void ASSOC_initializeArray(struct ASSOC_Array *array)
{
    array->root = createBlock(0);
}


void ASSOC_cleanupArray(struct ASSOC_Array *array)
{
    cleanupBlock(array->root);
}


int ASSOC_insert(struct ASSOC_Array *array, const char *key, void *value)
{
    struct ASSOC_KeyValuePair kvp;
    kvp.key = key;
    kvp.value = value;
    return addToBlock(array, array->root, &kvp, 0, 1);
}


/*int ASSOC_remove(struct ASSOC_Array *array, const char *key);
int ASSOC_find(struct ASSOC_Array *array, const char *key);
*/
int ASSOC_preorderTransversal(
    struct ASSOC_Array *array,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
)
{
    return transverseBlock(array->root, 0, callback, userData);
}

void ASSOC_dump(struct ASSOC_Array *array)
{
    dumpNode(array->root, 0);
}


