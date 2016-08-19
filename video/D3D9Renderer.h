#pragma once

#include <map>
#include <memory>
#include <d3dcompiler.h>

#include "BaseWnd.h"
#include "Looper.h"
#include "MultipleBuffering.h"
#include "ComPtrDefs.h"
#include "D3DPsVideoProcessor.h"

using namespace std;

namespace WinRTCSDK{

#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2')
#define D3DFMT_I420 (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0')

struct VideoFrameInfo
{
	VideoFrameInfo() : pSample(NULL), 
		format(D3DFMT_UNKNOWN), image(NULL), width(0), height(0),
		stride(0), rotation(0), bMirroring(FALSE), bMicMuteIcon(FALSE){}

	// for DXVA
	_com_ptr_IDirect3DSurface9 pSample;
	// for software decoder
	D3DFORMAT format;
	BYTE     *image; 
	int       width;
	int       height;
	int       stride;
	int       rotation; // 0:0, 1:90, 2:180, 3:270
	// common info
	BOOL      bMirroring;
	BOOL      bMicMuteIcon;
};

struct IconData
{
	IconData(){argb = NULL; width=0; height=0;}
	~IconData(){ if ( argb ) free (argb);}
	
	VOID Set(BYTE* icon, int w, int h)
	{
		if (argb) free (argb); // free old one
		width = w;
		height = h;
		argb = (BYTE*) ::malloc(w * h * 4);
		::memcpy(argb, icon, w*h*4);
	}
	BYTE* argb; int width; int height ;
private : 
	IconData & operator = (IconData & r);
	IconData (IconData & r);
};

class D3D9Video
{
private:
	_com_ptr_IDirect3D9Ex            pD3DEx_; // this is a passed in param

	_com_ptr_IDirect3DSwapChain9     pSwapChain_;
    _com_ptr_IDirect3DDevice9Ex      pDevice_;
    _com_ptr_IDirect3DDeviceManager9 pD3DDevManager_;

	INT                              width_;
    INT                              height_;
	HWND                             hwnd_; // for windowed render
	_com_ptr_IDirect3DTexture9       micMuteIcon_;
	D3DPRESENT_PARAMETERS            d3dpp_ ;
	// video processing
	D3DPsVideoProcessor             *pPsVideoProcessor_;
private:
	HRESULT   _Destroy ();
    HRESULT   _DrawVideoPS(VideoFrameInfo &frame);
    HRESULT   _DrawVideoDXVA(VideoFrameInfo &frame);

public:
    ~D3D9Video ();
    D3D9Video (_com_ptr_IDirect3D9Ex pD3DEx, HWND hwnd);

public:
	_com_ptr_IDirect3DDeviceManager9 GetD3DDeviceManager9();
	HRESULT Create (const IconData * iconData);
	VOID    Reset(UINT width, UINT height); // reset the video on first frame!
	HRESULT DrawVideo(VideoFrameInfo &frame);
	INT     GetWidth() ;
	INT     GetHeight() ;
};

class D3D9Renderer
{
private:
	// locks for D3D9Video object deletion.
	map< D3D9Video*, std::shared_ptr<Mutex>> deletionLocks_;
	Mutex                          maplock_;
	Looper	                       dispatcher_;
	map <string, D3D9Video *>      videoList_; 
    _com_ptr_IDirect3D9Ex          pD3DEx_;
	IconData                       muteIconData_;
private:
    void       _Destroy();
	D3D9Video* _GetOrResetD3DVideo(string videoId, int width, int height);

public:
    HRESULT Reset();
    HRESULT Create();

	// draw video frame
	HRESULT DrawVideo(string videoID, VideoFrameInfo &frame);

	// remove the video stream
	void    RemoveVideo(string videoID);

	// create the video stream
	void    CreateVideo(string videoID, HWND hwnd);

	void    SetMicMuteIcon(BYTE* argb, int w, int h);

	// provide DXVA for other modules
	_com_ptr_IDirect3DDeviceManager9 GetD3DDeviceManager9(string videoID);


public:
    D3D9Renderer(void);
    ~D3D9Renderer(void);
};

} // namespace WinRTCSDK