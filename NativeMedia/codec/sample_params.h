/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#ifndef __SAMPLE_PARAMS_H__
#define __SAMPLE_PARAMS_H__

#include "sample_defs.h"
#include "plugin_utils.h"

struct sPluginParams
{
    mfxPluginUID      pluginGuid;
    mfxChar           strPluginPath[MSDK_MAX_FILENAME_LEN];
    MfxPluginLoadType type;
    sPluginParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
};

#endif //__SAMPLE_PARAMS_H__
