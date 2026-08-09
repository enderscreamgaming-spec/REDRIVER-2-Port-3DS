#include <string.h>

#include "fs.h"

struct FS_FINDDATA
{
    int unused;
};

void FS_FixPathSlashes(char* pathbuff)
{
    if (!pathbuff)
        return;

    for (char* p = pathbuff; *p; p++)
    {
        if (*p == '\\')
            *p = '/';
    }
}

const char* FS_FindFirst(const char* wildcard, FS_FINDDATA** findData)
{
    (void)wildcard;

    if (findData)
        *findData = NULL;

    return NULL;
}

const char* FS_FindNext(FS_FINDDATA* findData)
{
    (void)findData;
    return NULL;
}

void FS_FindClose(FS_FINDDATA* findData)
{
    (void)findData;
}

bool FS_FindIsDirectory(FS_FINDDATA* findData)
{
    (void)findData;
    return false;
}
