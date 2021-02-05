#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "winnt_types.h"
#include "codealloc.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"
#include "Files.h"
#include "Find.h"

int find_next_file_count = 0;

HANDLE WINAPI FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    DebugLog("%p [%s], %p", lpFileName, lpFileName, lpFindFileData);

    return (HANDLE) "FIND";
}

BOOL WINAPI FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    union size {
        int64_t size;
        struct {
            int32_t low;
            int32_t high;
        };
    } Size;
    DebugLog("%p, %p", hFindFile, lpFindFileData);

    char *FakeFilePath = "C:\\fake_path\\fakename.exe";

    lpFindFileData->ftCreationTime.dwLowDateTime = 0x3AACDE5A;
    lpFindFileData->ftCreationTime.dwHighDateTime = 0x01D6E798;
    lpFindFileData->ftLastAccessTime.dwLowDateTime = 0xDCBD7A00;
    lpFindFileData->ftLastAccessTime.dwHighDateTime = 0x01D6E8C3;
    lpFindFileData->ftLastWriteTime.dwLowDateTime = 0xDCBD7A00;
    lpFindFileData->ftLastWriteTime.dwHighDateTime = 0x01D6E8C3;
    lpFindFileData->nFileSizeHigh = Size.high;
    lpFindFileData->nFileSizeLow = Size.low;
    lpFindFileData->dwReserved0 = 0;
    lpFindFileData->dwReserved1 = 0;
    memcpy(lpFindFileData->cFileName, FakeFilePath, strlen(FakeFilePath));
    lpFindFileData->cFileName[strlen(FakeFilePath)] = 0;
    lpFindFileData->cAlternateFileName[0] = 0;

    if (find_next_file_count)
        return FALSE;

    // Avoid infinite loop
    find_next_file_count = 1;

    return TRUE;
}

BOOL WINAPI FindClose(HANDLE hFindFile) // TO FIX
{
    DebugLog("%p", hFindFile);
    return TRUE;
}

HANDLE WINAPI FindFirstFileW(PWCHAR lpFileName, PVOID lpFindFileData)
{
    char *name = CreateAnsiFromWide(lpFileName);

    DebugLog("%p [%s], %p", lpFileName, name, lpFindFileData);

    free(name);

    SetLastError(ERROR_FILE_NOT_FOUND);

    return INVALID_HANDLE_VALUE;
}


DECLARE_CRT_EXPORT("FindFirstFileA", FindFirstFileA);
DECLARE_CRT_EXPORT("FindNextFileA", FindNextFileA);
DECLARE_CRT_EXPORT("FindFirstFileW", FindFirstFileW);
DECLARE_CRT_EXPORT("FindClose", FindClose);
