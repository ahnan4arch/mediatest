#pragma once

#include <memory.h>
#include <stdio.h>
#include <tchar.h>
#include "mfxdefs.h"

// file 
#define MSDK_FOPEN(file, name, mode) _tfopen_s(&file, name, mode)

#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
#endif

// time
#define MSDK_SLEEP(msec) Sleep(msec)
#define MSDK_USLEEP(usec) \
{ \
    LARGE_INTEGER due; \
    due.QuadPart = -(10*(int)usec); \
    HANDLE t = CreateWaitableTimer(NULL, TRUE, NULL); \
    SetWaitableTimer(t, &due, 0, NULL, NULL, 0); \
    WaitForSingleObject(t, INFINITE); \
    CloseHandle(t); \
}

#define MSDK_GET_TIME(T,S,F) ((mfxF64)((T)-(S))/(mfxF64)(F))

typedef mfxI64 msdk_tick;
typedef LARGE_INTEGER mfxTime;

inline msdk_tick msdk_time_get_tick(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceCounter(&t1);
    return t1.QuadPart;
}

inline msdk_tick msdk_time_get_frequency(void)
{
    LARGE_INTEGER t1;

    QueryPerformanceFrequency(&t1);
    return t1.QuadPart;
}

#define MSDK_DEC_WAIT_INTERVAL 300000
#define MSDK_ENC_WAIT_INTERVAL 300000
#define MSDK_VPP_WAIT_INTERVAL 300000
#define MSDK_SURFACE_WAIT_INTERVAL 20000
#define MSDK_WAIT_INTERVAL MSDK_DEC_WAIT_INTERVAL+3*MSDK_VPP_WAIT_INTERVAL+MSDK_ENC_WAIT_INTERVAL // an estimate for the longest pipeline we have in samples
#define MSDK_INVALID_SURF_IDX 0xFFFF
#define MSDK_MAX_FILENAME_LEN 1024

// error trace
#define MSDK_PRINT_RET_MSG(ERR) {msdk_printf(MSDK_STRING("\nReturn on error: error code %d,\t%s\t%d\n\n"), (int)ERR, MSDK_STRING(__FILE__), __LINE__);}
#define MSDK_TRACE_LEVEL(level, ERR) if (level <= msdk_trace_get_level()) {msdk_err<<NoFullPath(MSDK_STRING(__FILE__)) << MSDK_STRING(" :")<< __LINE__ <<MSDK_STRING(" [") \
    <<level<<MSDK_STRING("] ") << ERR << std::endl;}
#define MSDK_TRACE_CRITICAL(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_CRITICAL, ERR)
#define MSDK_TRACE_ERROR(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_ERROR, ERR)
#define MSDK_TRACE_WARNING(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_WARNING, ERR)
#define MSDK_TRACE_INFO(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_INFO, ERR)
#define MSDK_TRACE_DEBUG(ERR) MSDK_TRACE_LEVEL(MSDK_TRACE_LEVEL_DEBUG, ERR)

#define MSDK_CHECK_ERROR(P, X, ERR)              {if ((X) == (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_NOT_EQUAL(P, X, ERR)          {if ((X) != (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_RESULT(P, X, ERR)             {if ((X) > (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_PARSE_RESULT(P, X, ERR)       {if ((X) > (P)) {return ERR;}}
#define MSDK_CHECK_RESULT_SAFE(P, X, ERR, ADD)   {if ((X) > (P)) {ADD; MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_IGNORE_MFX_STS(P, X)                {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_CHECK_POINTER(P, ...)               {if (!(P)) {return __VA_ARGS__;}}
#define MSDK_CHECK_POINTER_NO_RET(P)             {if (!(P)) {return;}}
#define MSDK_CHECK_POINTER_SAFE(P, ERR, ADD)     {if (!(P)) {ADD; return ERR;}}
#define MSDK_BREAK_ON_ERROR(P)                   {if (MFX_ERR_NONE != (P)) break;}
#define MSDK_SAFE_DELETE_ARRAY(P)                {if (P) {delete[] P; P = NULL;}}
#define MSDK_SAFE_RELEASE(X)                     {if (X) { X->Release(); X = NULL; }}
#define MSDK_SAFE_FREE(X)                        {if (X) { free(X); X = NULL; }}

#ifndef MSDK_SAFE_DELETE
#define MSDK_SAFE_DELETE(P)                      {if (P) {delete P; P = NULL;}}
#endif // MSDK_SAFE_DELETE

#define MSDK_ZERO_MEMORY(VAR)                    {memset(&VAR, 0, sizeof(VAR));}
#define MSDK_MAX(A, B)                           (((A) > (B)) ? (A) : (B))
#define MSDK_MIN(A, B)                           (((A) < (B)) ? (A) : (B))
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#define MSDK_ALIGN(value, alignment)             (alignment) * ( (value) / (alignment) + (((value) % (alignment)) ? 1 : 0))
#define MSDK_ARRAY_LEN(value)                    (sizeof(value) / sizeof(value[0]))

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(par) (par)
#endif

#ifndef MFX_PRODUCT_VERSION
#define MFX_PRODUCT_VERSION "1.0.0.0"
#endif

#define MSDK_SAMPLE_VERSION MSDK_STRING(MFX_PRODUCT_VERSION)

#define MFX_IMPL_VIA_MASK(x) (0x0f00 & (x))


// strings

#define MSDK_STRING(x) _T(x)
#define MSDK_CHAR(x) _T(x)

typedef TCHAR msdk_char;

#define msdk_printf   _tprintf
#define msdk_fprintf  _ftprintf
#define msdk_sprintf  _stprintf_s // to be removed
#define msdk_vprintf  _vtprintf
#define msdk_strlen   _tcslen
#define msdk_strcmp   _tcscmp
#define msdk_strncmp  _tcsnicmp
#define msdk_strstr   _tcsstr
#define msdk_atoi     _ttoi
#define msdk_strtol   _tcstol
#define msdk_strtod   _tcstod
#define msdk_strchr   _tcschr
#define msdk_itoa_decimal(value, str)   _itow_s(value, str, 4, 10)
#define msdk_strcopy _tcscpy_s
#define msdk_strncopy_s _tcsncpy_s

#define MSDK_MEMCPY_BITSTREAM(bitstream, offset, src, count) memcpy_s((bitstream).Data + (offset), (bitstream).MaxLength - (offset), (src), (count))
#define MSDK_MEMCPY_BUF(bufptr, offset, maxsize, src, count) memcpy_s((bufptr)+ (offset), (maxsize) - (offset), (src), (count))
#define MSDK_MEMCPY_VAR(dstVarName, src, count) memcpy_s(&(dstVarName), sizeof(dstVarName), (src), (count))
#define MSDK_MEMCPY(dst, src, count) memcpy_s(dst, (count), (src), (count))

#ifdef UNICODE
    #define msdk_cout std::wcout
    #define msdk_err std::wcerr
#else
    #define msdk_cout std::cout
    #define msdk_err std::cerr
#endif

typedef std::basic_string<msdk_char> msdk_string;
typedef std::basic_stringstream<msdk_char> msdk_stringstream;
typedef std::basic_ostream<msdk_char, std::char_traits<msdk_char> > msdk_ostream;
typedef std::basic_istream<msdk_char, std::char_traits<msdk_char> > msdk_istream;

// inline functions

inline mfxU16 msdk_atomic_inc16(volatile mfxU16 *pVariable)
{
    return _InterlockedIncrement16((volatile short*)pVariable);
}

/* Thread-safe 16-bit variable decrementing */
inline mfxU16 msdk_atomic_dec16(volatile mfxU16 *pVariable)
{
    return _InterlockedDecrement16((volatile short*)pVariable);
}
