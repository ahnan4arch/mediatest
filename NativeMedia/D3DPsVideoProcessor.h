#pragma once
#include "ComPtrDefs.h"

namespace WinRTCSDK{

//Flexible Vertex Format, FVF
struct CustomVertex
{
	FLOAT       x, y, z, rhw;   
	D3DCOLOR    diffuse;    
	FLOAT       tu, tv; // texture coordinates
};

// coustom vertex format
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

class D3DPsVideoProcessor
{
public:
	D3DPsVideoProcessor(void);
	~D3DPsVideoProcessor(void);
	HRESULT   Init(_com_ptr_IDirect3DDevice9Ex pd3dDevice_, UINT nWidth, UINT nHeight, D3DFORMAT nPixelFormat);
	void      UnInit();
	HRESULT   UpdateTexture(BYTE* pI420, int stride, bool flipVertically, bool flipHorizontally);
	HRESULT   AlphaBlendingIcon(_com_ptr_IDirect3DTexture9  pIcon, RECT destRC);
	HRESULT   DrawVideo(); // need to be called between a BeginScene/EndScene pair 

	HRESULT  UpdateTextureYUY2(BYTE* pYUY2, int stride);

private:
	void      _MakeQuadVertices(int w, int h, bool bFlipHorizontally, bool bFlipVertically, CustomVertex * pVertex);
	HRESULT   _CompileShader( _In_ LPCVOID  src, SIZE_T size, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob );
	HRESULT   _LoadTexture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int height);
private:
	_com_ptr_IDirect3DDevice9Ex     pd3dDevice_;
	_com_ptr_IDirect3DTexture9      pd3dTextureY_;
	_com_ptr_IDirect3DTexture9      pd3dTextureU_;
	_com_ptr_IDirect3DTexture9      pd3dTextureV_;
	_com_ptr_IDirect3DTexture9      pd3dTextureRGB32_;
	_com_ptr_IDirect3DVertexBuffer9 pd3dVertexBuffer_;
	_com_ptr_IDirect3DVertexBuffer9 pd3dVertexBuffer4Icon_;
	_com_ptr_IDirect3DPixelShader9  pPixelShader_;

	// for yuy2 conversion
	_com_ptr_IDirect3DTexture9      pd3dTextureYUY2_;
	_com_ptr_IDirect3DPixelShader9  pPixelShaderYUY2_;

	INT    nWidth_;
	INT    nHeight_;
	DWORD  nPixelFormat_;
	bool   bFlipHorizontally_;
	bool   bFlipVertically_;
};

}