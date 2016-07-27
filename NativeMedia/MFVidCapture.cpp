#include "stdafx.h"
// stl headers
#include <algorithm>
// local headers
#include "MFVidCapture.h"
#include "NativeMediaLog.h"

namespace WinRTCSDK{

MFVidCapture::MFVidCapture(IVideoCaptureCallback *callback):
	ppresentationDescriptor_(NULL),
	nRefCount_(1),
	callback_(callback),
	pReader_ (NULL),
	symbolicLink_ (),
	lock_(),
	frameWidth_(0),
	frameHeight_(0),
	defaultStride_(0),
	format_(GUID_NULL),
	dispatcher_("CameraReader", true),
	cameraRunningFlag_(false),
	asyncMode_(false),
	cameraControl_()
{
	OSVERSIONINFO ver;
	ver.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	::GetVersionEx(&ver);
	if (ver.dwMajorVersion ==6 && ver.dwMinorVersion<2)//Win7
	{
		// we use async mode on windows 7 only
		// asyncMode_ = true;
		isWindows7_ = true;
		NativeMediaLogger::log(LogLevel::Info, "Camera", "Windows7 using async mode");
	}
}

MFVidCapture::~MFVidCapture(void)
{
}
	
VOID MFVidCapture::SetD3DDevManager( _com_ptr_IDirect3DDeviceManager9 pD3D9Mgr)
{
	pD3DManager_ = pD3D9Mgr;
}

std::vector<MediaDevInfo> MFVidCapture::GetDevList()
{
	_UpdateDevList();
	return devList_;
}

HRESULT MFVidCapture::OpenCamera(std::wstring symbolicLink)
{
	AutoLock l (lock_);
	if ( cameraRunningFlag_ == true) return S_OK;

	_UpdateDevList();

	// check if the specified device exists.
	auto result = std::find_if(devList_.begin(), devList_.end(), 
		[=](MediaDevInfo &info)->bool{ return info.symbolicLink == symbolicLink;} );
	if ( result == devList_.end() ) return -1;

	symbolicLink_ = symbolicLink; //set current device ID;

	HRESULT hr = _CreateSourceReader();
	if (FAILED(hr)) { return hr; }

	// setup output format
	_SetFormat();

	cameraRunningFlag_ = true;
	dispatcher_.startUp(); // ensure dispatcher thread
	
	if ( asyncMode_ == false ){
		LooperTask t = [=] () { _CameraRunner(); };
		dispatcher_.runAsyncTask(t);
	}else{
		// Async read the first frame
		pReader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,NULL, NULL, NULL, NULL);
	}
	// also create the camera control
	cameraControl_.Create(pSource_);
	return S_OK;
}

HRESULT MFVidCapture::CloseCamera()
{
	AutoLock l (lock_);

	if ( cameraRunningFlag_ == false) return S_OK;

	// shut down internal thread first!
	cameraRunningFlag_ = false;
	dispatcher_.stopAndJoin();

	// destroy the reader
	cameraControl_.Destroy();
	_DestroySourceReader();
	symbolicLink_ = L"";
	return S_OK;
}

// This loop is for synchronous reading mode
HRESULT MFVidCapture::_CameraRunner()
{
	HRESULT hr ;
	DWORD streamIndex, streamFlags;
	LONGLONG llTimeStamp;
	DWORD timestampBeforeCap, timestampAfterCap; 
	_com_ptr_IMFSample pSample;
	const int FRAME_INTERVAL = 30;
	// use MMCSS
	HANDLE mmcssHandle = NULL;
	DWORD mmcssTaskIndex = 0;
	mmcssHandle = ::AvSetMmThreadCharacteristics(L"Capture", &mmcssTaskIndex);
	if (mmcssHandle != NULL){
		BOOL ret = ::AvSetMmThreadPriority(mmcssHandle,AVRT_PRIORITY_HIGH );
	}

	while ( cameraRunningFlag_) 
	{
	    hr = S_OK;
		timestampBeforeCap = ::GetTickCount();

	    hr = pReader_->ReadSample( (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
			&streamIndex,  &streamFlags,  &llTimeStamp,  &pSample);

		if (FAILED(hr)) 
		{
			::Sleep(FRAME_INTERVAL);
			continue;
		}
		
		hr = _HandleCameraSample(S_OK, streamIndex, streamFlags, llTimeStamp, pSample);
		
		timestampAfterCap = ::GetTickCount();
		DWORD duration = timestampAfterCap-timestampBeforeCap;
		if ( duration < FRAME_INTERVAL) ::Sleep( FRAME_INTERVAL - (duration));
	}

	if ( mmcssHandle){
		::AvRevertMmThreadCharacteristics(mmcssHandle);
	}

	return hr;
}
	
HRESULT MFVidCapture::_HandleCameraSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
						LONGLONG llTimestamp, IMFSample * pSample)
{
	HRESULT hr;

	if ( FAILED(hrStatus)) return hrStatus;

	if ( cameraRunningFlag_  && asyncMode_){	// Async read the next frame
		pReader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,NULL, NULL, NULL, NULL);
	}

	if (pSample) 
	{
		_com_ptr_IMFMediaBuffer pBuffer = NULL;
		// Get the video frame buffer from the sample.
		hr = pSample->GetBufferByIndex(0, &pBuffer);
		// output the frame.
		if (SUCCEEDED(hr))   
		{
			VideoFrameInfo frame;
				
			frame.bMirroring = TRUE;
			MediaBufferLock bufferLock(pBuffer);
			_com_ptr_IDirect3DSurface9 pSurface = bufferLock.GetD3DSurface();
			if ( pSurface != NULL )
			{
				frame.pSample = pSurface;
			}
			else
			{
				bufferLock.LockBuffer(defaultStride_, &frame.image, (LONG*)&frame.stride);
				frame.width = frameWidth_;
				frame.height = frameHeight_;
				if ( format_ == MFVideoFormat_I420){
					frame.format = D3DFMT_I420;
				}else if ( format_ == MFVideoFormat_YUY2){
					frame.format = D3DFMT_YUY2; 
				}
			}

			callback_->OnVideoCaptureData(frame);
		}
	}

	return S_OK;
}

HRESULT MFVidCapture::_UpdateDevList ()
{
	devList_.clear();

	IMFActivate  **ppDevices;
    UINT32  devCount;

    _com_ptr_IMFAttributes pAttributes = NULL;
    // Create an attribute store to specify the enumeration parameters.
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

	// Source type: video capture devices
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)){ return hr; }

    // Enumerate devices.
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &devCount);
	if ( devCount<1) return -1; // no capture dev

	for (int i=0; i<(int)devCount;i++)
	{
		MediaDevInfo info;
		WCHAR buff [4096];
		// friendly name
		hr = ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, buff, 4096, NULL);
		if (SUCCEEDED(hr))info.devFriendlyName = buff;
		// symbolic link
		hr = ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, buff, 4096, NULL);
		if (SUCCEEDED(hr))info.symbolicLink = buff;
		info.devType = MediaDevType::eVideoCapture;
		devList_.push_back(info);
	}

	for (DWORD i = 0; i < devCount; i++)
    {
        SafeRelease(&ppDevices[i]);
    }
    CoTaskMemFree(ppDevices);
	ppDevices = NULL;
	devCount = 0;
	return hr;
}

HRESULT MFVidCapture::_DestroySourceReader ( )
{
	HRESULT hr;
	ppresentationDescriptor_ = NULL; // the comp_ptr will release the object
	if (pSource_)
	{
		hr = pSource_->Stop();
		hr = pSource_->Shutdown();
		pSource_ = NULL;
	}
	pReader_ = NULL;
    
    return S_OK;
}

HRESULT MFVidCapture::_CreateVideoCaptureDevice(IMFMediaSource **ppSource)
{
    *ppSource = NULL;
    _com_ptr_IMFAttributes pAttributes = NULL;

    HRESULT hr = MFCreateAttributes(&pAttributes, 3);
    if (FAILED(hr)) goto done;

    // Set the device type to video.    
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) goto done;
    
    // Set the symbolic link.
    hr = pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            (LPCWSTR)symbolicLink_.c_str());            
    if (FAILED(hr)) goto done;

	// Set the max buffers
	hr = pAttributes->SetUINT32(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_MAX_BUFFERS, 1);
    if (FAILED(hr)) goto done;
        
	hr = ::MFCreateDeviceSource(pAttributes, ppSource);

done:
    return hr;    
}


HRESULT MFVidCapture::_CreateSourceReader ()
{
	HRESULT  hr = S_OK;

    pSource_ = NULL;
    _com_ptr_IMFAttributes   pAttributes = NULL;

    AutoLock l (lock_);

	hr= _CreateVideoCaptureDevice(&pSource_);
    if (FAILED(hr)) return hr;

    // Create an attribute store to hold initialization settings.
	int n = 4;
	bool bDXVA = false;
	if ( asyncMode_ == false ){
		n--;
	}

	if ( bDXVA == false ){
		n--;
	}

	hr = MFCreateAttributes(&pAttributes, n);
    if (FAILED(hr)) return hr;

	// enable async mode
	if ( asyncMode_ == true ){
		hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK , this);
		if (FAILED(hr)) return hr;
	}

	// enable hardware decoder
	hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS , TRUE);
    if (FAILED(hr)) return hr;
	
	// enable DXVA
	if ( pD3DManager_&&bDXVA ){
		hr = pAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, (IUnknown*)pD3DManager_);
	    if (FAILED(hr)) return hr;
	} 

	// enable low latency
	hr = pAttributes->SetUINT32(MF_LOW_LATENCY , TRUE);
	if (FAILED(hr)) return hr;

    hr = ::MFCreateSourceReaderFromMediaSource( pSource_, pAttributes, &pReader_ );
    if (FAILED(hr)) return hr;

	hr = pSource_->CreatePresentationDescriptor(&ppresentationDescriptor_);
    if (FAILED(hr)) return hr;

    if (FAILED(hr)) {
		return hr;
    }

    return hr;
}

std::string MFVidCapture::_GetFmtName( GUID fmt)
{
	static struct { GUID guid;  std::string name; } fmtList [] = 
	{
		MFVideoFormat_RGB32,"MFVideoFormat_RGB32",
		MFVideoFormat_ARGB32,"MFVideoFormat_ARGB32",
		MFVideoFormat_RGB24,"MFVideoFormat_RGB24",
		MFVideoFormat_RGB555,"MFVideoFormat_RGB555",
		MFVideoFormat_RGB565,"MFVideoFormat_RGB565",
		MFVideoFormat_RGB8, "MFVideoFormat_RGB8", 
		MFVideoFormat_AI44, "MFVideoFormat_AI44", 
		MFVideoFormat_AYUV, "MFVideoFormat_AYUV", 
		MFVideoFormat_YUY2, "MFVideoFormat_YUY2", 
		MFVideoFormat_YVYU, "MFVideoFormat_YVYU", 
		MFVideoFormat_YVU9, "MFVideoFormat_YVU9", 
		MFVideoFormat_UYVY, "MFVideoFormat_UYVY", 
		MFVideoFormat_NV11, "MFVideoFormat_NV11", 
		MFVideoFormat_NV12, "MFVideoFormat_NV12", 
		MFVideoFormat_YV12, "MFVideoFormat_YV12", 
		MFVideoFormat_I420, "MFVideoFormat_I420", 
		MFVideoFormat_IYUV, "MFVideoFormat_IYUV", 
		MFVideoFormat_DVC,  "MFVideoFormat_DVC",  
		MFVideoFormat_H264, "MFVideoFormat_H264", 
		MFVideoFormat_MJPG, "MFVideoFormat_MJPG", 
		MFVideoFormat_420O, "MFVideoFormat_420O", 
		GUID_NULL, ""
	};

	for(int i=0; fmtList [i].guid != GUID_NULL ;i++)
	{
		if ( fmtList [i].guid == fmt ) return fmtList [i].name;
	}
	return "";
}
	
HRESULT MFVidCapture::_GetFormatFromMediaType( VideoCaptureFormat& fmt, _com_ptr_IMFMediaType type)
{
	HRESULT hr = type->GetGUID(MF_MT_SUBTYPE, &fmt.type);
	fmt.name = _GetFmtName(fmt.type);
	hr = MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &fmt.fsNumerator, &fmt.fsDenominator );
	hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &fmt.width, &fmt.height);
	return hr;
}

static float __FS(VideoCaptureFormat&fmt) {
	if ( fmt.fsDenominator ==0) return 0.0;
	return (fmt.fsNumerator*1.0f )/(fmt.fsDenominator*1.0f );
}

HRESULT MFVidCapture::_SetFormat ()
{
	int i;
	HRESULT hr;
    GUID majorType;

	if (pReader_ == NULL) return E_INVALIDARG;

    _com_ptr_IMFMediaType pNativeType = NULL;
    _com_ptr_IMFMediaType pUsedType = NULL;

	std::vector<VideoCaptureFormat> mjpgFormats;
	std::vector<VideoCaptureFormat> h264Formats;
	std::vector<VideoCaptureFormat> yuy2Formats;
	std::vector<VideoCaptureFormat> i420Formats;

	VideoCaptureFormat fmt;
    // Find the native formats of the stream.
	for (i=0; /* keep trying get formats until failed*/ ;i++)
	{
		// the GetNativeMediaType method returns a copy of underling mediatype
		// so we can modify it safely
		hr = pReader_->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pNativeType);
		if ( SUCCEEDED(hr))
		{
		    hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
			if ( majorType != MFMediaType_Video) continue; // care only video type

			_GetFormatFromMediaType(fmt, pNativeType);

			NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Native VideoFmt: %s, resolution=%dx%d, fs=%d/%d\n",
				fmt.name.c_str(), fmt.width, fmt.height, fmt.fsNumerator, fmt.fsDenominator);

			// compressed format
			if (fmt.type == MFVideoFormat_H264 ) h264Formats.push_back(fmt);
			if (fmt.type == MFVideoFormat_MJPG ) mjpgFormats.push_back(fmt);
			// uncompressed format
			if (fmt.type == MFVideoFormat_YUY2 ) yuy2Formats.push_back(fmt);
			if (fmt.type == MFVideoFormat_I420  ) i420Formats.push_back(fmt);
		}
		else
		{
			break;
		}
	}
	
	// compare resolution and frame speed
	auto comp = [](VideoCaptureFormat& l, VideoCaptureFormat& r)->bool
	{
		// frame speed greater than 30, we will put resolution at priority 
		float lcap = __FS(l) * l.width * l.height;
		float rcap = __FS(r) * r.width * r.height;

		if( lcap > rcap ){
			return true;
		}else if ( ::abs(lcap - rcap ) < 0.1f) { // same cap
			if ( l.width > r.width && l.height > r.height ){
				return true;
			}
		}
		return false;
	};

	// sort the uncompressed types
	std::sort(yuy2Formats.begin(), yuy2Formats.end(), comp);
	std::sort(i420Formats.begin(), i420Formats.end(), comp);

	// sort the compressed formats
	std::sort(h264Formats.begin(), h264Formats.end(), comp);
	std::sort(mjpgFormats.begin(), mjpgFormats.end(), comp);

    // Define the output type.
    hr = MFCreateMediaType(&pUsedType);
    if (FAILED(hr)) { goto done; }

	// alway try native uncompressed types only.
	UINT32 maxWidth = 1920;
	UINT32 maxHeight = 1080;
	if ( isWindows7_ ){
		maxWidth = 1280;
		maxHeight = 720;
	}

	// compressed formats
	if (yuy2Formats.size()> 0 && yuy2Formats[0].width >= 1280 && __FS(yuy2Formats[0])>29.0 ){
		fmt = yuy2Formats[0];
	}else if (i420Formats.size()> 0 && i420Formats[0].width >= 1280 && __FS(i420Formats[0])>29.0 ){
		fmt = i420Formats[0];
	}else if ( h264Formats.size()>0 ) {
		fmt = h264Formats[0];
		fmt.type = MFVideoFormat_I420;
		NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Decode H264 to I420");
	}else if ( mjpgFormats.size() > 0){
		fmt = mjpgFormats[0];
		fmt.type = MFVideoFormat_YUY2;
		NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Decode MJPG to YUY2");
	}else {
		NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Camera dosn't suport 720p");
		if (yuy2Formats.size()>0){ // all cameras should support at lease yuy2 formats
			fmt = yuy2Formats[0];
		}
	}

	// always use 16:9 
	if ( fmt.width >= maxWidth ){
		fmt.width = maxWidth;
		fmt.height = maxHeight;
	}else if (fmt.width >= 1280){
		fmt.width = 1280;
		fmt.height = 720;
	}else{
		fmt.width = 640;
		fmt.height = 360;
	}

	NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Try VideoFmt: %s, resolution=%dx%d, fs=%d/%d\n",
		fmt.name.c_str(), fmt.width, fmt.height, fmt.fsNumerator, fmt.fsDenominator);

    hr = pUsedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	hr = MFSetAttributeRatio(pUsedType, MF_MT_FRAME_RATE, fmt.fsNumerator, fmt.fsDenominator);
	hr = MFSetAttributeSize(pUsedType, MF_MT_FRAME_SIZE, fmt.width, fmt.height);
	hr = pUsedType->SetGUID(MF_MT_SUBTYPE, fmt.type);
	hr = pReader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pUsedType);
	
	if (FAILED(hr)) {
		NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Try lowest capability\n");
		// try lowest resolution
		hr = MFSetAttributeRatio(pUsedType, MF_MT_FRAME_RATE, 30, 1);
		hr = MFSetAttributeSize(pUsedType, MF_MT_FRAME_SIZE, 640, 360);
		hr = pUsedType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
		hr = pReader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pUsedType);
	}

done:

	hr = pReader_->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pUsedType);
	_GetFormatFromMediaType (fmt, pUsedType);

	NativeMediaLogger4CPP::log(LogLevel::Info, "Camera", "Final format: %s, resolution=%dx%d, fs=%d/%d\n",
		fmt.name.c_str(), fmt.width, fmt.height, fmt.fsNumerator, fmt.fsDenominator);
	
	format_ = fmt.type;
	frameWidth_ = fmt.width;
	frameHeight_ = fmt.height;
	maxFSNumerator_= fmt.fsNumerator;
	maxFSDenominator_= fmt.fsDenominator;

	if (SUCCEEDED(hr)){
		_GetDefaultStride(pUsedType, &defaultStride_);
	}
    return hr;
}

// Gets the default stride for a video frame, assuming no extra padding bytes.
HRESULT MFVidCapture::_GetDefaultStride(_com_ptr_IMFMediaType pType, LONG *plStride)
{
    LONG lStride = 0;

    // Try to get the default stride from the media type.
    HRESULT hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if (FAILED(hr))
    {
        // Attribute not set. Try to calculate the default stride.
        GUID subtype = GUID_NULL;

        UINT32 width = 0;
        UINT32 height = 0;

        // Get the subtype and the image size.
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (SUCCEEDED(hr))
        {
            hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &lStride);
        }

        // Set the attribute for later reference.
        if (SUCCEEDED(hr))
        {
            (void)pType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(lStride));
        }
    }

    if (SUCCEEDED(hr))
    {
        *plStride = lStride;
    }
    return hr;
}

// compare the removed device id with current device id, 
// to check if our camera disconnected. 
HRESULT MFVidCapture::CheckDeviceLost(std::wstring removedId, BOOL *pbDeviceLost)
{
    if (pbDeviceLost == NULL)
    {
        return E_POINTER;
    }

    *pbDeviceLost = FALSE;

	if (removedId.size()==0)
    {
        return S_OK;
    }

	AutoLock l (lock_);
    if (symbolicLink_ == removedId)
    {
        *pbDeviceLost = TRUE;
    }

    return S_OK;
}

} // namespace WinRTCSDK