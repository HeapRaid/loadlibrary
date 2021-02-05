#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <assert.h>
#include <stdlib.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"

static const uint16_t kTempPath[] = L".\\FAKETEMP\\";
static const uint16_t kFakePath[] = L"C:\\dummy\\dummy.exe";
static LPSTR kTempPathA = ".\\FAKETEMP\\";
static const char kFakeBasePathA[] = "C:\\dummy\\";

STATIC DWORD WINAPI GetTempPathW(DWORD nBufferLength, PVOID lpBuffer)
{
    DebugLog("%u, %p", nBufferLength, lpBuffer);

    memcpy(lpBuffer, kTempPath, sizeof(kTempPath));

    return sizeof(kTempPath) - 2;
}

STATIC DWORD WINAPI GetTempPathA(DWORD nBufferLength, PVOID lpBuffer)
{
    DebugLog("%u, %p", nBufferLength, lpBuffer);

    memcpy(lpBuffer, kTempPathA, strlen(kTempPathA));

    return strlen(kTempPathA) - 2;
}

STATIC DWORD WINAPI GetLogicalDrives(void)
{
    DebugLog("");

    return 1 << 2;
}

#define DRIVE_FIXED 3

STATIC UINT WINAPI GetDriveTypeW(PWCHAR lpRootPathName)
{
    char *path = CreateAnsiFromWide(lpRootPathName);
    DebugLog("%p [%s]", lpRootPathName, path);
    free(path);
    return DRIVE_FIXED;
}

STATIC DWORD WINAPI GetLongPathNameA(LPCSTR lpszShortPath,
                                     LPSTR lpszLongPath,
                                     DWORD cchBuffer)
{
    DebugLog("%p [%s]", lpszShortPath, lpszShortPath);

    // For now we just return the 8.3 format path as the long path
    if (cchBuffer > strlen(lpszShortPath)) {
        memcpy(lpszLongPath, lpszShortPath, sizeof(lpszShortPath));
    }

    return strlen(lpszShortPath);
}

STATIC DWORD WINAPI GetLongPathNameW(LPCWSTR lpszShortPath,
                                     LPWSTR lpszLongPath,
                                     DWORD cchBuffer)
{
    DebugLog("");

    // For now we just return the 8.3 format path as the long path
    if (cchBuffer > CountWideChars(lpszShortPath)) {
        memcpy(lpszLongPath, lpszShortPath, CountWideChars(lpszShortPath) * sizeof(WCHAR));
    }

    return CountWideChars(lpszShortPath);
}

STATIC DWORD WINAPI RtlGetFullPathName_U(LPCWSTR lpFileName,
                                         DWORD nBufferLength,
                                         LPWSTR lpBuffer,
                                         LPWSTR *lpFilePart)
{
    LPSTR lpFileNameA = CreateAnsiFromWide((PVOID)lpFileName);

    DebugLog("%p [%s], %d, %p, %p", lpFileName, lpFileNameA, nBufferLength, lpBuffer, lpFilePart);

    if (nBufferLength > CountWideChars(lpFileName)) {
        memcpy(lpBuffer, lpFileName, CountWideChars(lpFileName) * sizeof(WCHAR));
    }
    return CountWideChars(lpFileName) * sizeof(WCHAR);
}

STATIC DWORD WINAPI GetFinalPathNameByHandleW(HANDLE hFile,
                                              LPWSTR lpszFilePath,
                                              DWORD cchFilePath,
                                              DWORD dwFlags)
{
    DebugLog("%p, %p, %#x", hFile, lpszFilePath, dwFlags);

    if (cchFilePath > CountWideChars(kFakePath)) {
        memcpy(lpszFilePath, kFakePath, CountWideChars(kFakePath)*sizeof(WCHAR));
    }

    return CountWideChars(kFakePath);
}

DECLARE_CRT_EXPORT("GetTempPathW", GetTempPathW);
DECLARE_CRT_EXPORT("GetTempPathA", GetTempPathA);
DECLARE_CRT_EXPORT("GetLogicalDrives", GetLogicalDrives);
DECLARE_CRT_EXPORT("GetDriveTypeW", GetDriveTypeW);
DECLARE_CRT_EXPORT("GetLongPathNameA", GetLongPathNameA);
DECLARE_CRT_EXPORT("GetLongPathNameW", GetLongPathNameW);
DECLARE_CRT_EXPORT("RtlGetFullPathName_U", RtlGetFullPathName_U);
DECLARE_CRT_EXPORT("GetFinalPathNameByHandleW", GetFinalPathNameByHandleW);
