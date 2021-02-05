#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <stdlib.h>
#include <stdio.h>

#include "winnt_types.h"
#include "codealloc.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"
#include "Memory.h"

void WINAPI RtlAcquirePebLock(void)
{
    DebugLog("");
    return;
}

void WINAPI RtlReleasePebLock(void)
{
    DebugLog("");
    return;
}

NTSTATUS WINAPI LdrGetDllHandle(PWCHAR pwPath, PVOID unused, PUNICODE_STRING ModuleFileName, PHANDLE pHModule)
{
    DebugLog("%S %p %p %p", pwPath, unused, ModuleFileName, pHModule);
    pHModule = (HANDLE) 'LDRP';
    return 0;
}

NTSTATUS WINAPI EtwRegister(PVOID ProvideId, PVOID EnableCallback, PVOID CallbackContext, PVOID RegHandle)
{
    DebugLog("");
    return 0;
}

NTSTATUS WINAPI EtwUnregister(HANDLE RegHandle)
{
    DebugLog("");
    return 0;
}

ULONG WINAPI EtwEventWrite(HANDLE RegHAndle, PVOID EventDescriptor, ULONG UserDataCount, PVOID UserData, PVOID a5)
{
    DebugLog("");
    return 0;
}

static NTSTATUS WINAPI LdrLoadDll(PWCHAR PathToFile,
                                  ULONG Flags,
                                  PUNICODE_STRING ModuleFilename,
                                  PHANDLE ModuleHandle)
{
    char *PathToFileA = CreateAnsiFromWide(PathToFile);
    char *ModuleFilenameA = CreateAnsiFromWide(ModuleFilename->Buffer);

    DebugLog("%p [%s], %p [%s], %p, %#x", PathToFile, PathToFileA, ModuleFilename, ModuleFilenameA, ModuleHandle, Flags);

    *ModuleHandle = (HANDLE) 'LOAD';

    free(PathToFileA);
    free(ModuleFilenameA);

    return 0;
}

static NTSTATUS WINAPI LdrUnloadDll(HANDLE ModuleHandle) {
    DebugLog("%p", ModuleHandle);

    return 0;
}

static NTSTATUS WINAPI LdrGetProcedureAddress(HMODULE Module,
                                              PANSI_STRING Name,
                                              WORD Ordinal,
                                              PVOID *Address)
{
    DebugLog("%p %s %hu %p", Module, Name->buf, Ordinal, Address);

    // Recognizable value to crash on.
    *Address = (PVOID) 'LDRZ';

    // Search if the requested function has been already exported.
    ENTRY e = { Name->buf, NULL }, *ep;
    hsearch_r(e, FIND, &ep, &crtexports);

    // If found, store the pointer and return.
    if (ep != NULL) {
        *Address = ep->data;
        return 0;
    }

    if (strcmp(Name->buf, "EtwEventRegister") == 0) {
        *Address = EtwRegister;
    }
    if (strcmp(Name->buf, "EtwEventUnregister") == 0) {
        *Address = EtwUnregister;
    }
    if (strcmp(Name->buf, "EtwEventWrite") == 0) {
        *Address = EtwEventWrite;
    }

    DebugLog("FIXME: %s unresolved", Name->buf);

    return 0;
}

static NTSTATUS WINAPI NtReadFile(HANDLE FileHandle,
                                  HANDLE Event,
                                  PVOID ApcRoutine,
                                  PVOID ApcContext,
                                  PIO_STATUS_BLOCK IoStatusBlock,
                                  PVOID Buffer,
                                  ULONG Length,
                                  LARGE_INTEGER *ByteOffset,
                                  PULONG Key)
{
    DebugLog("%p, %p, %p, %#x", FileHandle, IoStatusBlock, Buffer, Length);
    ((PIO_STATUS_BLOCK) IoStatusBlock)->Information = fread(Buffer, 1, Length, FileHandle);
    ((PIO_STATUS_BLOCK) IoStatusBlock)->DUMMYUNIONNAME.Status = STATUS_SUCCESS;
    return 0;
}

STATIC BOOL WINAPI NtFreeVirtualMemory(HANDLE ProcessHandle,
                                       PVOID *BaseAddress,
                                       SIZE_T *RegionSize,
                                       ULONG FreeType)
{
    DebugLog("%p, %p, %#x", ProcessHandle, *BaseAddress, RegionSize);

    if (FreeType == MEM_RELEASE)
        code_free(*BaseAddress);
    return TRUE;
}

static NTSTATUS WINAPI NtProtectVirtualMemory(HANDLE ProcessHandle,
                                              PVOID *BaseAddress,
                                              ULONG *NumberOfBytesToProtect,
                                              ULONG NewAccessProtection,
                                              ULONG *OldAccessProtection)
{
    DebugLog("%p, %p, %#x, %#x", ProcessHandle, *BaseAddress, *NumberOfBytesToProtect, NewAccessProtection);
    // We always allocate RXW.
    return 0;
}

static NTSTATUS WINAPI NtAllocateVirtualMemory(HANDLE ProcessHandle,
                                               PVOID *BaseAddress,
                                               ULONG_PTR ZeroBits,
                                               SIZE_T* RegionSize,
                                               ULONG AllocationType,
                                               ULONG Protect)
{
    DebugLog("%p, %p, %#x, %#x, %#x", ProcessHandle, BaseAddress, *RegionSize, AllocationType, Protect);

    if (AllocationType & ~(MEM_COMMIT | MEM_RESERVE)) {
        DebugLog("AllocationType %#x not implemnted", AllocationType);
        return STATUS_NOT_IMPLEMENTED;
    }

    // This VirtualAlloc() always returns PAGE_EXECUTE_READWRITE memory.
    if (Protect & PAGE_READWRITE){
        *BaseAddress = code_malloc(*RegionSize);
        DebugLog("%#x bytes of memory allocated at address %p", *RegionSize, *BaseAddress);
    }
    else if (Protect & PAGE_EXECUTE_READWRITE) {
        DebugLog("JIT PAGE_EXECUTE_READWRITE Allocation Requested");
        *BaseAddress = code_malloc(*RegionSize);
        DebugLog("%#x bytes of memory allocated at address %p", *RegionSize, *BaseAddress);
    }
    else {
        DebugLog("flProtect flags %#x not implemented", Protect);
        return STATUS_NOT_IMPLEMENTED;
    }

    return 0;
}

STATIC NTSTATUS WINAPI LdrDisableThreadCalloutsForDll(HMODULE hDll)
{
    DebugLog("%p", hDll);

    return 0;
}

DECLARE_CRT_EXPORT("RtlAcquirePebLock", RtlAcquirePebLock);
DECLARE_CRT_EXPORT("RtlReleasePebLock", RtlReleasePebLock);
DECLARE_CRT_EXPORT("LdrGetDllHandle", LdrGetDllHandle);
DECLARE_CRT_EXPORT("LdrLoadDll", LdrLoadDll);
DECLARE_CRT_EXPORT("LdrUnloadDll", LdrUnloadDll);
DECLARE_CRT_EXPORT("LdrGetProcedureAddress", LdrGetProcedureAddress);
DECLARE_CRT_EXPORT("NtReadFile", NtReadFile);
DECLARE_CRT_EXPORT("NtFreeVirtualMemory", NtFreeVirtualMemory);
DECLARE_CRT_EXPORT("NtProtectVirtualMemory", NtProtectVirtualMemory);
DECLARE_CRT_EXPORT("NtAllocateVirtualMemory", NtAllocateVirtualMemory);
DECLARE_CRT_EXPORT("LdrDisableThreadCalloutsForDll", LdrDisableThreadCalloutsForDll);
