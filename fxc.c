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
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/unistd.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <mcheck.h>
#include <err.h>
#include <getopt.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "util.h"
#include "hook.h"
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

typedef struct _D3D_SHADER_MACRO
{
    LPCSTR Name;
    LPCSTR Definition;
} D3D_SHADER_MACRO;

typedef GUID IID;
typedef const IID* REFIID;

typedef struct ID3D10Blob ID3D10Blob;
typedef struct ID3D10BlobVtbl
{
    HRESULT (*QueryInterface )(
        ID3D10Blob * This,
        /* [in] */ REFIID riid,
        /* [annotation][iid_is][out] */
        void **ppvObject);
    ULONG (*AddRef )(
        ID3D10Blob * This);
    ULONG (*Release )(
        ID3D10Blob * This);
    void* (*GetBufferPointer )(
        ID3D10Blob * This);
    SIZE_T (*GetBufferSize )(
        ID3D10Blob * This);
} ID3D10BlobVtbl;

struct ID3D10Blob
{
    const struct ID3D10BlobVtbl *lpVtbl;
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

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(uintptr_t)1)
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

void print_usage()
{
    printf("Usage: fxc <options> <files>\n");
    printf("\n");
    printf("   -?, -help           print this message\n");
    printf("\n");
    printf("   -T <profile>        target profile\n");
    printf("   -E <name>           entrypoint name\n");
//  printf("   -I <include>        additional include path\n");
//  printf("   -Vi                 display details about the include process\n");
//  printf("\n");
//  printf("   -Od                 disable optimizations\n");
//  printf("   -Op                 disable preshaders\n");
//  printf("   -O{0,1,2,3}         optimization level 0..3.  1 is default\n");
//  printf("   -WX                 treat warnings as errors\n");
//  printf("   -Vd                 disable validation\n");
//  printf("   -Zi                 enable debugging information\n");
//  printf("   -Zss                debug name with source information\n");
//  printf("   -Zsb                debug name with only binary information\n");
//  printf("   -Zpr                pack matrices in row-major order\n");
//  printf("   -Zpc                pack matrices in column-major order\n");
//  printf("\n");
//  printf("   -Gpp                force partial precision\n");
//  printf("   -Gfa                avoid flow control constructs\n");
//  printf("   -Gfp                prefer flow control constructs\n");
//  printf("   -Gdp                disable effect performance mode\n");
//  printf("   -Ges                enable strict mode\n");
//  printf("   -Gec                enable backwards compatibility mode\n");
//  printf("   -Gis                force IEEE strictness\n");
//  printf("   -Gch                compile as a child effect for FX 4.x targets\n");
    printf("\n");
    printf("   -Fo <file>          output object file\n");
//  printf("   -Fl <file>          output a library\n");
//  printf("   -Fc <file>          output assembly code listing file\n");
//  printf("   -Fx <file>          output assembly code and hex listing file\n");
//  printf("   -Fh <file>          output header file containing object code\n");
//  printf("   -Fe <file>          output warnings and errors to a specific file\n");
//  printf("   -Fd <file>          extract shader PDB and write to given file\n");
//  printf("   -Vn <name>          use <name> as variable name in header file\n");
//  printf("   -Cc                 output color coded assembly listings\n");
//  printf("   -Ni                 output instruction numbers in assembly listings\n");
//  printf("   -No                 output instruction byte offset in assembly listings\n");
//  printf("   -Lx                 output hexadecimal literals\n");
//  printf("\n");
//  printf("   -P <file>           preprocess to file (must be used alone)\n");
//  printf("\n");
//  printf("   @<file>             options response file\n");
//  printf("   -dumpbin            load a binary file rather than compiling\n");
//  printf("   -Qstrip_reflect     strip reflection data from 4_0+ shader bytecode\n");
//  printf("   -Qstrip_debug       strip debug information from 4_0+ shader bytecode\n");
//  printf("   -Qstrip_priv        strip private data from 4_0+ shader bytecode\n");
//  printf("   -Qstrip_rootsignature         strip root signature from shader bytecode\n");
//  printf("\n");
//  printf("   -setrootsignature <file>      attach root signature to shader bytecode\n");
//  printf("   -extractrootsignature <file>  extract root signature from shader bytecode\n");
//  printf("   -verifyrootsignature <file>   verify shader bytecode against root signature\n");
//  printf("\n");
//  printf("   -compress           compress DX10 shader bytecode from files\n");
//  printf("   -decompress         decompress bytecode from first file, output files should\n");
//  printf("                       be listed in the order they were in during compression\n");
//  printf("\n");
//  printf("   -shtemplate <file>  template shader file for merging/matching resources\n");
//  printf("   -mergeUAVs          merge UAV slots of template shader and current shader\n");
//  printf("   -matchUAVs          match template shader UAV slots in current shader\n");
//  printf("   -res_may_alias      assume that UAVs/SRVs may alias for cs_5_0+\n");
//  printf("   -enable_unbounded_descriptor_tables  enables unbounded descriptor tables\n");
//  printf("   -all_resources_bound  enable aggressive flattening in SM5.1+\n");
//  printf("\n");
//  printf("   -setprivate <file>  private data to add to compiled shader blob\n");
//  printf("   -getprivate <file>  save private data from shader blob\n");
//  printf("   -force_rootsig_ver <profile>  force root signature version (rootsig_1_1 if omitted)\n");
//  printf("\n");
    printf("   -D <id>=<text>      define macro\n");
    printf("   -LD <version>       Load specified D3DCompiler version\n");
//  printf("   -nologo             suppress copyright message\n");
    printf("\n");
    printf("   <profile>: cs_4_0 cs_4_1 cs_5_0 cs_5_1 ds_5_0 ds_5_1 gs_4_0 gs_4_1 gs_5_0\n");
    printf("      gs_5_1 hs_5_0 hs_5_1 lib_4_0 lib_4_1 lib_4_0_level_9_1\n");
    printf("      lib_4_0_level_9_1_vs_only lib_4_0_level_9_1_ps_only lib_4_0_level_9_3\n");
    printf("      lib_4_0_level_9_3_vs_only lib_4_0_level_9_3_ps_only lib_5_0 ps_2_0\n");
    printf("      ps_2_a ps_2_b ps_2_sw ps_3_0 ps_3_sw ps_4_0 ps_4_0_level_9_1\n");
    printf("      ps_4_0_level_9_3 ps_4_0_level_9_0 ps_4_1 ps_5_0 ps_5_1 rootsig_1_0\n");
    printf("      rootsig_1_1 tx_1_0 vs_1_1 vs_2_0 vs_2_a vs_2_sw vs_3_0 vs_3_sw vs_4_0\n");
    printf("      vs_4_0_level_9_1 vs_4_0_level_9_3 vs_4_0_level_9_0 vs_4_1 vs_5_0 vs_5_1\n");
    printf("\n");
    exit(0);
}

void print_error(const char* format, ...)
{
    fprintf(stderr, "\033[0;31m");
    va_list ap;
    va_start(ap, format);
        vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "%s\033[0m\n", ", use -? to get usage information");
    exit(1);
}

int main(int argc, char **argv)
{
    PIMAGE_DOS_HEADER DosHeader;
    PIMAGE_NT_HEADERS PeHeader;
    int c = 0, optionIndex = 0, defineIndex = 0;
    int inputFile = -1, objectFile = -1, headerFile = -1;
    LPCSTR target = NULL, entryPoint = NULL;
    ID3DBlob *pCode = NULL, *pError = NULL;
    HRESULT hr = 1;
    D3D_SHADER_MACRO defines[FXC_MAX_MACROS + 1] = { { NULL, NULL } };
    struct pe_image image = {
        .entry  = NULL,
        .name   = "engine/D3DCompiler_43.dll",
    };
    struct option longOptions[] = {
        {"help", no_argument, NULL, '?'},
        {0, 0, 0, 0}
    };
    
    while ((c = getopt_long_only(argc, argv, "T:E:I:V:O:W:Z:G:F:C:N:L:P:Q:D:?", longOptions, &optionIndex)) != -1) {
        switch(c) {
            case 'T':
                target = optarg;
            break;
            case 'E':
                entryPoint = optarg;
            break;
            case 'F':
                switch(optarg[0])
                {
                    case 'o':
                        if (objectFile != -1)
                            print_error("'-Fo' option used more than once");
                        objectFile = open(optarg[1] ? &optarg[1] : argv[optind++],
                                          O_WRONLY | O_CREAT | O_TRUNC,
                                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    break;
                    case 'h':
                        if (headerFile != -1)
                            print_error("'-Fh' option used more than once");
                        headerFile = open(optarg[1] ? &optarg[1] : argv[optind++],
                                          O_WRONLY | O_CREAT | O_TRUNC,
                                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
                    break;
                }
            break;
            case 'L':
            if (optarg[0] == 'D')
            {
                long int version = strtol(optarg[1] ? &optarg[1] : argv[optind++], NULL, 10);
                if (33 <= version && version <= 43)
                    snprintf(image.name, sizeof(image.name), "engine/D3DCompiler_%ld.dll", version);
                else
                    print_error("Compiler version '%ld' is unsupported", version);
            }
            break;
            case 'D':
            if (defineIndex < FXC_MAX_MACROS)
            {
                char* sep = strchr(optarg, '=');
                if (sep) *sep = '\0';
                defines[defineIndex].Name = optarg;
                defines[defineIndex].Definition = sep ? sep + 1 : "1";
            }
            else
            {
                print_error("Too many macros defined (%d)", defineIndex);
            }
            break;
            case '?':
                print_usage();
            break;
            default:
                print_error("'-%c' is not implemented yet", c);
            break;
        }
    }

    if (optind > argc - 1)
        print_error("No files specified");
    else if (optind < argc - 1)
        print_error("Too many files specified ('%s' was the last one)", argv[argc - 1]);
    else
        inputFile = open(argv[optind], O_RDONLY);

    // Load the D3DCompiler module.
    if (pe_load_library(image.name, &image.image, &image.size) == false) {
        LogMessage("You must add the dll and vdm files to the engine directory");
        return 1;
    }

    // Handle relocations, imports, etc.
    link_pe_images(&image, 1);

    // Fetch the headers to get base offsets.
    DosHeader   = (PIMAGE_DOS_HEADER) image.image;
    PeHeader    = (PIMAGE_NT_HEADERS)(image.image + DosHeader->e_lfanew);

    // Load any additional exports.
    if (!process_extra_exports(image.image, PeHeader->OptionalHeader.BaseOfCode, "engine/D3DCompiler.map")) {
#ifndef NDEBUG
        LogMessage("The map file wasn't found, symbols wont be available");
#endif
    } else {
        // Calculate the commands needed to get export and map symbols visible in gdb.
        if (IsGdbPresent()) {
            LogMessage("GDB: add-symbol-file %s %#x+%#x",
                       image.name,
                       image.image,
                       PeHeader->OptionalHeader.BaseOfCode);
            LogMessage("GDB: shell bash genmapsym.sh %#x+%#x symbols_%d.o < %s",
                       image.image,
                       PeHeader->OptionalHeader.BaseOfCode,
                       getpid(),
                       "engine/D3DCompiler.map");
            LogMessage("GDB: add-symbol-file symbols_%d.o 0", getpid());
            __debugbreak();
        }
    }

    if (get_export("D3DCompile", &D3DCompile) == -1) {
        if (get_export("D3DCompileFromMemory", &D3DCompile) == -1) {
            errx(EXIT_FAILURE, "Failed to resolve D3DCompile entrypoint");
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
        errx(EXIT_FAILURE, "Resource Limits Exhausted, Signal %s", strsignal(Signal));
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

# ifndef NDEBUG
    // Enable Maximum heap checking.
    mcheck_pedantic(NULL);
# endif

    if (inputFile == -1) {
        fprintf(stderr, "failed to open file: %s\n", argv[optind]);
    } else {
        const SIZE_T blockSize = getpagesize() * 16;
        SIZE_T srcSize = 0;
        PBYTE srcData = NULL;

        // Read data until the stream is empty
        srcData = malloc(blockSize);
        for (SIZE_T bytes = 0; (bytes = read(inputFile, srcData + srcSize, blockSize)) != 0; srcSize += bytes) {
            // If the entire buffer was filled we resize it so we can read the rest
            if (bytes == blockSize)
                srcData = realloc(srcData, srcSize + blockSize);
        }
        close(inputFile);

        hr = D3DCompile(
            srcData,
            srcSize,
            argv[optind],
            defines,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            0,
            0,
            &pCode,
            &pError
        );
    }

    if (pError) {
        fprintf(stderr, "%s\n", (LPCSTR)ID3D10Blob_GetBufferPointer(pError));
        ID3D10Blob_Release(pError);
    }

    if (hr != 0) {
        fprintf(stderr, "compilation failed; no code produced\n");
        return 1;
    } else if (pCode) {
        PBYTE out = (PBYTE)ID3D10Blob_GetBufferPointer(pCode);
        SIZE_T len = ID3D10Blob_GetBufferSize(pCode);

        if (headerFile != -1) {
            dprintf(headerFile, "const unsigned char g_%s[] =\n{\n    ", entryPoint);
            for (SIZE_T i = 0; i < len; i++) {
                dprintf(headerFile, "%3u", out[i]);
                if (i < len - 1)
                    dprintf(headerFile, ", ");
                if (i % 6 == 5)
                    dprintf(headerFile, "\n    ");
            }
            dprintf(headerFile, "\n};\n");
            close(headerFile);
            printf("compilation header save succeeded\n");
        }

        if (objectFile != -1) {
            write(objectFile, out, len);
            close(objectFile);
            printf("compilation object save succeeded\n");
        }

        ID3D10Blob_Release(pCode);
    }
    return 0;
}
