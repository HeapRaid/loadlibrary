#ifndef LOADLIBRARY_FILES_H
#define LOADLIBRARY_FILES_H


extern void WINAPI SetLastError(DWORD dwErrCode);
HANDLE WINAPI CreateFileA(PCHAR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, PVOID lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE WINAPI CreateFileW(PWCHAR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, PVOID lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
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
                             ULONG EaLength);
DWORD WINAPI GetFileAttributesA(LPCSTR lpFileName);

enum {
    CREATE_NEW          = 1,
    CREATE_ALWAYS       = 2,
    OPEN_EXISTING       = 3,
    OPEN_ALWAYS         = 4,
    TRUNCATE_EXISTING   = 5
};

#define FILE_TYPE_CHAR 0x0002
#define FILE_TYPE_DISK 0x0001
#define FILE_TYPE_PIPE 0x0003
#define FILE_TYPE_REMOTE 0x8000
#define FILE_TYPE_UNKNOWN 0x0000

#define ERROR_FILE_NOT_FOUND 2

#define FILE_ATTRIBUTE_NORMAL 128
#define FILE_ATTRIBUTE_DIRECTORY 16

#define INVALID_FILE_ATTRIBUTES -1;

#endif //LOADLIBRARY_FILES_H
