#pragma once

#include <atlbase.h>
#include <d3d9.h>
#include <dxva2api.h>
#include "base_allocator.h"

enum eTypeHandle
{
    DXVA2_PROCESSOR     = 0x00,
    DXVA2_DECODER       = 0x01
};

struct directxMemId
{
    IDirect3DSurface9* m_surface;
    HANDLE m_handle;
};

class D3DFrameAllocator: public BaseFrameAllocator
{
public:
    D3DFrameAllocator();
    virtual ~D3DFrameAllocator();

    virtual mfxStatus Init(IDirect3DDeviceManager9 *pManager);
    virtual mfxStatus Close();

    virtual IDirect3DDeviceManager9* GetDeviceManager()
    {
        return m_manager;
    };

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    CComPtr<IDirect3DDeviceManager9> m_manager;
    CComPtr<IDirectXVideoDecoderService> m_decoderService;
    CComPtr<IDirectXVideoProcessorService> m_processorService;
    HANDLE m_hDecoder;
    HANDLE m_hProcessor;
    DWORD m_surfaceUsage;
};

