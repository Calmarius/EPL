#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "assocarray.h"

#define ELEMENTS 3

/**
 * Stores one data block
 */
struct AssocBlock
{
    /**
     * Stores the key value pairs. It's elements * 2 in size
     * to store enough elements when merging.
     */
    struct ASSOC_KeyValuePair keys[ELEMENTS * 2];
    /**
     * Stores the pointers to the child blocks.
     */
    struct AssocBlock *pointers[ELEMENTS * 2 + 1];
    /**
     * Stores the count of the elements in the block
     */
    int elementCount;
    /**
     * Stores a reference to the parent node. For the root node it must be 0.
     */
    struct AssocBlock *parent;
};

/**
 * Creates a new block and sets its parent node.
 *
 * @param [in] parent The parent node
 *
 * @return New block, returns 0 on failure.
 */
static struct AssocBlock *createBlock(struct AssocBlock *parent)
{
    // allocate memory
    struct AssocBlock *block = malloc(sizeof(*block));
    if (!block) return 0;
    // zero the space
    memset(block, 0, sizeof(*block));
    // set parent
    block->parent = parent;
    return block;
}

/**
 * Frees the memory allocated for the block.
 *
 * @param [in] recursive Set to non-zero to release the child nodes too.
 */
static void cleanupBlock(struct AssocBlock *block, int recursive)
{
    int i;
    // release child nodes
    if (recursive)
    {
        for (i = 0; i <= block->elementCount; i++)
        {
            if (block->pointers[i])
            {
                cleanupBlock(block->pointers[i], recursive);
            }
        }
    }
    // release memory
    free(block);
}

static int addToBlock(
    struct ASSOC_Array *array,
    struct AssocBlock *block,
    struct ASSOC_KeyValuePair *kvp,
    struct AssocBlock *newBlockPtr,
    int needRecursion
);

/**
 * Splits oversized block. If the block contains too many elements it's split into
 * two pieces. The middle element is added to the parent block as separator.
 * If the root node is oversized, it's split into two new blocks and a new empty root node
 * is created, the separator added to it.
 *
 * @param [in,out] array The associtiavive array the block is in.
 * @param [in,out] block The subject.
 */
static void checkBlockForSplit(
    struct ASSOC_Array *array,
    struct AssocBlock *block)
{
    int middleIndex;
    struct AssocBlock *newBlock;
    struct AssocBlock *parentBlock;
    int i;

    // If the block is not oversized, do nothing.
    if (block->elementCount <= ELEMENTS) return;
    // at this point we need to split the block.
    // Pick the middle element
    middleIndex = block->elementCount >> 1;

    // Create a new block.
    newBlock = createBlock(block->parent); //TODO: Add error handling here.
    // Move elements and pointers to the new block on the right side of the middle element.
    newBlock->pointers[0] = block->pointers[middleIndex + 1];
    for (i = middleIndex + 1; i < block->elementCount; i++)
    {
        newBlock->keys[newBlock->elementCount] = block->keys[i];
        newBlock->pointers[newBlock->elementCount + 1] = block->pointers[i + 1];
        newBlock->elementCount++;
    }
    // truncate the block before the middle.
    block->elementCount = middleIndex;

    // Check parent node
    if (!block->parent)
    {
        // create new root node, if no parent exist
        parentBlock = createBlock(0);
        array->root = parentBlock;
        // set the first pointer to the left block
        parentBlock->pointers[0] = block;
        // set the parent pointer to the new parent block.
        block->parent = parentBlock;
        newBlock->parent = parentBlock;
    }
    else
    {
        // If parent node exist, just use it.
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

int compareKey(
    const struct ASSOC_KeyValuePair *a,
    const struct ASSOC_KeyValuePair *b)
{
    int min = a->keyLength < b->keyLength ? a->keyLength : b->keyLength;
    int result = memcmp(a->key, b->key, min);
    if (!result)
    {
        return a->keyLength - b->keyLength;
    }
    return result;
}

/**
 * Adds a key-value pair to the block.
 *
 * @param [in,out] array The array the block is in.
 * @param [in,out] block The subject.
 * @param [in] kvp The key-value pair.
 * @param [in] newBlockPtr The right pointer of the newly added block is set to this value.
 * @param [in] needRecursion When set to zero, it adds the element to the
 *      'block', otherwise it will go down to the leaf nodes add the element there.
 *
 * @return non-zero on successful add. Zero otherwise.
 */

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
        int result = compareKey(kvp, &block->keys[insertPos]);
        if (result < 0)
        {
            break;
        }
        else if (!result)
        {
            return 0;
        }
    }
    // if needRecursion is set, propagate down to the leaf nodes and add the node there.
    if (needRecursion && (block->pointers[insertPos]))
    {
        return addToBlock(array, block->pointers[insertPos], kvp, newBlockPtr, 1);
    }
    // At this point we are in a leaf node or forced to add the element to this node.
    // Shift the elements and pointer right to make room the new element
    for (i = block->elementCount; i > insertPos; i--)
    {
        block->keys[i] = block->keys[i - 1];
        block->pointers[i + 1] = block->pointers[i];
    }
    // Set the new element and pointer.
    block->keys[insertPos] = *kvp;
    block->pointers[insertPos + 1] = newBlockPtr;
    // Set the parent node of the newly added child pointer, if set.
    if (newBlockPtr)
    {
        newBlockPtr->parent = block;
    }
    // Finally increase the element count.
    block->elementCount++;
    // Check the block if it's oversized.
    checkBlockForSplit(array, block);

    // success.
    return 1;
}

/**
 * Transverses a block in preorder way.
 *
 * @param [in,out] block Subject
 * @param [in] level Reserved, set to 0.
 * @param [in] callback The callback function will be called for each element.
 *      Arguments are:
 *          the current key-value pair,
 *          level of recursion,
 *          index of the element in block,
 *          user provided data.
 *      If it returns 0, the transversal is terminated.
 * @param [in] userData User provided data, it's passed to the callback.
 *
 * @return Returns zero, if the callback terinated the transversal, non-zero otherwise.
 */
static int transverseBlock(
    struct AssocBlock *block,
    int level,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
)
{
    int i;
    // transverse the elements first.
    for (i = 0; i < block->elementCount; i++)
    {
        if (!callback(&block->keys[i], level, i, userData)) return 0;
    }
    // then transverse the child nodes.
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

/**
 * Transverses a block in inorder way.
 *
 * @param [in,out] block Subject
 * @param [in] level Reserved, set to 0.
 * @param [in] callback The callback function will be called for each element.
 *      Arguments are:
 *          the current key-value pair,
 *          level of recursion,
 *          index of the element in block,
 *          user provided data.
 *      If it returns 0, the transversal is terminated.
 * @param [in] userData User provided data, it's passed to the callback.
 *
 * @return Returns zero, if the callback terinated the transversal, non-zero otherwise.
 */
static int inorderTransverseBlock(
    struct AssocBlock *block,
    int level,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
)
{
    int i;
    // transverse the elements first.
    for (i = 0; i < block->elementCount; i++)
    {
        if (block->pointers[i])
        {
            if (!inorderTransverseBlock(block->pointers[i], level + 1, callback, userData))
            {
                return 0;
            }
        }
        if (!callback(&block->keys[i], level, i, userData)) return 0;
    }
    if (block->pointers[i])
    {
        if (!inorderTransverseBlock(block->pointers[i], level + 1, callback, userData))
        {
            return 0;
        }
    }

    return 1;

}

/**
 * Dumps the contents of a block recursively. Used for diagnostics.
 *
 * @param [in] block Subject.
 * @param [in] level Reserved, set to 0.
 */
void dumpNode(const struct AssocBlock *block, int level)
{
    int i;

    // Dump the contents of the node, indent it with the current level.
    printf("%*s[", level * 4, "");
    for (i = 0; i < block->elementCount; i++)
    {
        if (i) printf(", ");
        printf("%.*s", block->keys[i].keyLength, block->keys[i].key);
    }
    printf("]          this: %p, parent: %p\n", block, block->parent);
    // Dump child nodes.
    for (i = 0; i <= block->elementCount; i++)
    {
        if (block->pointers[i])
        {
            // Check parent pointer consistency.
            if (block->pointers[i]->parent != block)
            {
                printf("*** INCONSISENT PARENT POINTER! ***\n");
            }
            dumpNode(block->pointers[i], level + 1);
        }
    }
}

void ASSOC_initializeArray(struct ASSOC_Array *array)
{
    array->root = createBlock(0);
}

/**
 * Merges the right block to the left, and releases the right.
 *
 * @param [in,out] array The array the blocks are in. Root node may be changed.
 * @param [in,out] left The left block.
 * @param [in,out] right The right block, it's contents will be added to the
 *      left and the block will be released.
 * @param [in] separator The separator that will separate the two merged blocks.
 *      If no separator given, the rightmost child of the left and the leftmost child
 *      of the right block will be also merged together. This process will be repeated
 *      till we reached the leaf nodes.
 */
void mergeBlocks(
    struct ASSOC_Array *array,
    struct AssocBlock *left,
    struct AssocBlock *right,
    struct ASSOC_KeyValuePair *separator)
{
    int i;
    // Get the right most child of the left block and the leftmost child of the right block.
    struct AssocBlock *rightMostOfLeft = left->pointers[left->elementCount];
    struct AssocBlock *leftMostOfRight = right->pointers[0];
    if (separator)
    {
        // If separator given add it to the left block, the separators right pointer
        // will be the leftmost child of the right block.
        left->keys[left->elementCount] = *separator;
        left->pointers[left->elementCount + 1] = right->pointers[0];
        // Set the parant node of the leftmost child.
        if (left->pointers[left->elementCount + 1])
        {
            left->pointers[left->elementCount + 1]->parent = left;
        }
        // We have one more elements now.
        left->elementCount++;
    }
    // Add the elements from the right node to the left.
    for (i = 0; i < right->elementCount; i++)
    {
        // We add the elements and pointers on the right side.
        left->keys[left->elementCount] = right->keys[i];
        left->pointers[left->elementCount + 1] = right->pointers[i + 1];
        // update the parent node of the child nodes.
        if (left->pointers[left->elementCount + 1])
        {
            left->pointers[left->elementCount + 1]->parent = left;
        }
        // We have one more element.
        left->elementCount++;
    }
    // Right block now has no elements in it.
    right->elementCount = 0;
    if (rightMostOfLeft && leftMostOfRight && !separator)
    {
        // If no separator given, we need to merge the rightmost of the left block
        // and the leftmost of the right block (since we will have only one pointer).
        mergeBlocks(array, rightMostOfLeft, leftMostOfRight, 0);
    }
    // If the block became to big, time to split it again.
    checkBlockForSplit(array, left);
    // Release the right block.
    cleanupBlock(right, 0);
}

int removeFromBlock(
    struct ASSOC_Array *array,
    struct AssocBlock *block,
    const char *key,
    int keyLength,
    int moveDown
);

/**
 * Eliminates blocks with too few elements.
 *
 * @param [in,out] array The arry the block is in. The root node of the array may be changed.
 * @param [in,out] block The undersized block.
 */
void eliminateUndersizedBlock(struct ASSOC_Array *array, struct AssocBlock *block)
{
    assert(block->elementCount < (ELEMENTS >> 1));
    if (block->parent)
    {
        // If the block has parent node...
        struct AssocBlock *parent = block->parent;
        int i;
        struct ASSOC_KeyValuePair *kvp;
        // Find the element which has pointers next to it to this block.
        assert(parent->elementCount); //< The parent node must have at lease one element.
        for (i = 0; i <= parent->elementCount; i++)
        {
            if (parent->pointers[i] == block)
            {
                if (i == parent->elementCount)
                {
                    kvp = &parent->keys[i - 1];
                }
                else
                {
                    kvp = &parent->keys[i];
                }
                break;
            }
        }
        // Remove that element, but use it as separator when merging the two child nodes.
        removeFromBlock(array, parent, kvp->key, kvp->keyLength, 1);
    }
    else
    {
        // If the node don't have a parent node, it's the root node
        if (block->pointers[0] && !block->elementCount)
        {
            // If it's not a leaf, and is empty,
            // set it's only child as root,
            array->root = block->pointers[0];
            array->root->parent = 0;
            // and release this node.
            cleanupBlock(block, 0);
        }
    }
}

/**
 * Removes element from a block. If the element is not found it will searches the
 * child nodes too.
 *
 * @param [in,out] array The array the block is in. The root node may be changed.
 * @param [in,out] block The block to begin searching in.
 * @param [in] key The key to find.
 * @param [in] moveDown When set to non-zero it uses the deleted element as
 *      separator of the two child nodes that will be merged.
 *
 * @return non-zero if the element was found an deleted, zero otherwise.
 */
int removeFromBlock(
    struct ASSOC_Array *array,
    struct AssocBlock *block,
    const char *key,
    int keyLength,
    int moveDown)
{
    int removalPos;

    // Find the element in the current block.
    for (removalPos = 0; removalPos < block->elementCount; removalPos++)
    {
        struct ASSOC_KeyValuePair kvp;
        kvp.key = key;
        kvp.keyLength = keyLength;
        int result = compareKey(&kvp, &block->keys[removalPos]);
        if (result < 0)
        {
            // The key not found.
            break;
        }
        else if (!result)
        {
            // Element found.
            int j;
            // Store the left and right child and the key-value pair to delete.
            struct AssocBlock *left = block->pointers[removalPos];
            struct AssocBlock *right = block->pointers[removalPos + 1];
            struct ASSOC_KeyValuePair kvp = block->keys[removalPos];
            // Remove the element by shifting the elements right to it leftward.
            for (j = removalPos; j < block->elementCount - 1; j++)
            {
                block->keys[j] = block->keys[j + 1];
                block->pointers[j + 1] = block->pointers[j + 2];
            }
            // Now the block has less elements.
            block->elementCount--;
            if (left && right)
            {
                // If the block wasn't a leaf block, merge the two child nodes
                // next the removed elements.
                // Use the removed element as separator, if moveDown is set.
                mergeBlocks(array, left, right, moveDown ? &kvp : 0);
            }
            if (block->elementCount < (ELEMENTS >> 1))
            {
                // If the block has too few elements eliminate it.
                eliminateUndersizedBlock(array, block);
            }
            // Element is successfully removed.
            return 1;
        }
    }
    // At this point the element is not found in the block.
    if (block->pointers[removalPos])
    {
        // Try the appropriate child node.
        return removeFromBlock(array, block->pointers[removalPos], key, keyLength, moveDown);
    }
    // At this point, we are in a leaf node, element not found, return zero.
    return 0;
}

void ASSOC_cleanupArray(struct ASSOC_Array *array)
{
    cleanupBlock(array->root, 1);
}


int ASSOC_insert(struct ASSOC_Array *array, const char *key, int keyLength, void *value)
{
    struct ASSOC_KeyValuePair kvp;
    kvp.key = key;
    kvp.keyLength = keyLength;
    kvp.value = value;
    return addToBlock(array, array->root, &kvp, 0, 1);
}


int ASSOC_remove(struct ASSOC_Array *array, const char *key, int keyLength)
{
    return removeFromBlock(array, array->root, key, keyLength, 0);
}

static void *findInBlock(struct AssocBlock *block, const char *key, int keyLength)
{
    int foundPos;
    // Find the element in the current block.
    for (foundPos = 0; foundPos < block->elementCount; foundPos++)
    {
        struct ASSOC_KeyValuePair kvp;
        kvp.key = key;
        kvp.keyLength = keyLength;
        int result = compareKey(&kvp, &block->keys[foundPos]);
        if (result < 0)
        {
            // The key not found.
            break;
        }
        else if (!result)
        {
            // Element found.
            return block->keys[foundPos].value;
        }
    }
    // At this point the element is not found in the block.
    if (block->pointers[foundPos])
    {
        // Try the appropriate child node.
        return findInBlock(block->pointers[foundPos], key, keyLength);
    }
    return 0;
}

void *ASSOC_find(struct ASSOC_Array *array, const char *key, int keyLength)
{
    return findInBlock(array->root, key, keyLength);
}

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

int ASSOC_transverseInorder(
    struct ASSOC_Array *array,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
)
{
    return inorderTransverseBlock(array->root, 0, callback, userData);
}


