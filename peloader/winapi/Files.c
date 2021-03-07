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
#include <limits.h>

#include "winnt_types.h"
#include "codealloc.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"
#include "Files.h"
#include "file_mapping.h"

union size {
    int64_t size;
    struct {
        int32_t low;
        int32_t high;
    };
} Size;

union offset {
    int64_t offset;
    struct {
        int32_t low;
        int32_t high;
    };
} Offset;

static MappedFileViewList* MappedFileViews;

typedef struct _CREATEFILE2_EXTENDED_PARAMETERS {
    DWORD dwSize;
    DWORD dwFileAttributes;
    DWORD dwFileFlags;
    DWORD dwSecurityQosFlags;
    PVOID lpSecurityAttributes;
    HANDLE hTemplateFile;
} CREATEFILE2_EXTENDED_PARAMETERS, *PCREATEFILE2_EXTENDED_PARAMETERS;

NTSTATUS WINAPI NtCreateFile(HANDLE *FileHandle,
                             ACCESS_MASK DesiredAccess,
                             POBJECT_ATTRIBUTES ObjectAttributes,
                             PIO_STATUS_BLOCK IoStatusBlock,
                             LARGE_INTEGER *AllocationSize,
                             ULONG FileAttributes,
                             ULONG ShareAccess,
                             ULONG CreateDisposition,
                             ULONG CreateOptions,
                             PVOID EaBuffer,
                             ULONG EaLength)
{
    LPSTR filename = CreateAnsiFromWide(ObjectAttributes->name->Buffer);

    DebugLog("%p, %#x, %p, [%s]", FileHandle, DesiredAccess, ObjectAttributes, filename);

    // Translate path seperator.
    while (strchr(filename, '\\'))
    *strchr(filename, '\\') = '/';

    // I'm just going to tolower() everything.
    for (char *t = filename; *t; t++)
    *t = tolower(*t);

    switch (CreateDisposition) {
        case FILE_SUPERSEDED:
            *FileHandle = fopen(filename, "r");
            break;
        case FILE_OPEN:
            if (access(filename, F_OK) == 0){
                *FileHandle = fopen(filename, "r+");
            }
            else {
                free(filename);
                return STATUS_NO_SUCH_FILE;
            }
            break;
            // This is the disposition used by CreateTempFile().
        case FILE_CREATED:
            *FileHandle = fopen(filename, "w");
            // Unlink it immediately so it's cleaned up on exit.
            unlink(filename);
            break;
        default:
            abort();
    }

    free(filename);

    return 0;
}

STATIC DWORD WINAPI GetFileAttributesW(PVOID lpFileName)
{
    DWORD Result = FILE_ATTRIBUTE_NORMAL;
    char *filename = CreateAnsiFromWide(lpFileName);
    DebugLog("%p [%s]", lpFileName, filename);

    if (strstr(filename, "RebootActions") || strstr(filename, "RtSigs")) {
        Result = INVALID_FILE_ATTRIBUTES;
        goto finish;
    }

finish:
    free(filename);
    return Result;
}

STATIC BOOL WINAPI SetFileAttributesA(LPCSTR lpFileName,
                                      DWORD dwFileAttributes)
{
    DebugLog("%p [%s]", lpFileName, lpFileName);

    SetLastError(0);
    return true;
}

STATIC BOOL WINAPI SetFileAttributesW(LPWSTR lpFileName,
                               DWORD dwFileAttributes)
{
    LPSTR lpFileNameA = CreateAnsiFromWide(lpFileName);
    DebugLog("%p [%s], %#x", lpFileName, lpFileNameA, dwFileAttributes);

    SetLastError(0);

    return true;
}

STATIC DWORD WINAPI GetFileAttributesExW(PWCHAR lpFileName, DWORD fInfoLevelId, LPWIN32_FILE_ATTRIBUTE_DATA lpFileInformation)
{
    char *filename = CreateAnsiFromWide(lpFileName);
    DebugLog("%p [%s], %u, %p", lpFileName, filename, fInfoLevelId, lpFileInformation);

    assert(fInfoLevelId == 0);

    lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    free(filename);
    return TRUE;
}


HANDLE WINAPI CreateFileA(PCHAR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, PVOID lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    FILE *FileHandle;

    DebugLog("%p [%s], %#x, %#x, %p, %#x, %#x, %p", lpFileName, lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    // Translate path seperator.
    while (strchr(lpFileName, '\\'))
        *strchr(lpFileName, '\\') = '/';

    // I'm just going to tolower() everything.
    for (char *t = lpFileName; *t; t++)
        *t = tolower(*t);

    switch (dwCreationDisposition) {
        case OPEN_EXISTING:
            FileHandle = fopen(lpFileName, "r");
            break;
        case CREATE_ALWAYS:
            FileHandle = fopen("/dev/null", "w");
            break;
        // This is the disposition used by CreateTempFile().
        case CREATE_NEW:
            if (strstr(lpFileName, "/faketemp/")) {
                FileHandle = fopen(lpFileName, "w");
                // Unlink it immediately so it's cleaned up on exit.
                unlink(lpFileName);
            } else {
                FileHandle = fopen("/dev/null", "w");
            }
            break;
        default:
            abort();
    }

    DebugLog("%s => %p", lpFileName, FileHandle);

    FileHandle ? SetLastError(0) : SetLastError(ERROR_FILE_NOT_FOUND);
    return FileHandle ? FileHandle : INVALID_HANDLE_VALUE;
}

HANDLE WINAPI CreateFileW(PWCHAR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, PVOID lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    FILE *FileHandle;
    char *filename = CreateAnsiFromWide(lpFileName);

    DebugLog("%p [%s], %#x, %#x, %p, %#x, %#x, %p", lpFileName, filename, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    // Translate path seperator.
    while (strchr(filename, '\\'))
        *strchr(filename, '\\') = '/';

    // I'm just going to tolower() everything.
    for (char *t = filename; *t; t++)
        *t = tolower(*t);

    //LogMessage("%u %s", dwCreationDisposition, filename);

    switch (dwCreationDisposition) {
        case OPEN_EXISTING:
            FileHandle = fopen(filename, "r");
            break;
        case CREATE_ALWAYS:
            FileHandle = fopen("/dev/null", "w");
            break;
        // This is the disposition used by CreateTempFile().
        case CREATE_NEW:
            if (strstr(filename, "/faketemp/")) {
                FileHandle = fopen(filename, "w");
                // Unlink it immediately so it's cleaned up on exit.
                unlink(filename);
            } else {
                FileHandle = fopen("/dev/null", "w");
            }
            break;
        default:
            abort();
    }

    DebugLog("%s => %p", filename, FileHandle);

    free(filename);

    SetLastError(ERROR_FILE_NOT_FOUND);
    return FileHandle ? FileHandle : INVALID_HANDLE_VALUE;
}

HANDLE CreateFile2(PWCHAR lpFileName,
                   DWORD dwDesiredAccess,
                   DWORD dwShareMode,
                   DWORD dwCreationDisposition,
                   PCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams)
{
    return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, pCreateExParams->lpSecurityAttributes, dwCreationDisposition,
        pCreateExParams->dwFileAttributes | pCreateExParams->dwFileFlags, pCreateExParams->hTemplateFile);
}

/**
 * TODO: handle 64 bit 
 */
STATIC DWORD WINAPI SetFilePointer(HANDLE hFile, LONG liDistanceToMove,  LONG *lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    int result;

    DebugLog("%p, %#x, %p, %u", hFile, liDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);

    result = fseek(hFile, liDistanceToMove, dwMoveMethod);

    DWORD pos = ftell(hFile);

    if (lpDistanceToMoveHigh) {
        *lpDistanceToMoveHigh = 0;
    }

    return pos;
}

STATIC BOOL WINAPI SetFilePointerEx(HANDLE hFile, uint64_t liDistanceToMove,  uint64_t *lpNewFilePointer, DWORD dwMoveMethod)
{
    int result;

    DebugLog("%p, %llu, %p, %u", hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);

    result = fseek(hFile, liDistanceToMove, dwMoveMethod);

    // dwMoveMethod maps onto SEEK_SET/SEEK_CUR/SEEK_END perfectly.
    if (lpNewFilePointer) {
        *lpNewFilePointer = ftell(hFile);
    }

    // Windows is permissive here.
    return TRUE;
    //return result != -1; 
}

STATIC BOOL WINAPI CloseHandle(HANDLE hObject)
{
    DebugLog("%p", hObject);
    if (hObject != (HANDLE) 'EVNT'
     && hObject != INVALID_HANDLE_VALUE
     && hObject != (HANDLE) 'SEMA')
        fclose(hObject);
    return TRUE;
}


STATIC BOOL WINAPI ReadFile(HANDLE hFile, PVOID lpBuffer, DWORD nNumberOfBytesToRead, PDWORD lpNumberOfBytesRead, PVOID lpOverlapped)
{
    DebugLog("%p, %p, %#x", hFile, lpBuffer, nNumberOfBytesToRead);
    *lpNumberOfBytesRead = fread(lpBuffer, 1, nNumberOfBytesToRead, hFile);
    return TRUE;
}

STATIC BOOL WINAPI WriteFile(HANDLE hFile, PVOID lpBuffer, DWORD nNumberOfBytesToWrite, PDWORD lpNumberOfBytesWritten, PVOID lpOverlapped)
{
    DebugLog("%p, %p, %#x", hFile, lpBuffer, nNumberOfBytesToWrite);
    *lpNumberOfBytesWritten = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, hFile);
    return TRUE;
}

STATIC BOOL WINAPI DeleteFileW(PWCHAR lpFileName)
{
    char *AnsiFilename = CreateAnsiFromWide(lpFileName);

    DebugLog("%p [%s]", lpFileName, AnsiFilename);

    free(AnsiFilename);
    return TRUE;
}
STATIC BOOL WINAPI DeleteFileA(LPCSTR lpFileName)
{
    DebugLog("%p [%s]", lpFileName, lpFileName);

    return TRUE;
}


STATIC BOOL WINAPI GetFileSizeEx(HANDLE hFile, uint64_t *lpFileSize)
{
    long curpos = ftell(hFile);

    fseek(hFile, 0, SEEK_END);

    *lpFileSize = ftell(hFile);

    fseek(hFile, curpos, SEEK_SET);

    DebugLog("%p, %p => %llu", hFile, lpFileSize, *lpFileSize);


    return TRUE;
}

STATIC DWORD WINAPI GetFileSize(HANDLE hFile, DWORD *lpFileSizeHigh)
{
    long curpos = ftell(hFile);

    fseek(hFile, 0, SEEK_END);

    size_t FileSize = ftell(hFile);

    Size.size = FileSize;

    fseek(hFile, curpos, SEEK_SET);

    DebugLog("%p => %#x", hFile, FileSize);

    if (lpFileSizeHigh != NULL)
        *lpFileSizeHigh = Size.high;

    return FileSize;
}

DWORD WINAPI GetFileAttributesA(LPCSTR lpFileName)
{
    DWORD Result = FILE_ATTRIBUTE_NORMAL;
    DebugLog("%p [%s]", lpFileName, lpFileName);

    if (strstr(lpFileName, "RebootActions") || strstr(lpFileName, "RtSigs")) {
        Result = INVALID_FILE_ATTRIBUTES;
    }

    if(strncmp(lpFileName, ".\\FAKETEMP\\", strlen(".\\FAKETEMP\\")) == 0) {
        Result = FILE_ATTRIBUTE_DIRECTORY;
    }

    return Result;
}

STATIC HANDLE WINAPI CreateFileMappingA(HANDLE hFile,
                                        PVOID lpFileMappingAttributes,
                                        DWORD flProtect,
                                        DWORD dwMaximumSizeHigh,
                                        DWORD dwMaximumSizeLow,
                                        LPCSTR lpName)
{
    DebugLog("%p, %#x, %#x, %#x, [%s]", hFile, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    assert(!lpFileMappingAttributes);
    assert(!lpName);

    Size.high = dwMaximumSizeHigh;
    Size.low = dwMaximumSizeLow;

    // This assumes the file is only going to be accessed using memory-mapping.
    // This will fail in case ReadFile/WriteFile is called on a mapped file.
    fseek(hFile, Size.size, SEEK_SET);

    return hFile;
}

STATIC HANDLE WINAPI CreateFileMappingW(HANDLE hFile,
                                        PVOID lpFileMappingAttributes,
                                        DWORD flProtect,
                                        DWORD dwMaximumSizeHigh,
                                        DWORD dwMaximumSizeLow,
                                        LPCWSTR lpName)
{
    char *name = CreateAnsiFromWide(lpName);

    HANDLE hMap = CreateFileMappingA(hFile, lpFileMappingAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, name);

    free(name);
    return hMap;
}

STATIC PVOID WINAPI MapViewOfFileEx(HANDLE hFileMappingObject,
                                    DWORD dwDesiredAccess,
                                    DWORD dwFileOffsetHigh,
                                    DWORD dwFileOffsetLow,
                                    SIZE_T dwNumberOfBytesToMap,
                                    PVOID lpBaseAddress)

{
    DebugLog("%p, %#x, %#x, %#x, %#x, %p", hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, lpBaseAddress);

    Offset.high = dwFileOffsetHigh;
    Offset.low = dwFileOffsetLow;

    int access = 0;
    assert(!(dwDesiredAccess & 0x1));
    if (dwDesiredAccess & 0x2) // FILE_MAP_WRITE
        access |= PROT_WRITE;
    if (dwDesiredAccess & 0x4) // FILE_MAP_READ
        access |= PROT_READ;
    if (dwDesiredAccess & 0x20) // FILE_MAP_EXECUTE
        access |= PROT_EXEC;
    if (!access)
        access = PROT_NONE;
    if (dwDesiredAccess & 0x20000000)
        access |= MAP_HUGETLB;

    MappedFileView *pFileView = calloc(1, sizeof(MappedFileView));
    if (pFileView == NULL) {
        DebugLog("[ERROR] failed to allocate view of file: %s ", strerror(errno));
        return NULL;
    }

    int fd = fileno(hFileMappingObject);
    SIZE_T size = dwNumberOfBytesToMap ? dwNumberOfBytesToMap : ftell(hFileMappingObject) - Offset.offset;
    pFileView->base = mmap(lpBaseAddress, size, access, MAP_PRIVATE, fd, Offset.offset);
    pFileView->size = size;
    if (pFileView->base == MAP_FAILED) {
        DebugLog("[ERROR] failed to create file view mapping: %s", strerror(errno));
        free(pFileView);
        return NULL;
    }

    AddMappedView(pFileView, &MappedFileViews);

    return pFileView->base;
}

STATIC PVOID WINAPI MapViewOfFile(HANDLE hFileMappingObject,
                                  DWORD dwDesiredAccess,
                                  DWORD dwFileOffsetHigh,
                                  DWORD dwFileOffsetLow,
                                  SIZE_T dwNumberOfBytesToMap)
{
    return MapViewOfFileEx(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, 0);
}

STATIC BOOL WINAPI FlushViewOfFile(PVOID lpBaseAddress,
                                   SIZE_T dwNumberOfBytesToFlush)
{
    DebugLog("%p, %#x", lpBaseAddress,  dwNumberOfBytesToFlush);

    if (msync(lpBaseAddress, dwNumberOfBytesToFlush, MS_ASYNC) < 0) {
        DebugLog("[ERROR] failed to sync %#x bytes in file view mapping %p: %s", dwNumberOfBytesToFlush, lpBaseAddress, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

STATIC BOOL WINAPI UnmapViewOfFile(PVOID lpBaseAddress)
{
    DebugLog("%p", lpBaseAddress);

    MappedFileView *pFileView = SearchMappedViews(lpBaseAddress, MappedFileViews);
    if (!pFileView) {
        DebugLog("[ERROR] no file view mapping found to unmap");
        return FALSE;
    }

    if (munmap(pFileView->base, pFileView->size) < 0) {
        DebugLog("[ERROR] failed to unmap file view mapping %p: %s", lpBaseAddress, strerror(errno));
        return FALSE;
    }

    DeleteMappedView(pFileView, MappedFileViews);
    return TRUE;
}

STATIC DWORD WINAPI NtOpenSymbolicLinkObject(PHANDLE LinkHandle, DWORD DesiredAccess, PVOID ObjectAttributes)
{
    DebugLog("");
    *LinkHandle = (HANDLE) 'SYMB';
    return STATUS_SUCCESS;
}

STATIC NTSTATUS WINAPI NtQuerySymbolicLinkObject(HANDLE LinkHandle, PUNICODE_STRING LinkTarget, PULONG ReturnedLength)
{
    DebugLog("");
    return STATUS_SUCCESS;
}

STATIC NTSTATUS WINAPI NtClose(HANDLE Handle)
{
    DebugLog("");
    return STATUS_SUCCESS;
}

STATIC BOOL WINAPI DeviceIoControl(
  HANDLE       hDevice,
  DWORD        dwIoControlCode,
  PVOID       lpInBuffer,
  DWORD        nInBufferSize,
  PVOID       lpOutBuffer,
  DWORD        nOutBufferSize,
  PDWORD      lpBytesReturned,
  PVOID       lpOverlapped)
{
    DebugLog("");
    return FALSE;
}

STATIC NTSTATUS WINAPI NtQueryVolumeInformationFile(HANDLE FileHandle,
                                             PVOID IoStatusBlock,
                                             PVOID FsInformation,
                                             ULONG Length,
                                             DWORD FsInformationClass)
{
    DebugLog("%p, %p, %#x", FileHandle, FsInformation, FsInformationClass);
    if (FsInformationClass == FileFsDeviceInformation){
        ((PFILE_FS_DEVICE_INFORMATION)FsInformation)->DeviceType = FILE_DEVICE_DISK;
        ((PFILE_FS_DEVICE_INFORMATION)FsInformation)->Characteristics = 0x0;
    }
    return 0;
}

STATIC DWORD WINAPI GetFullPathNameW(PWCHAR lpFileName,
                                     DWORD nBufferLength,
                                     PWCHAR lpBuffer,
                                     PWCHAR *lpFilePart)
{
    DebugLog("");
    return 0;
}

STATIC BOOL WINAPI SetEndOfFile(HANDLE hFile)
{
    DebugLog("");
    return ftruncate(fileno(hFile), ftell(hFile)) != -1;
}

STATIC DWORD WINAPI GetFileVersionInfoSizeExW(DWORD dwFlags, PWCHAR lptstrFilename, PDWORD lpdwHandle)
{
    DebugLog("%#x, %p, %p", dwFlags, lptstrFilename, lpdwHandle);
    return 0;
}

STATIC BOOL WINAPI GetFileVersionInfoExW(DWORD dwFlags, PWCHAR lptstrFilename, DWORD dwHandle, DWORD dwLen, PVOID lpData)
{
    DebugLog("");
    return FALSE;
}

STATIC BOOL WINAPI VerQueryValueW(PVOID pBlock, PWCHAR lpSubBlock, PVOID  *lplpBuffer, PDWORD puLen)
{
    DebugLog("");
    return FALSE;
}

STATIC DWORD WINAPI QueryDosDevice(PVOID lpDeviceName, PVOID lpTargetPath, DWORD ucchMax)
{
    DebugLog("");
    return 0;
}

STATIC BOOL WINAPI GetDiskFreeSpaceExW(PWCHAR lpDirectoryName, PVOID lpFreeBytesAvailableToCaller, PVOID lpTotalNumberOfBytes, QWORD *lpTotalNumberOfFreeBytes)
{
    DebugLog("%S", lpDirectoryName);
    *lpTotalNumberOfFreeBytes = 0x000000000ULL;
    return FALSE;
}

STATIC NTSTATUS WINAPI NtQueryInformationFile(HANDLE FileHandle,
                                       PVOID IoStatusBlock,
                                       PVOID FileInformation,
                                       ULONG Length,
                                       DWORD FileInformationClass)
{
    DebugLog("%p, %#x, %#x", FileHandle, Length, FileInformationClass);
    if (FileInformationClass == FileStandardInformation) {
        fseek((FILE*)FileHandle, 0L, SEEK_END);
        size_t FileSize = ftell((FILE*)FileHandle);
        rewind((FILE*)FileHandle);
        ((PFILE_STANDARD_INFORMATION) FileInformation)->AllocationSize = FileSize;
        ((PFILE_STANDARD_INFORMATION) FileInformation)->EndOfFile = FileSize;
        ((PFILE_STANDARD_INFORMATION) FileInformation)->NumberOfLinks = 0;
        ((PFILE_STANDARD_INFORMATION) FileInformation)->DeletePending = FALSE;
        ((PFILE_STANDARD_INFORMATION) FileInformation)->Directory = FALSE; //TODO: Check if FileHandle is a directory
    }
    return 0;
}

STATIC BOOL WINAPI SetFileTime(HANDLE hFile,
                               const FILETIME *lpCreationTime,
                               const FILETIME *lpLastAccessTime,
                               const FILETIME *lpLastWriteTime)
{
    DebugLog("%p, %p, %p, %p", hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);

    return true;
}

STATIC BOOL WINAPI GetFileTime(HANDLE hFile,
                               PFILETIME lpCreationTime,
                               PFILETIME lpLastAccessTime,
                               PFILETIME lpLastWriteTime) // TO FIX
{
    DebugLog("%p, %p, %p, %p", hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
    lpCreationTime->dwHighDateTime = 0;
    lpCreationTime->dwLowDateTime = 0;
    lpLastAccessTime->dwHighDateTime = 0;
    lpLastAccessTime->dwLowDateTime = 0;
    lpLastWriteTime->dwLowDateTime = 0;
    lpLastWriteTime->dwLowDateTime = 0;

    return true;
}

STATIC DWORD WINAPI GetFileType(HANDLE hFile)
{
    DebugLog("%p", hFile);

    return FILE_TYPE_DISK;
}

STATIC DWORD WINAPI GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR *lpFilePart)
{
    if (nBufferLength < PATH_MAX)
        return PATH_MAX;

    char* result = realpath(lpFileName, lpBuffer);
    if (!result)
        return 0;
    if (lpFilePart)
        *lpFilePart = basename(result);
    return strlen(result);
}

DECLARE_CRT_EXPORT("VerQueryValueW", VerQueryValueW);
DECLARE_CRT_EXPORT("GetFileVersionInfoExW", GetFileVersionInfoExW);
DECLARE_CRT_EXPORT("GetFileVersionInfoSizeExW", GetFileVersionInfoSizeExW);
DECLARE_CRT_EXPORT("GetFileAttributesW", GetFileAttributesW);
DECLARE_CRT_EXPORT("SetFileAttributesA", SetFileAttributesA);
DECLARE_CRT_EXPORT("SetFileAttributesW", SetFileAttributesW);
DECLARE_CRT_EXPORT("GetFileAttributesExW", GetFileAttributesExW);
DECLARE_CRT_EXPORT("CreateFileA", CreateFileA);
DECLARE_CRT_EXPORT("CreateFileW", CreateFileW);
DECLARE_CRT_EXPORT("SetFilePointer", SetFilePointer);
DECLARE_CRT_EXPORT("SetFilePointerEx", SetFilePointerEx);
DECLARE_CRT_EXPORT("CloseHandle", CloseHandle);
DECLARE_CRT_EXPORT("ReadFile", ReadFile);
DECLARE_CRT_EXPORT("WriteFile", WriteFile);
DECLARE_CRT_EXPORT("DeleteFileW", DeleteFileW);
DECLARE_CRT_EXPORT("GetFileSizeEx", GetFileSizeEx);
DECLARE_CRT_EXPORT("GetFileSize", GetFileSize);
DECLARE_CRT_EXPORT("NtOpenSymbolicLinkObject", NtOpenSymbolicLinkObject);
DECLARE_CRT_EXPORT("NtQuerySymbolicLinkObject", NtQuerySymbolicLinkObject);
DECLARE_CRT_EXPORT("NtClose", NtClose);
DECLARE_CRT_EXPORT("DeviceIoControl", DeviceIoControl);
DECLARE_CRT_EXPORT("NtQueryVolumeInformationFile", NtQueryVolumeInformationFile);
DECLARE_CRT_EXPORT("GetFullPathNameW", GetFullPathNameW);
DECLARE_CRT_EXPORT("SetEndOfFile", SetEndOfFile);
DECLARE_CRT_EXPORT("QueryDosDeviceW", QueryDosDevice);
DECLARE_CRT_EXPORT("GetDiskFreeSpaceExW", GetDiskFreeSpaceExW);
DECLARE_CRT_EXPORT("NtQueryInformationFile", NtQueryInformationFile);
DECLARE_CRT_EXPORT("SetFileTime", SetFileTime);
DECLARE_CRT_EXPORT("GetFileTime", GetFileTime);
DECLARE_CRT_EXPORT("GetFileType", GetFileType);
DECLARE_CRT_EXPORT("CreateFileMappingA", CreateFileMappingA);
DECLARE_CRT_EXPORT("CreateFileMappingW", CreateFileMappingW);
DECLARE_CRT_EXPORT("GetFileAttributesA", GetFileAttributesA);
DECLARE_CRT_EXPORT("MapViewOfFile", MapViewOfFile);
DECLARE_CRT_EXPORT("MapViewOfFileEx", MapViewOfFileEx);
DECLARE_CRT_EXPORT("FlushViewOfFile", FlushViewOfFile);
DECLARE_CRT_EXPORT("UnmapViewOfFile", UnmapViewOfFile);
DECLARE_CRT_EXPORT("DeleteFileA", DeleteFileA);
DECLARE_CRT_EXPORT("NtCreateFile", NtCreateFile);
DECLARE_CRT_EXPORT("CreateFile2", CreateFile2);
DECLARE_CRT_EXPORT("GetFullPathNameA", GetFullPathNameA);
