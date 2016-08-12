
#include "stdafx.h"

#include "decode_pipeline.h"
#include <sstream>

int _tmain_decode()
{
    sInputParams        Params;   // input parameters from command line
    CDecodingPipeline   Pipeline; // pipeline for decoding, includes input file reader, decoder and output file writer

    mfxStatus sts = MFX_ERR_NONE; // return value check


	Params.bUseHWLib = true;
	Params.videoType = MFX_CODEC_AVC;
	Params.nAsyncDepth = 4;

	sts = Pipeline.Init(&Params, NULL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    // print stream info
    Pipeline.PrintInfo();

    msdk_printf(MSDK_STRING("Decoding started\n"));

    for (;;)
    {
        sts = Pipeline.RunDecoding();

        if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
            {
                msdk_printf(MSDK_STRING("\nERROR: Incompatible video parameters detected. Recovering...\n"));
            }
            else
            {
                msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned unexpected error. Recovering...\n"));
                // sts = Pipeline.ResetDevice(&Params);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            }

            sts = Pipeline.ResetDecoder(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }

    msdk_printf(MSDK_STRING("\nDecoding finished\n"));

    return 0;
}
