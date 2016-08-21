#pragma once

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN   
#include <windows.h>
#include <Windowsx.h>
#include <shlwapi.h>
#include <tchar.h>
#include <dshow.h>
#include <Mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <Dbt.h>
#include <ks.h>
#include <devpkey.h>
#include <d3d9.h>
#include <Dxva2api.h>
#include <ks.h>
#include <ksmedia.h>

// C RunTime Header Files
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>

// C++ headers
#include <vector>
#include <string>

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}
