#include <stdio.h>

#include "assocarray.h"

int callback(struct ASSOC_KeyValuePair *kvp, int level, int index, void *userData)
{
    if (!index && level)
    {
        printf("]\n%*s[ ", level * 4, "");
    }
    else
    {
        if (index)
        {
            printf(", ");
        }
    }
    printf("%s", kvp->key);
    return 1;
}

int main()
{
    struct ASSOC_Array array;
    ASSOC_initializeArray(&array);
    ASSOC_insert(&array, "zizzz", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "zizzz", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "xyz", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "yay", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "bar", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "car", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "baz", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "foo", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "a", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "b", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "c", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "d", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "e", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "f", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "g", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "h", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "i", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "j", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "k", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "l", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "m", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "n", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "o", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "p", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "q", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "r", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "s", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "t", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "u", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "v", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "w", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "x", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "y", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_insert(&array, "z", 0);
    ASSOC_dump(&array); printf("\n");
    ASSOC_cleanupArray(&array);

    fgetc(stdin);

    return 0;
}
