//
// Portions of this code are based on wine, which included this notice:
//
// msvcrt.dll math functions
//
// Copyright 2000 Jon Griffiths
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
//

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <search.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <error.h>
#include <wchar.h>
#include <float.h>
#include <fpu_control.h>

#include "winnt_types.h"
#include "codealloc.h"
#include "pe_linker.h"
#include "ntoskernel.h"
#include "log.h"
#include "winexports.h"
#include "util.h"
#include "winstrings.h"

#define MSVCRT__SW_INEXACT      0x00000001 /* inexact (precision) */
#define MSVCRT__SW_UNDERFLOW    0x00000002 /* underflow */
#define MSVCRT__SW_OVERFLOW     0x00000004 /* overflow */
#define MSVCRT__SW_ZERODIVIDE   0x00000008 /* zero divide */
#define MSVCRT__SW_INVALID      0x00000010 /* invalid */

#define MSVCRT__SW_UNEMULATED     0x00000040  /* unemulated instruction */
#define MSVCRT__SW_SQRTNEG        0x00000080  /* square root of a neg number */
#define MSVCRT__SW_STACKOVERFLOW  0x00000200  /* FP stack overflow */
#define MSVCRT__SW_STACKUNDERFLOW 0x00000400  /* FP stack underflow */

#define MSVCRT__SW_DENORMAL     0x00080000 /* denormal status bit */

#define MSVCRT__MCW_EM        0x0008001f
#define MSVCRT__MCW_IC        0x00040000
#define MSVCRT__MCW_RC        0x00000300
#define MSVCRT__MCW_PC        0x00030000
#define MSVCRT__MCW_DN        0x03000000

#define MSVCRT__EM_INVALID    0x00000010
#define MSVCRT__EM_DENORMAL   0x00080000
#define MSVCRT__EM_ZERODIVIDE 0x00000008
#define MSVCRT__EM_OVERFLOW   0x00000004
#define MSVCRT__EM_UNDERFLOW  0x00000002
#define MSVCRT__EM_INEXACT    0x00000001
#define MSVCRT__IC_AFFINE     0x00040000
#define MSVCRT__IC_PROJECTIVE 0x00000000
#define MSVCRT__RC_CHOP       0x00000300
#define MSVCRT__RC_UP         0x00000200
#define MSVCRT__RC_DOWN       0x00000100
#define MSVCRT__RC_NEAR       0x00000000
#define MSVCRT__PC_24         0x00020000
#define MSVCRT__PC_53         0x00010000
#define MSVCRT__PC_64         0x00000000
#define MSVCRT__DN_SAVE       0x00000000
#define MSVCRT__DN_FLUSH      0x01000000
#define MSVCRT__DN_FLUSH_OPERANDS_SAVE_RESULTS 0x02000000
#define MSVCRT__DN_SAVE_OPERANDS_FLUSH_RESULTS 0x03000000
#define MSVCRT__EM_AMBIGUOUS  0x80000000

#define PF_XMMI64_INSTRUCTIONS_AVAILABLE 10
extern BOOL WINAPI IsProcessorFeaturePresent(DWORD ProcessorFeature);

typedef void (*_PVFV)();

static void _initterm(const _PVFV *ppfn, const _PVFV *end)
{
    do
    {
        _PVFV pfn = *++ppfn;
        if (pfn)
        {
            pfn();
        }
    } while (ppfn < end);
}

typedef int (*_PIFV)();

static int _initterm_e(const _PIFV *ppfn, const _PIFV *end)
{
    do
    {
        _PIFV pfn = *++ppfn;
        if (pfn)
        {
            int err = pfn();
            if (err) return err;
        }
    } while (ppfn < end);

    return 0;
}

typedef int (*_onexit_t)(void);

static void _onexit_handler(int status, void* func)
{
    ((_onexit_t)func)();
}

static _onexit_t _onexit(_onexit_t func)
{
    on_exit(_onexit_handler, func);
    return func;
}

static _onexit_t __dllonexit(_onexit_t func, _PVFV **pbegin, _PVFV **pend)
{
    return _onexit(func);
}

static void _lock(int locknum)
{
    DebugLog("%d", locknum);
    return;
}

static void _unlock(int locknum)
{
    DebugLog("%d", locknum);
    return;
}

static void * operator_new(uint32_t sz)
{
    return malloc(sz);
}

static void operator_delete(void* ptr)
{
    free(ptr);
}

int __control87_2( unsigned int newval, unsigned int mask,
                         unsigned int *x86_cw, unsigned int *sse2_cw )
{
    unsigned long fpword;
    unsigned int flags;
    unsigned int old_flags;

    if (x86_cw)
    {
        __asm__ __volatile__( "fstcw %0" : "=m" (fpword) );

        /* Convert into mask constants */
        flags = 0;
        if (fpword & 0x1)  flags |= MSVCRT__EM_INVALID;
        if (fpword & 0x2)  flags |= MSVCRT__EM_DENORMAL;
        if (fpword & 0x4)  flags |= MSVCRT__EM_ZERODIVIDE;
        if (fpword & 0x8)  flags |= MSVCRT__EM_OVERFLOW;
        if (fpword & 0x10) flags |= MSVCRT__EM_UNDERFLOW;
        if (fpword & 0x20) flags |= MSVCRT__EM_INEXACT;
        switch (fpword & 0xc00)
        {
        case 0xc00: flags |= MSVCRT__RC_UP|MSVCRT__RC_DOWN; break;
        case 0x800: flags |= MSVCRT__RC_UP; break;
        case 0x400: flags |= MSVCRT__RC_DOWN; break;
        }
        switch (fpword & 0x300)
        {
        case 0x0:   flags |= MSVCRT__PC_24; break;
        case 0x200: flags |= MSVCRT__PC_53; break;
        case 0x300: flags |= MSVCRT__PC_64; break;
        }
        if (fpword & 0x1000) flags |= MSVCRT__IC_AFFINE;

        if (mask)
        {
            flags = (flags & ~mask) | (newval & mask);

            /* Convert (masked) value back to fp word */
            fpword = 0;
            if (flags & MSVCRT__EM_INVALID)    fpword |= 0x1;
            if (flags & MSVCRT__EM_DENORMAL)   fpword |= 0x2;
            if (flags & MSVCRT__EM_ZERODIVIDE) fpword |= 0x4;
            if (flags & MSVCRT__EM_OVERFLOW)   fpword |= 0x8;
            if (flags & MSVCRT__EM_UNDERFLOW)  fpword |= 0x10;
            if (flags & MSVCRT__EM_INEXACT)    fpword |= 0x20;
            switch (flags & MSVCRT__MCW_RC)
            {
            case MSVCRT__RC_UP|MSVCRT__RC_DOWN: fpword |= 0xc00; break;
            case MSVCRT__RC_UP:                 fpword |= 0x800; break;
            case MSVCRT__RC_DOWN:               fpword |= 0x400; break;
            }
            switch (flags & MSVCRT__MCW_PC)
            {
            case MSVCRT__PC_64: fpword |= 0x300; break;
            case MSVCRT__PC_53: fpword |= 0x200; break;
            case MSVCRT__PC_24: fpword |= 0x0; break;
            }
            if (flags & MSVCRT__IC_AFFINE) fpword |= 0x1000;

            __asm__ __volatile__( "fldcw %0" : : "m" (fpword) );
        }
        *x86_cw = flags;
    }

    if (!sse2_cw) return 1;

    if (IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
    {
        __asm__ __volatile__( "stmxcsr %0" : "=m" (fpword) );

        /* Convert into mask constants */
        flags = 0;
        if (fpword & 0x80)   flags |= MSVCRT__EM_INVALID;
        if (fpword & 0x100)  flags |= MSVCRT__EM_DENORMAL;
        if (fpword & 0x200)  flags |= MSVCRT__EM_ZERODIVIDE;
        if (fpword & 0x400)  flags |= MSVCRT__EM_OVERFLOW;
        if (fpword & 0x800)  flags |= MSVCRT__EM_UNDERFLOW;
        if (fpword & 0x1000) flags |= MSVCRT__EM_INEXACT;
        switch (fpword & 0x6000)
        {
        case 0x6000: flags |= MSVCRT__RC_UP|MSVCRT__RC_DOWN; break;
        case 0x4000: flags |= MSVCRT__RC_UP; break;
        case 0x2000: flags |= MSVCRT__RC_DOWN; break;
        }
        switch (fpword & 0x8040)
        {
        case 0x0040: flags |= MSVCRT__DN_FLUSH_OPERANDS_SAVE_RESULTS; break;
        case 0x8000: flags |= MSVCRT__DN_SAVE_OPERANDS_FLUSH_RESULTS; break;
        case 0x8040: flags |= MSVCRT__DN_FLUSH; break;
        }

        if (mask)
        {
            old_flags = flags;
            mask &= MSVCRT__MCW_EM | MSVCRT__MCW_RC | MSVCRT__MCW_DN;
            flags = (flags & ~mask) | (newval & mask);

            if (flags != old_flags)
            {
                /* Convert (masked) value back to fp word */
                fpword = 0;
                if (flags & MSVCRT__EM_INVALID)    fpword |= 0x80;
                if (flags & MSVCRT__EM_DENORMAL)   fpword |= 0x100;
                if (flags & MSVCRT__EM_ZERODIVIDE) fpword |= 0x200;
                if (flags & MSVCRT__EM_OVERFLOW)   fpword |= 0x400;
                if (flags & MSVCRT__EM_UNDERFLOW)  fpword |= 0x800;
                if (flags & MSVCRT__EM_INEXACT)    fpword |= 0x1000;
                switch (flags & MSVCRT__MCW_RC)
                {
                case MSVCRT__RC_UP|MSVCRT__RC_DOWN: fpword |= 0x6000; break;
                case MSVCRT__RC_UP:                 fpword |= 0x4000; break;
                case MSVCRT__RC_DOWN:               fpword |= 0x2000; break;
                }
                switch (flags & MSVCRT__MCW_DN)
                {
                case MSVCRT__DN_FLUSH_OPERANDS_SAVE_RESULTS: fpword |= 0x0040; break;
                case MSVCRT__DN_SAVE_OPERANDS_FLUSH_RESULTS: fpword |= 0x8000; break;
                case MSVCRT__DN_FLUSH:                       fpword |= 0x8040; break;
                }
                __asm__ __volatile__( "ldmxcsr %0" : : "m" (fpword) );
            }
        }
        *sse2_cw = flags;
    }
    else *sse2_cw = 0;

    return 1;
}

unsigned int _control87(unsigned int newval, unsigned int mask)
{
    unsigned int flags = 0;
    unsigned int sse2_cw;

    __control87_2( newval, mask, &flags, &sse2_cw );

    if ((flags ^ sse2_cw) & (MSVCRT__MCW_EM | MSVCRT__MCW_RC)) flags |= MSVCRT__EM_AMBIGUOUS;
    flags |= sse2_cw;
    return flags;
}

unsigned int _controlfp(unsigned int newval, unsigned int mask)
{
    return _control87( newval, mask & ~MSVCRT__EM_DENORMAL );
}

static unsigned int _clearfp(void)
{
    unsigned int flags = 0;
    unsigned long fpword;

    __asm__ __volatile__( "fnstsw %0; fnclex" : "=m" (fpword) );
    if (fpword & 0x1)  flags |= MSVCRT__SW_INVALID;
    if (fpword & 0x2)  flags |= MSVCRT__SW_DENORMAL;
    if (fpword & 0x4)  flags |= MSVCRT__SW_ZERODIVIDE;
    if (fpword & 0x8)  flags |= MSVCRT__SW_OVERFLOW;
    if (fpword & 0x10) flags |= MSVCRT__SW_UNDERFLOW;
    if (fpword & 0x20) flags |= MSVCRT__SW_INEXACT;

    if (IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
    {
        __asm__ __volatile__( "stmxcsr %0" : "=m" (fpword) );
        if (fpword & 0x1)  flags |= MSVCRT__SW_INVALID;
        if (fpword & 0x2)  flags |= MSVCRT__SW_DENORMAL;
        if (fpword & 0x4)  flags |= MSVCRT__SW_ZERODIVIDE;
        if (fpword & 0x8)  flags |= MSVCRT__SW_OVERFLOW;
        if (fpword & 0x10) flags |= MSVCRT__SW_UNDERFLOW;
        if (fpword & 0x20) flags |= MSVCRT__SW_INEXACT;
        fpword &= ~0x3f;
        __asm__ __volatile__( "ldmxcsr %0" : : "m" (fpword) );
    }
    return flags;
}

static int _fpclass(double x)
{
    switch (fpclassify(x))
    {
        case FP_NAN: return 0x2;
        case FP_INFINITE: return signbit(x) ? 0x4 : 0x200;
        case FP_SUBNORMAL: return signbit(x) ? 0x10 : 0x80;
        case FP_ZERO: return signbit(x) ? 0x20 : 0x40;
        default: return signbit(x) ? 0x8 : 0x100;
    }
}

static size_t _mbstrlen (const char *s)
{
    mbstate_t state;
    size_t result = 0;
    size_t nbytes;
    memset (&state, '\0', sizeof (state));
    while ((nbytes = mbrlen (s, MB_LEN_MAX, &state)) > 0)
    {
        if (nbytes >= (size_t) -2)
        {
            /* Something is wrong.  */
            errno = EILSEQ;
            return (size_t) -1;
        }
        s += nbytes;
        ++result;
    }
    return result;
}

static int _isnan(double x)
{
    return isnan(x);
}

static void _purecall(void)
{
    DebugLog("Pure virtual call attempted, aborting");
    abort();
}

void _amsg_exit(int rterrnum)
{
    error(255, rterrnum, "runtime error ");
}

DECLARE_CRT_EXPORT("_initterm", _initterm);
DECLARE_CRT_EXPORT("_initterm_e", _initterm_e);
DECLARE_CRT_EXPORT("_onexit", _onexit);
DECLARE_CRT_EXPORT("__dllonexit", __dllonexit);
DECLARE_CRT_EXPORT("_lock", _lock);
DECLARE_CRT_EXPORT("_unlock", _unlock);
DECLARE_CRT_EXPORT("??2@YAPAXI@Z", operator_new);
DECLARE_CRT_EXPORT("??3@YAXPAX@Z", operator_delete);
DECLARE_CRT_EXPORT("malloc", malloc);
DECLARE_CRT_EXPORT("free", free);
DECLARE_CRT_EXPORT("setlocale", setlocale);
DECLARE_CRT_EXPORT("_strdup", strdup);
DECLARE_CRT_EXPORT("getenv", getenv);
DECLARE_CRT_EXPORT("_controlfp", _controlfp);
DECLARE_CRT_EXPORT("_clearfp", _clearfp);
DECLARE_CRT_EXPORT("_stricmp", strcasecmp);
DECLARE_CRT_EXPORT("_strnicmp", strncasecmp);
DECLARE_CRT_EXPORT("_finite", finite);
DECLARE_CRT_EXPORT("floor", floor);
DECLARE_CRT_EXPORT("_fpclass", _fpclass);
DECLARE_CRT_EXPORT("_CIfmod", fmod);
DECLARE_CRT_EXPORT("_CItanh", tanh);
DECLARE_CRT_EXPORT("_CItan", tan);
DECLARE_CRT_EXPORT("_CIsinh", sinh);
DECLARE_CRT_EXPORT("_CIsin", sin);
DECLARE_CRT_EXPORT("_CIlog", log);
DECLARE_CRT_EXPORT("_CIpow", pow);
DECLARE_CRT_EXPORT("_CIexp", exp);
DECLARE_CRT_EXPORT("_CIsqrt", sqrt);
DECLARE_CRT_EXPORT("_CIcosh", cosh);
DECLARE_CRT_EXPORT("_CIcos", cos);
DECLARE_CRT_EXPORT("_CIatan2", atan2);
DECLARE_CRT_EXPORT("_CIatan", atan);
DECLARE_CRT_EXPORT("_CIasin", asin);
DECLARE_CRT_EXPORT("_CIacos", acos);
DECLARE_CRT_EXPORT("atof", atof);
DECLARE_CRT_EXPORT("_mbstrlen", _mbstrlen);
DECLARE_CRT_EXPORT("modf", modf);
DECLARE_CRT_EXPORT("_isnan", _isnan);
DECLARE_CRT_EXPORT("ceil", ceil);
DECLARE_CRT_EXPORT("qsort", qsort);
DECLARE_CRT_EXPORT("_purecall", _purecall);
DECLARE_CRT_EXPORT("_amsg_exit", _amsg_exit);
DECLARE_CRT_EXPORT("?terminate@@YAXXZ", abort);
