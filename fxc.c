//
// Copyright (C) 2017 Tavis Ormandy
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <poll.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"

// Any usage limits to prevent bugs disrupting system.
const struct rlimit kUsageLimits[] = {
    [RLIMIT_FSIZE]  = { .rlim_cur = 0x20000000, .rlim_max = 0x20000000 },
    [RLIMIT_CPU]    = { .rlim_cur = 3600,       .rlim_max = RLIM_INFINITY },
    [RLIMIT_CORE]   = { .rlim_cur = 0,          .rlim_max = 0 },
    [RLIMIT_NOFILE] = { .rlim_cur = 32,         .rlim_max = 32 },
};

// The official utility has a maximum number of macros allowed
#define FXC_MAX_MACROS 256

// The official utility has a maximum number of include paths allowed
#define FXC_MAX_INCLUDES 100

// Maximum line width for word wrapping the command line
#define FXC_COLUMN_WIDTH 80

typedef struct _D3D_SHADER_MACRO
{
    LPCSTR Name;
    LPCSTR Definition;
} D3D_SHADER_MACRO;

typedef enum _D3D_INCLUDE_TYPE
{
    D3D_INCLUDE_LOCAL       = 0,
    D3D_INCLUDE_SYSTEM      = ( D3D_INCLUDE_LOCAL + 1 ) ,
    D3D10_INCLUDE_LOCAL     = D3D_INCLUDE_LOCAL,
    D3D10_INCLUDE_SYSTEM    = D3D_INCLUDE_SYSTEM,
    D3D_INCLUDE_FORCE_DWORD = 0x7fffffff
} D3D_INCLUDE_TYPE;

typedef GUID IID;
typedef const IID* REFIID;

typedef struct ID3D10Blob ID3D10Blob;
typedef struct ID3D10BlobVtbl
{
    HRESULT (WINAPI *QueryInterface )(
        ID3D10Blob * This,
        /* [in] */ REFIID riid,
        /* [annotation][iid_is][out] */
        void **ppvObject);
    ULONG (WINAPI *AddRef )(
        ID3D10Blob * This);
    ULONG (WINAPI *Release )(
        ID3D10Blob * This);
    void* (WINAPI *GetBufferPointer )(
        ID3D10Blob * This);
    SIZE_T (WINAPI *GetBufferSize )(
        ID3D10Blob * This);
} ID3D10BlobVtbl;

struct ID3D10Blob
{
    const struct ID3D10BlobVtbl *lpVtbl;
};

typedef struct ID3D10Include ID3D10Include;
struct ID3D10IncludeVtbl
{
    HRESULT (WINAPI *Open)(ID3D10Include * This, D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, PVOID pParentData, PVOID *ppData, UINT *pBytes);
    HRESULT (WINAPI *Close)(ID3D10Include * This, PVOID pData);
};

struct ID3D10Include
{
    const struct ID3D10IncludeVtbl *lpVtbl;
    int includeDirs[FXC_MAX_INCLUDES + 1];
};

typedef struct ID3D10Blob ID3DBlob;
typedef struct ID3D10Include ID3DInclude;

#define ID3D10Blob_QueryInterface(This,riid,ppvObject)  \
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) )

#define ID3D10Blob_AddRef(This) \
    ( (This)->lpVtbl -> AddRef(This) )

#define ID3D10Blob_Release(This)    \
    ( (This)->lpVtbl -> Release(This) )

#define ID3D10Blob_GetBufferPointer(This)   \
    ( (This)->lpVtbl -> GetBufferPointer(This) )

#define ID3D10Blob_GetBufferSize(This)  \
    ( (This)->lpVtbl -> GetBufferSize(This) )

#define D3DCOMPILE_DEBUG                                (1 << 0)
#define D3DCOMPILE_SKIP_VALIDATION                      (1 << 1)
#define D3DCOMPILE_SKIP_OPTIMIZATION                    (1 << 2)
#define D3DCOMPILE_PACK_MATRIX_ROW_MAJOR                (1 << 3)
#define D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR             (1 << 4)
#define D3DCOMPILE_PARTIAL_PRECISION                    (1 << 5)
#define D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT             (1 << 6)
#define D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT             (1 << 7)
#define D3DCOMPILE_NO_PRESHADER                         (1 << 8)
#define D3DCOMPILE_AVOID_FLOW_CONTROL                   (1 << 9)
#define D3DCOMPILE_PREFER_FLOW_CONTROL                  (1 << 10)
#define D3DCOMPILE_ENABLE_STRICTNESS                    (1 << 11)
#define D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY       (1 << 12)
#define D3DCOMPILE_IEEE_STRICTNESS                      (1 << 13)
#define D3DCOMPILE_OPTIMIZATION_LEVEL0                  (1 << 14)
#define D3DCOMPILE_OPTIMIZATION_LEVEL1                  0
#define D3DCOMPILE_OPTIMIZATION_LEVEL2                  ((1 << 14) | (1 << 15))
#define D3DCOMPILE_OPTIMIZATION_LEVEL3                  (1 << 15)
#define D3DCOMPILE_RESERVED16                           (1 << 16)
#define D3DCOMPILE_RESERVED17                           (1 << 17)
#define D3DCOMPILE_WARNINGS_ARE_ERRORS                  (1 << 18)

#define D3DCOMPILE_EFFECT_CHILD_EFFECT                  (1 << 0)
#define D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS                (1 << 1)

#define D3D_DISASM_ENABLE_COLOR_CODE                    (1 << 0)
#define D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS          (1 << 1)
#define D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING         (1 << 2)
#define D3D_DISASM_ENABLE_INSTRUCTION_CYCLE             (1 << 3)
#define D3D_DISASM_DISABLE_DEBUG_INFO                   (1 << 4)
#define D3D_DISASM_ENABLE_INSTRUCTION_OFFSET            (1 << 5)
#define D3D_DISASM_INSTRUCTION_ONLY                     (1 << 6)
#define D3D_DISASM_PRINT_HEX_LITERALS                   (1 << 7)

HRESULT (WINAPI* D3DCompile)(
    PVOID                  pSrcData,
    SIZE_T                 SrcDataSize,
    LPCSTR                 pSourceName,
    const D3D_SHADER_MACRO *pDefines,
    ID3DInclude            *pInclude,
    LPCSTR                 pEntrypoint,
    LPCSTR                 pTarget,
    UINT                   Flags1,
    UINT                   Flags2,
    ID3DBlob               **ppCode,
    ID3DBlob               **ppErrorMsgs
);

HRESULT (WINAPI* D3DPreprocess)(
    PVOID                  pSrcData,
    SIZE_T                 SrcDataSize,
    LPCSTR                 pSourceName,
    const D3D_SHADER_MACRO *pDefines,
    ID3DInclude            *pInclude,
    ID3DBlob               **ppCodeText,
    ID3DBlob               **ppErrorMsgs
);

HRESULT (WINAPI* D3DDisassemble)(
    PVOID    pSrcData,
    SIZE_T   SrcDataSize,
    UINT     Flags,
    LPCSTR   szComments,
    ID3DBlob **ppDisassembly
);

void print_usage()
{
    printf("Usage: fxc <options> <files>\n");
    printf("\n");
    printf("   -?, -help           print this message\n");
    printf("\n");
    printf("   -T <profile>        target profile\n");
    printf("   -E <name>           entrypoint name\n");
    printf("   -I <include>        additional include path. use - to read from stdin\n");
//  printf("   -Vi                 display details about the include process\n");
    printf("\n");
    printf("   -Od                 disable optimizations\n");
    printf("   -Op                 disable preshaders\n");
    printf("   -O{0,1,2,3}         optimization level 0..3.  1 is default\n");
    printf("   -WX                 treat warnings as errors\n");
    printf("   -Vd                 disable validation\n");
    printf("   -Zi                 enable debugging information\n");
    printf("   -Zpr                pack matrices in row-major order\n");
    printf("   -Zpc                pack matrices in column-major order\n");
    printf("\n");
    printf("   -Gpp                force partial precision\n");
    printf("   -Gfa                avoid flow control constructs\n");
    printf("   -Gfp                prefer flow control constructs\n");
    printf("   -Gdp                disable effect performance mode\n");
    printf("   -Ges                enable strict mode\n");
    printf("   -Gec                enable backwards compatibility mode\n");
    printf("   -Gis                force IEEE strictness\n");
    printf("   -Gch                compile as a child effect for FX 4.x targets\n");
    printf("\n");
    printf("   -Fo <file>          output object file\n");
    printf("   -Fc <file>          output assembly code listing file\n");
//  printf("   -Fx <file>          output assembly code and hex listing file\n");
    printf("   -Fh <file>          output header file containing object code\n");
//  printf("   -Fe <file>          output warnings and errors to a specific file\n");
//  printf("   -Vn <name>          use <name> as variable name in header file\n");
    printf("   -Cc                 output color coded assembly listings\n");
    printf("   -Ni                 output instruction numbers in assembly listings\n");
    printf("\n");
    printf("   -P <file>           preprocess to file (must be used alone)\n");
    printf("\n");
//  printf("   @<file>             options response file\n");
//  printf("   -dumpbin            load a binary file rather than compiling\n");
//  printf("   -Qstrip_reflect     strip reflection data from 4_0+ shader bytecode\n");
//  printf("   -Qstrip_debug       strip debug information from 4_0+ shader bytecode\n");
//  printf("\n");
//  printf("   -compress           compress DX10 shader bytecode from files\n");
//  printf("   -decompress         decompress bytecode from first file, output files should\n");
//  printf("                       be listed in the order they were in during compression\n");
//  printf("\n");
    printf("   -D <id>=<text>      define macro\n");
    printf("   -LD <version>       Load specified D3DCompiler version\n");
//  printf("   -nologo             suppress copyright message\n");
    printf("\n");
    printf("   <profile>: cs_4_0 cs_4_1 cs_5_0 ds_5_0 fx_2_0 fx_4_0 fx_4_1 fx_5_0 gs_4_0\n");
    printf("      gs_4_1 gs_5_0 hs_5_0 ps_2_0 ps_2_a ps_2_b ps_2_sw ps_3_0 ps_3_sw ps_4_0\n");
    printf("      ps_4_0_level_9_1 ps_4_0_level_9_3 ps_4_0_level_9_0 ps_4_1 ps_5_0 tx_1_0\n");
    printf("      vs_1_1 vs_2_0 vs_2_a vs_2_sw vs_3_0 vs_3_sw vs_4_0 vs_4_0_level_9_1\n");
    printf("      vs_4_0_level_9_3 vs_4_0_level_9_0 vs_4_1 vs_5_0\n");
    printf("\n");
    exit(EXIT_SUCCESS);
}

void print_error(const char* format, ...)
{
    fprintf(stderr, "\033[0;31m");
    va_list ap;
    va_start(ap, format);
        vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "%s\033[0m\n", ", use -? to get usage information");
    exit(EXIT_FAILURE);
}

void print_error_msg(const char* format, ...)
{
    fprintf(stderr, "\033[0;31m");
    va_list ap;
    va_start(ap, format);
        vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\033[0m\n");
    exit(EXIT_FAILURE);
}

PCHAR format_cmd_line(int argc, char **argv)
{
    static LPCSTR start = "//\n";
    static LPCSTR prefix = "//   fxc";
    static LPCSTR lineWrap = "//    ";
    PCHAR cmdLine, argp;
    SIZE_T cmdLineSize = strlen(prefix) + 1; // prefix + newline
    struct {
        bool wrap;
        LPCSTR arg;
    } fragments[argc];
    fragments[0].wrap = false;
    fragments[0].arg = prefix;

    // Calculate total size needed for formatted command line
    for (int i = 1, c = 0; i < argc; i++) {
        cmdLineSize += strlen(argv[i]) + 1; // argument + separator
        fragments[i].arg = argv[i];
        fragments[i].wrap = cmdLineSize - c > FXC_COLUMN_WIDTH;
        if (fragments[i].wrap) {
            c = cmdLineSize;
            cmdLineSize += strlen(lineWrap);
        }
    }
    cmdLineSize += strlen(start) + 1; // start + terminator

    // Allocate the line and format it including line breaks
    cmdLine = malloc(cmdLineSize);
    argp = stpcpy(cmdLine, start);
    for (int i = 0; i < argc; i++) {
        if (fragments[i].wrap) {
            *argp++ = '\n';
            argp = stpcpy(argp, lineWrap);
        } else if (i > 0) {
            *argp++ = ' ';
        }
        argp = stpcpy(argp, fragments[i].arg);
    }
    *argp++ = '\n';
    *argp++ = '\0';

    // Check we've correctly calculated the size
    assert(argp - cmdLine == cmdLineSize);
    return cmdLine;
}

PVOID read_stream(INT fd, SIZE_T* pSize)
{
    const SIZE_T blockSize = getpagesize() * 16;
    SIZE_T size = 0, allocSize = blockSize;
    PBYTE data = malloc(blockSize);
    
    // Wait until the stream is ready
    struct pollfd ready = { fd, POLLIN, 0 };
    poll(&ready, 1, -1);

    // Read data until we will block
    do {
        SIZE_T bytes = 0;

        // If the entire buffer was filled we resize it so we can read the rest
        if (size >= allocSize) {
            allocSize += blockSize;
            data = realloc(data, allocSize);
        }

        bytes = read(fd, data + size, allocSize - size);
        if (bytes <= 0)
            break;
        size += bytes;
    } while (poll(&ready, 1, 0) > 0 && ready.revents & POLLIN);
    
    if (pSize)
        *pSize = size;
    return data;
}

PVOID read_file(INT dir, LPCSTR pFileName, SIZE_T* pSize)
{
    int file = openat(dir, pFileName, O_RDONLY);
    PBYTE data = NULL;
    
    if (file == -1)
        return NULL;
    
    data = read_stream(file, pSize);
    close(file);
    return data;
}

INT create_file(PCHAR pFileName)
{
    if (!pFileName)
        return -1;

    if (pFileName[0] == '-' && pFileName[1] == '\0')
        return STDOUT_FILENO;

    return open(pFileName, O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
}

HRESULT WINAPI include_open(ID3D10Include* This, D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, PVOID pParentData, PVOID *ppData, UINT *pBytes)
{
    DebugLog("Type: %d, File: %s, Parent: %p\n", IncludeType, pFileName, pParentData);

    if (!ppData)
        return STATUS_INVALID_PARAMETER;

    // TODO: Change working directory to the directory of the parent include file
    for (int i = 0; i < FXC_MAX_INCLUDES && This->includeDirs[i] != -1; i++) {
        SIZE_T size;
        *ppData = read_file(This->includeDirs[i], pFileName, &size);
        if (*ppData) {
            if (pBytes)
                *pBytes = (UINT)size;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_FAILURE;
}

HRESULT WINAPI include_close(ID3D10Include* This, PVOID pData)
{
    free(pData);
    return STATUS_SUCCESS;
}

HRESULT WINAPI pipe_include_open(ID3D10Include* This, D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, PVOID pParentData, PVOID *ppData, UINT *pBytes)
{
    DebugLog("Type: %d, File: %s, Parent: %p\n", IncludeType, pFileName, pParentData);

    if (!ppData)
        return STATUS_INVALID_PARAMETER;

    printf(IncludeType == D3D_INCLUDE_LOCAL ? "#include \"%s\"\n" : "#include <%s>\n", pFileName);

    SIZE_T size;
    *ppData = read_stream(STDIN_FILENO, &size);
    if (*ppData) {
        if (pBytes)
            *pBytes = (UINT)size;
        return STATUS_SUCCESS;
    }

    return STATUS_FAILURE;
}

struct ID3D10IncludeVtbl include_vtbl = { include_open, include_close };
struct ID3D10IncludeVtbl pipe_include_vtbl = { pipe_include_open, include_close };

int main(int argc, char **argv)
{
    int c = 0, optionIndex = 0, defineIndex = 0, includeIndex = 1;
    int flagsBit1 = 0, flagsBit2 = 0, flagsBitAsm = 0;
    int objectFile = -1, headerFile = -1, processFile = -1, assemblyFile = -1;
    bool optLevelSet = false, outputFileSet = false;
    
    HRESULT hr = 1;
    PCHAR cmdLine = NULL;
    UINT flags1 = 0, flags2 = 0, flagsAsm = 0;
    LPCSTR target = NULL, entryPoint = NULL;
    ID3DBlob *pCode = NULL, *pError = NULL;
    D3D_SHADER_MACRO defines[FXC_MAX_MACROS + 1] = { { NULL, NULL } };
    ID3DInclude includer = { &include_vtbl, { AT_FDCWD, -1 } };

    struct pe_image image = {
        .entry  = NULL,
        .name   = "engine/D3DCompiler_43.dll",
    };
    struct option longOptions[] = {
        {"help", no_argument, NULL, '?'},

        {"Od", no_argument, &flagsBit1, D3DCOMPILE_SKIP_OPTIMIZATION},
        {"Op", no_argument, &flagsBit1, D3DCOMPILE_NO_PRESHADER},
        {"WX", no_argument, &flagsBit1, D3DCOMPILE_WARNINGS_ARE_ERRORS},
        {"Vd", no_argument, &flagsBit1, D3DCOMPILE_SKIP_VALIDATION},

        {"Zi", no_argument, &flagsBit1, D3DCOMPILE_DEBUG},
        {"Zpr", no_argument, &flagsBit1, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR},
        {"Zpc", no_argument, &flagsBit1, D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR},

        {"Gpp", no_argument, &flagsBit1, D3DCOMPILE_PARTIAL_PRECISION},
        {"Gfa", no_argument, &flagsBit1, D3DCOMPILE_AVOID_FLOW_CONTROL},
        {"Gfp", no_argument, &flagsBit1, D3DCOMPILE_PREFER_FLOW_CONTROL},
        {"Gdp", no_argument, &flagsBit2, D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS},
        {"Ges", no_argument, &flagsBit1, D3DCOMPILE_ENABLE_STRICTNESS},
        {"Gec", no_argument, &flagsBit1, D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY},
        {"Gis", no_argument, &flagsBit1, D3DCOMPILE_IEEE_STRICTNESS},
        {"Gch", no_argument, &flagsBit2, D3DCOMPILE_EFFECT_CHILD_EFFECT},

        {"Cc", no_argument, &flagsBitAsm, D3D_DISASM_ENABLE_COLOR_CODE},
        {"Ni", no_argument, &flagsBitAsm, D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING},
        {0, 0, 0, 0}
    };

    // Cache the full command-line before we parse it
    cmdLine = format_cmd_line(argc, argv);

    while ((c = getopt_long_only(argc, argv, "T:E:I:O:F:L:P:Q:D:?", longOptions, &optionIndex)) != -1) {
        switch (c) {
            case 'T':
                target = optarg;
            break;
            case 'E':
                entryPoint = optarg;
            break;
            case 'I':
                if (optarg[0] == '-' && optarg[1] == '\0')
                    includer.lpVtbl = &pipe_include_vtbl;
                else if (includeIndex > FXC_MAX_INCLUDES || (includer.includeDirs[includeIndex++] = open(optarg, O_DIRECTORY | O_PATH)) == -1)
                    print_error_msg("unable to add include path to search list: %s\n", optarg);
            break;
            case 'O':
                if (optLevelSet)
                    print_error("Optimization level (-O#) set multiple times");
                switch (optarg[0]) {
                    case '0': flags1 |= D3DCOMPILE_OPTIMIZATION_LEVEL0; break;
                    case '1': flags1 |= D3DCOMPILE_OPTIMIZATION_LEVEL1; break;
                    case '2': flags1 |= D3DCOMPILE_OPTIMIZATION_LEVEL2; break;
                    case '3': flags1 |= D3DCOMPILE_OPTIMIZATION_LEVEL3; break;
                }
                optLevelSet = true;
            break;
            case 'F':
                switch (optarg[0]) {
                    case 'o':
                        if (objectFile != -1)
                            print_error("'-Fo' option used more than once");
                        objectFile = create_file(optarg[1] ? &optarg[1] : argv[optind++]);
                    break;
                    case 'h':
                        if (headerFile != -1)
                            print_error("'-Fh' option used more than once");
                        headerFile = create_file(optarg[1] ? &optarg[1] : argv[optind++]);
                    break;
                    case 'c':
                        if (assemblyFile != -1)
                            print_error("'-Fc' option used more than once");
                        assemblyFile = create_file(optarg[1] ? &optarg[1] : argv[optind++]);
                    break;
                }
                outputFileSet = true;
            break;
            case 'L':
            if (optarg[0] == 'D') {
                long int version = strtol(optarg[1] ? &optarg[1] : argv[optind++], NULL, 10);
                if (33 <= version && version <= 43)
                    snprintf(image.name, sizeof(image.name), "engine/D3DCompiler_%ld.dll", version);
                else
                    print_error("Compiler version '%ld' is unsupported", version);
            }
            break;
            case 'P':
                if (processFile != -1)
                    print_error("'-P' option used more than once");
                processFile = create_file(optarg);
            break;
            case 'D':
            if (defineIndex < FXC_MAX_MACROS) {
                char* sep = strchr(optarg, '=');
                if (sep) *sep = '\0';
                defines[defineIndex].Name = optarg;
                defines[defineIndex].Definition = sep ? sep + 1 : "1";
            } else {
                print_error("Too many macros defined (%d)", defineIndex);
            }
            break;
            case '?':
                print_usage();
            break;
            case '\0':
                // Apply the long option flag bits and reset them
                flags1 |= flagsBit1;
                flags2 |= flagsBit2;
                flagsAsm |= flagsBitAsm;
                flagsBit1 = flagsBit2 = flagsBitAsm = 0;
            break;
            default:
                print_error("'-%c' is not implemented yet", c);
            break;
        }
    }

    if (flags1 & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR && flags1 & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR)
        print_error("Cannot specify -Zpr and -Zpc together");
    if (flags1 & D3DCOMPILE_AVOID_FLOW_CONTROL && flags1 & D3DCOMPILE_PREFER_FLOW_CONTROL)
        print_error("Cannot specify -Gfa and -Gfp together");
    if (flags1 & D3DCOMPILE_ENABLE_STRICTNESS && flags1 & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY) {
        print_error_msg(
            "Strictness and compatibility mode are mutually exclusive:\n"
            "For DX9 compatibility mode, use -Gec\n"
            "For regular DX10 shaders and effects, use regular mode (do not specify -Gecor -Ges)\n"
            "For clean future-proof DX10 shaders and effects, use strict mode (-Ges)"
        );
    }
    if (processFile != -1 && !target)
        print_error("cannot preprocess to file and compile at the same time");

    if (!target)
        target = "fx_2_0";

    if (!outputFileSet)
        assemblyFile = STDOUT_FILENO;

    // Load the D3DCompiler module.
    if (pe_load_library(image.name, &image.image, &image.size) == false) {
        LogMessage("You must add the dll and vdm files to the engine directory");
        return EXIT_FAILURE;
    }

    // Handle relocations, imports, etc.
    link_pe_images(&image, 1);

    if (get_export("D3DCompile", &D3DCompile) == -1) {
        if (get_export("D3DCompileFromMemory", &D3DCompile) == -1) {
            print_error("Failed to resolve D3DCompile entrypoint");
        }
    }

    if (get_export("D3DPreprocess", &D3DPreprocess) == -1) {
        if (get_export("D3DPreprocessFromMemory", &D3DPreprocess) == -1) {
            print_error("Failed to resolve D3DPreprocess entrypoint");
        }
    }

    if (get_export("D3DDisassemble", &D3DDisassemble) == -1) {
        if (get_export("D3DDisassembleCode", &D3DDisassemble) == -1) {
            print_error("Failed to resolve D3DDisassemble entrypoint");
        }
    }

    EXCEPTION_DISPOSITION ExceptionHandler(struct _EXCEPTION_RECORD *ExceptionRecord,
            struct _EXCEPTION_FRAME *EstablisherFrame,
            struct _CONTEXT *ContextRecord,
            struct _EXCEPTION_FRAME **DispatcherContext)
    {
        LogMessage("Toplevel Exception Handler Caught Exception");
        abort();
    }

    VOID ResourceExhaustedHandler(int Signal)
    {
        print_error("Resource Limits Exhausted, Signal %s", strsignal(Signal));
    }

    setup_nt_threadinfo(ExceptionHandler);

    // Call DllMain()
    image.entry("FXC", DLL_PROCESS_ATTACH, NULL);

    // Install usage limits to prevent system crash.
    setrlimit(RLIMIT_CORE, &kUsageLimits[RLIMIT_CORE]);
    setrlimit(RLIMIT_CPU, &kUsageLimits[RLIMIT_CPU]);
    setrlimit(RLIMIT_FSIZE, &kUsageLimits[RLIMIT_FSIZE]);
    setrlimit(RLIMIT_NOFILE, &kUsageLimits[RLIMIT_NOFILE]);

    signal(SIGXCPU, ResourceExhaustedHandler);
    signal(SIGXFSZ, ResourceExhaustedHandler);

    if (optind > argc - 1)
        print_error("No files specified");
    else if (optind < argc - 1)
        print_error("Too many files specified ('%s' was the last one)", argv[argc - 1]);
    else {
        SIZE_T srcSize;
        PVOID srcData = read_file(AT_FDCWD, argv[optind], &srcSize);
        
        if (!srcData)
            fprintf(stderr, "failed to open file: %s\n", argv[optind]);

        if (processFile != -1) {
            hr = D3DPreprocess(
                srcData,
                srcSize,
                argv[optind],
                defines,
                &includer,
                &pCode,
                &pError
            );
        } else {
            hr = D3DCompile(
                srcData,
                srcSize,
                argv[optind],
                defines,
                &includer,
                entryPoint,
                target,
                flags1,
                flags2,
                &pCode,
                &pError
            );
        }
        
        free(srcData);
    }

    if (pError) {
        fprintf(stderr, "%s\n", (LPCSTR)ID3D10Blob_GetBufferPointer(pError));
        ID3D10Blob_Release(pError);
    }

    if (hr != 0 || !pCode) {
        fprintf(stderr, "compilation failed; no code produced\n");
        return EXIT_FAILURE;
    } else {
        PBYTE out = (PBYTE)ID3D10Blob_GetBufferPointer(pCode);
        SIZE_T size = ID3D10Blob_GetBufferSize(pCode);

        if (headerFile != -1) {
            dprintf(headerFile, "const unsigned char g_%s[] =\n{\n    ", entryPoint);
            for (SIZE_T i = 0; i < size; i++) {
                dprintf(headerFile, "%3u", out[i]);
                if (i < size - 1)
                    dprintf(headerFile, ", ");
                if (i % 6 == 5)
                    dprintf(headerFile, "\n    ");
            }
            dprintf(headerFile, "\n};\n");
            if (headerFile != STDOUT_FILENO) {
                close(headerFile);
                printf("compilation header save succeeded\n");
            }
        }

        if (objectFile != -1) {
            SIZE_T wrote = write(objectFile, out, size);
            assert(wrote == size);
            if (objectFile != STDOUT_FILENO) {
                close(objectFile);
                printf("compilation object save succeeded\n");
            }
        }

        if (processFile != -1) {
            SIZE_T wrote = write(processFile, out, size);
            assert(wrote == size);
            if (processFile != STDOUT_FILENO)
                close(processFile);
        }

        if (assemblyFile != -1) {
            ID3DBlob *disassm;
            hr = D3DDisassemble(out, size, flagsAsm, cmdLine, &disassm);
            if (hr != 0 || !pCode) {
                fprintf(stderr, "disassembly failed; no disassembly produced\n");
            } else {
                out = (PBYTE)ID3D10Blob_GetBufferPointer(disassm);
                size = ID3D10Blob_GetBufferSize(disassm);
                SIZE_T wrote = write(assemblyFile, out, size);
                assert(wrote == size);
                ID3D10Blob_Release(disassm);
            }
            if (assemblyFile != STDOUT_FILENO)
                close(assemblyFile);
        }

        ID3D10Blob_Release(pCode);
    }

    free(cmdLine);
    return EXIT_SUCCESS;
}
