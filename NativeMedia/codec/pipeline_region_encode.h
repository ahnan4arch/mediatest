//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//  This sample was distributed or derived from the Intel's Media Samples package.
//  The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
//  or https://software.intel.com/en-us/media-client-solutions-support.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_REGION_ENCODE_H__
#define __PIPELINE_REGION_ENCODE_H__

#include "pipeline_encode.h"

class CMSDKResource
{
public:
    CMSDKResource()
    {
        pEncoder=NULL;
        pPlugin=NULL;
    }

    MFXVideoSession Session;
    MFXVideoENCODE* pEncoder;
    MFXPlugin* pPlugin;
    CEncTaskPool TaskPool;
};

class CResourcesPool
{
public:
    CResourcesPool()
    {
        size=0;
        m_resources=NULL;
    }

    ~CResourcesPool()
    {
        delete[] m_resources;
    }

    CMSDKResource& operator[](int index){return m_resources[index];}

    int GetSize(){return size;}

    mfxStatus Init(int size,mfxIMPL impl, mfxVersion *pVer);
    mfxStatus InitTaskPools(CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL);
    mfxStatus CreateEncoders();
    mfxStatus CreatePlugins(mfxPluginUID pluginGUID, mfxChar* pluginPath);

    mfxStatus GetFreeTask(int resourceNum, sTask **ppTask);
    void CloseAndDeleteEverything();

protected:
    CMSDKResource* m_resources;
    int size;

private:
    CResourcesPool(const CResourcesPool& src){(void)src;}
    CResourcesPool& operator= (const CResourcesPool& src){(void)src;return *this;}
};



/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CRegionEncodingPipeline : public CEncodingPipeline
{
public:
    CRegionEncodingPipeline();
    virtual ~CRegionEncodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);

    void SetMultiView();
    void SetNumView(mfxU32 numViews) { m_nNumView = numViews; }

protected:
    mfxI64 m_timeAll;
    CResourcesPool m_resources;

    mfxExtHEVCRegion m_HEVCRegion;

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);

    virtual mfxStatus CreateAllocator();

    virtual MFXVideoSession& GetFirstSession(){return m_resources[0].Session;}
    virtual MFXVideoENCODE* GetFirstEncoder(){return m_resources[0].pEncoder;}
};

#endif // __PIPELINE_REGION_ENCODE_H__
