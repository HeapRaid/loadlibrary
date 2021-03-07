#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <cpuid.h>

#include "winnt_types.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"

#define PF_FLOATING_POINT_PRECISION_ERRATA 0
#define PF_XMMI_INSTRUCTIONS_AVAILABLE 6
#define PF_XMMI64_INSTRUCTIONS_AVAILABLE 10
#define PF_FASTFAIL_AVAILABLE 23
#define PF_MMX_INSTRUCTIONS_AVAILABLE 3

BOOL WINAPI IsProcessorFeaturePresent(DWORD ProcessorFeature)
{
    int eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);

    switch (ProcessorFeature) {
        case PF_XMMI_INSTRUCTIONS_AVAILABLE:
            return edx & bit_SSE;
        case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
            return edx & bit_SSE2;
        case PF_FLOATING_POINT_PRECISION_ERRATA:
            DebugLog("IsProcessorFeaturePresent(%u) => FALSE", ProcessorFeature);
            return FALSE;
        case PF_MMX_INSTRUCTIONS_AVAILABLE:
            return edx & bit_MMX;
        case PF_FASTFAIL_AVAILABLE: // NOTE: this will cause int 0x29
            DebugLog("IsProcessorFeaturePresent(%u) => TRUE", ProcessorFeature);
            return TRUE;
    }

    DebugLog("IsProcessorFeaturePresent(%u) => FALSE (Unknown)", ProcessorFeature);
    return FALSE;
}

DECLARE_CRT_EXPORT("IsProcessorFeaturePresent", IsProcessorFeaturePresent);
