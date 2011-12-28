#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

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

#define STATIC_SIZE(array) sizeof(array) / sizeof(array[0])

int main()
{
    struct ASSOC_Array array;
    const char *strs[] =
    {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    };

    srand(time(0));

    setbuf(stdout, 0);

    ASSOC_initializeArray(&array);
    int i;
    for (i = 0; i < 100000; i++)
    {
        int a = rand() % STATIC_SIZE(strs);
        int b = rand() % STATIC_SIZE(strs);
        const char *s = strs[a];
        strs[a] = strs[b];
        strs[b] = s;
    }
    for (i = 0; i < STATIC_SIZE(strs); i++)
    {
        printf("Adding %s\n", strs[i]);
        assert(ASSOC_insert(&array, strs[i], 0));
        ASSOC_dump(&array); printf("\n");
    }
    for (i = 0; i < 100000; i++)
    {
        int a = rand() % STATIC_SIZE(strs);
        int b = rand() % STATIC_SIZE(strs);
        const char *s = strs[a];
        strs[a] = strs[b];
        strs[b] = s;
    }
    printf("Adding stuff again!\n");
    for (i = 0; i < STATIC_SIZE(strs); i++)
    {
        printf("Adding %s\n", strs[i]);
        assert(!ASSOC_insert(&array, strs[i], 0));
        ASSOC_dump(&array); printf("\n");
    }
    for (i = 0; i < 100000; i++)
    {
        int a = rand() % STATIC_SIZE(strs);
        int b = rand() % STATIC_SIZE(strs);
        const char *s = strs[a];
        strs[a] = strs[b];
        strs[b] = s;
    }
    for (i = 0; i < STATIC_SIZE(strs); i++)
    {
        printf("Removing %s\n", strs[i]);
        assert(ASSOC_remove(&array, strs[i]));
        ASSOC_dump(&array); printf("\n");
    }
    for (i = 0; i < 100000; i++)
    {
        int a = rand() % STATIC_SIZE(strs);
        int b = rand() % STATIC_SIZE(strs);
        const char *s = strs[a];
        strs[a] = strs[b];
        strs[b] = s;
    }
    printf("Removing stuff again!\n");
    for (i = 0; i < STATIC_SIZE(strs); i++)
    {
        printf("Removing %s\n", strs[i]);
        assert(!ASSOC_remove(&array, strs[i]));
        ASSOC_dump(&array); printf("\n");
    }
    ASSOC_cleanupArray(&array);

    fgetc(stdin);

    return 0;
}
