#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <assert.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "strings.h"


STATIC BOOL WINAPI DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, PHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
    DebugLog("%p, %p, %p, %p, %#x, %u, %#x", hSourceProcessHandle, hSourceHandle, hTargetProcessHandle, lpTargetHandle, dwDesiredAccess, bInheritHandle, dwOptions);

    // lol i dunno
    *lpTargetHandle = hSourceProcessHandle;
    return TRUE;
}

STATIC UINT WINAPI SetHandleCount(UINT handleCount)
{
    DebugLog("%u", handleCount);
    return handleCount;
}

STATIC BOOL WINAPI GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
    union size {
        int64_t size;
        struct {
            int32_t low;
            int32_t high;
        };
    } Size;

    DebugLog("%p, %p", hFile, lpFileInformation);
    int64_t curpos = ftell(hFile);
    fseek(hFile, 0, SEEK_END);
    int64_t size = ftell(hFile);
    fseek(hFile, curpos, SEEK_SET);
    Size.size = size;

    lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_COMPRESSED;
    lpFileInformation->ftCreationTime.dwLowDateTime = 0x3AACDE5A;
    lpFileInformation->ftCreationTime.dwHighDateTime = 0x01D6E798;
    lpFileInformation->ftLastAccessTime.dwLowDateTime = 0xDCBD7A00;
    lpFileInformation->ftLastAccessTime.dwHighDateTime = 0x01D6E8C3;
    lpFileInformation->ftLastWriteTime.dwLowDateTime = 0xDCBD7A00;
    lpFileInformation->ftLastWriteTime.dwHighDateTime = 0x01D6E8C3;
    lpFileInformation->dwVolumeSerialNumber = 0x01D6E8C3;
    lpFileInformation->nFileSizeHigh = Size.high;
    lpFileInformation->nFileSizeLow = Size.low;
    lpFileInformation->nNumberOfLinks = 0;
    lpFileInformation->nFileIndexHigh = 0xA;
    lpFileInformation->nFileIndexLow = 0xB;

    return true;
}

DECLARE_CRT_EXPORT("DuplicateHandle", DuplicateHandle);
DECLARE_CRT_EXPORT("SetHandleCount", SetHandleCount);
DECLARE_CRT_EXPORT("GetFileInformationByHandle", GetFileInformationByHandle);
