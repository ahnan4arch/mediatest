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
//        Copyright (c) 2011-2015 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_USER_H__
#define __PIPELINE_USER_H__

#include "vm/so_defs.h"
#include "pipeline_encode.h"
#include "mfx_plugin_base.h"
#include "mfx_plugin_module.h"
#include "rotate_plugin_api.h"

/* This class implements the following pipeline: user plugin (frame rotation) -> mfxENCODE */
class CUserPipeline : public CEncodingPipeline
{
public:

    CUserPipeline();
    virtual ~CUserPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);
    virtual void PrintInfo();

protected:
    msdk_so_handle          m_PluginModule;
    MFXGenericPlugin*       m_pusrPlugin;
    mfxFrameSurface1*       m_pPluginSurfaces; // frames array for rotate input
    mfxFrameAllocResponse   m_PluginResponse;  // memory allocation response for rotate plugin

    mfxVideoParam                   m_pluginVideoParams;
    RotateParam                     m_RotateParams;

    virtual mfxStatus InitRotateParam(sInputParams *pParams);
    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();
};


#endif // __PIPELINE_USER_H__
