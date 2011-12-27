#ifndef ASSOCARRAY_H
#define ASSOCARRAY_H

struct AssocBlock;

struct ASSOC_Array
{
    struct AssocBlock *root;
};

struct ASSOC_KeyValuePair
{
    const char *key;
    const void *value;
};

void ASSOC_initializeArray(struct ASSOC_Array *array);
void ASSOC_cleanupArray(struct ASSOC_Array *array);
int ASSOC_insert(struct ASSOC_Array *array, const char *key, void *value);
int ASSOC_remove(struct ASSOC_Array *array, const char *key);
int ASSOC_find(struct ASSOC_Array *array, const char *key);
int ASSOC_preorderTransversal(
    struct ASSOC_Array *array,
    int (*callback)(struct ASSOC_KeyValuePair*, int, int, void*),
    void *userData
);
void ASSOC_dump(struct ASSOC_Array *array);

#endif // ASSOCARRAY_H
