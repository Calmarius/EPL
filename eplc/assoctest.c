/**
 * Copyright (c) 2012, Csirmaz Dávid
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

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
        assert(ASSOC_insert(&array, strs[i], strlen(strs[i]), 0));
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
        assert(!ASSOC_insert(&array, strs[i], strlen(strs[i]), 0));
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
        assert(ASSOC_remove(&array, strs[i], strlen(strs[i])));
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
        assert(!ASSOC_remove(&array, strs[i], strlen(strs[i])));
        ASSOC_dump(&array); printf("\n");
    }
    ASSOC_cleanupArray(&array);

    fgetc(stdin);

    return 0;
}
