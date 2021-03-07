#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <search.h>
#include <assert.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"

static HANDLE WINAPI LoadLibraryA(PCHAR lpFileName)
{
    // Translate path seperator.
    while (strchr(lpFileName, '\\'))
        *strchr(lpFileName, '\\') = '/';

    // I'm just going to tolower() everything.
    for (char *t = lpFileName; *t; t++)
        *t = tolower(*t);

    // Change the extension to so
    for (char *t = strrchr(lpFileName, '.'), i = 0;
        i < 4 && t && *t; t++, i++)
        *t = ".so"[i];

    HANDLE hModule = dlopen(lpFileName, RTLD_NOW);
    if (!hModule)
        DebugLog("FIXME: Failed to load %s", lpFileName);
    return hModule;
}

static HANDLE WINAPI LoadLibraryExW(PWCHAR lpFileName, HANDLE hFile, DWORD dwFlags)
{
    char *name = CreateAnsiFromWide(lpFileName);

    DebugLog("%p [%s], %p, %#x", lpFileName, name, hFile, dwFlags);

    HANDLE hModule = LoadLibraryA(name);

    free(name);

    return hModule;
}

static HANDLE WINAPI LoadLibraryW(PWCHAR lpFileName)
{
    DebugLog("%p", lpFileName);

    return (HANDLE) 'LOAD';
}

static PVOID WINAPI GetProcAddress(HANDLE hModule, PCHAR lpProcName)
{
    ENTRY key = { lpProcName }, *item;

    if (hModule == (HANDLE) NULL || hModule == (HANDLE) 'LOAD' || hModule == (HANDLE) 'MPEN' || hModule == (HANDLE) 'VERS' || hModule == (HANDLE) 'KERN')
    {
        if (hsearch_r(key, FIND, &item, &crtexports)) {
            return item->data;
        }
    }
    else if ((ULONG_PTR)lpProcName >> 16) // Ignore ordinals
    {
        PVOID ptr = dlsym(hModule, lpProcName);
        if (ptr) {
            return ptr;
        }
    }

    DebugLog("FIXME: %s unresolved", lpProcName);

    return NULL;
}

static VOID WINAPI FreeLibrary(PVOID hLibModule)
{
    DebugLog("FreeLibrary(%p)", hLibModule);
    dlclose(hLibModule);
}

static DWORD WINAPI GetModuleFileNameA(HANDLE hModule, PCHAR lpFilename, DWORD nSize)
{
    DebugLog("%p, %p, %u", hModule, lpFilename, nSize);

    strncpy(lpFilename, "C:\\dummy\\fakename.exe", nSize);

    return strlen(lpFilename);
}

static DWORD WINAPI GetModuleFileNameW(HANDLE hModule, PWCHAR lpFilename, DWORD nSize)
{
    DebugLog("%p, %p, %u", hModule, lpFilename, nSize);

    if (nSize > strlen("C:\\dummy\\fakename.exe")) {
        memcpy(lpFilename, L"C:\\dummy\\fakename.exe", sizeof(L"C:\\dummy\\fakename.exe"));
    }

    return strlen("C:\\dummy\\fakename.exe");
}

static HANDLE WINAPI GetModuleHandleA(PCHAR lpModuleName)
{
    DebugLog("%p [%s]", lpModuleName, lpModuleName);

    if (lpModuleName && memcmp(lpModuleName, L"mpengine.dll", sizeof(L"mpengine.dll")) == 0)
        return (HANDLE) 'MPEN';

    if (lpModuleName && memcmp(lpModuleName, L"bcrypt.dll", sizeof(L"bcrypt.dll")) == 0)
        return (HANDLE) 'LOAD';

    if (lpModuleName && memcmp(lpModuleName, L"KERNEL32.DLL", sizeof(L"KERNEL32.DLL")) == 0)
        return (HANDLE) 'KERN';

    if (lpModuleName && memcmp(lpModuleName, L"kernel32.dll", sizeof(L"kernel32.dll")) == 0)
        return (HANDLE) 'KERN';

    if (lpModuleName && memcmp(lpModuleName, L"version.dll", sizeof(L"version.dll")) == 0)
        return (HANDLE) 'VERS';

    // Change the extension to so
    for (char *t = strrchr(lpModuleName, '.'), i = 0;
        i < 4 && t && *t; t++, i++)
        *t = ".so"[i];

    HANDLE hModule = dlopen(lpModuleName, RTLD_NOLOAD);
    if (lpModuleName)
    {
        if (hModule)
            dlclose(hModule); // Restore refcount
        else
            DebugLog("FIXME: Module %s not found", lpModuleName);
    }

    return hModule;
}

static HANDLE WINAPI GetModuleHandleW(PVOID lpModuleName)
{
    char *name = CreateAnsiFromWide(lpModuleName);

    DebugLog("%p [%s]", lpModuleName, name);

    HANDLE hModule = GetModuleHandleA(lpModuleName);

    free(name);

    return hModule;
}

DECLARE_CRT_EXPORT("FreeLibrary", FreeLibrary);
DECLARE_CRT_EXPORT("LoadLibraryExW", LoadLibraryExW);
DECLARE_CRT_EXPORT("LoadLibraryW", LoadLibraryW);
DECLARE_CRT_EXPORT("LoadLibraryA", LoadLibraryA);
DECLARE_CRT_EXPORT("GetProcAddress", GetProcAddress);
DECLARE_CRT_EXPORT("GetModuleHandleW", GetModuleHandleW);
DECLARE_CRT_EXPORT("GetModuleHandleA", GetModuleHandleA);
DECLARE_CRT_EXPORT("GetModuleFileNameA", GetModuleFileNameA);
DECLARE_CRT_EXPORT("GetModuleFileNameW", GetModuleFileNameW);
