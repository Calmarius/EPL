#ifndef ASSOCARRAY_H
#define ASSOCARRAY_H

/**
 * Reserved internal structure.
 */
struct AssocBlock;

/**
 * Structure representing the associative array.
 * Don't mess with it's fields.
 */
struct ASSOC_Array
{
    struct AssocBlock *root;
};

/**
 * Stores a key-value pair.
 */
struct ASSOC_KeyValuePair
{
    const char *key;
    int keyLength;
    void *value;
};

/**
 * Initializes the array.
 */
void ASSOC_initializeArray(struct ASSOC_Array *array);
/**
 * Release the resources associated with the array.
 *
 * @param [in,out] array Subject.
 */
void ASSOC_cleanupArray(struct ASSOC_Array *array);
/**
 * Inserts element to the associative array.
 *
 * @param [in,out] array Subject.
 * @param [in] key The key of the element.
 * @param [in] keyLength Length of the key string.
 * @param [in] value The value of the element.
 *
 * @return non-zero on success, zero on failure.
 */
int ASSOC_insert(struct ASSOC_Array *array, const char *key, int keyLength, void *value);
/**
 * Removes elements from the associative array.
 *
 * @param [in,out] array Subject.
 * @param [in] key The key of the element to be removed.
 * @param [in] keyLength The length of the key string.
 *
 * @return non-zero on successfull removal, zero if the element is not found.
 */
int ASSOC_remove(struct ASSOC_Array *array, const char *key, int keyLength);
/**
 * Finds an element in the array.
 *
 * @param [in,out] array Subject.
 * @param [in] key The key of the element.
 * @param [in] keyLength The length of the key string.
 *
 * @return The corresponding value of the key, 0 if the key is not found.
 */
void *ASSOC_find(struct ASSOC_Array *array, const char *key, int keyLength);
/**
 * Transverses the array in preorder transversal.
 *
 * @param [in,out] array Subject.
 * @param [in] callback Callback provided by the user.
 *      Arguments:
 *          The current key-value pair
 *          The level of the recursion
 *          The element's index in the block.
 *          User provided data.
 *      Returns: non-zero if the transversal should continue, zero otherwise.
 * @param [in] userData User provided data for the callback.
 *
 * @return non-zero if the tree transversal finished, zero if the callback terminated it.
 */
int ASSOC_preorderTransversal(
    struct ASSOC_Array *array,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
);

/**
 * Transverses the array in inorder transversal.
 *
 * @param [in,out] array Subject.
 * @param [in] callback Callback provided by the user.
 *      Arguments:
 *          The current key-value pair
 *          The level of the recursion
 *          The element's index in the block.
 *          User provided data.
 *      Returns: non-zero if the transversal should continue, zero otherwise.
 * @param [in] userData User provided data for the callback.
 *
 * @return non-zero if the tree transversal finished, zero if the callback terminated it.
 */
int ASSOC_transverseInorder(
    struct ASSOC_Array *array,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
);
/**
 * Dumps the associative to the stdout. Useful for debugging.
 *
 * @param [in] array Subject.
 */
void ASSOC_dump(struct ASSOC_Array *array);

#endif // ASSOCARRAY_H
