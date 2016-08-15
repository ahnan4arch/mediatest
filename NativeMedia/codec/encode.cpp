
#include "stdafx.h"
#include <memory>
#include "encode_pipeline.h"
#include <stdarg.h>
#include <string>

#define VAL_CHECK(val, argIdx, argName) \
{ \
    if (val) \
    { \
        PrintHelp(NULL, MSDK_STRING("Input argument number %d \"%s\" require more parameters"), argIdx, argName); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}

void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage, ...)
{
    msdk_printf(MSDK_STRING("Encoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        va_list args;
        msdk_printf(MSDK_STRING("ERROR: "));
        va_start(args, strErrorMessage);
        msdk_vprintf(strErrorMessage, args);
        va_end(args);
        msdk_printf(MSDK_STRING("\n\n"));
    }

    msdk_printf(MSDK_STRING("Usage: %s <msdk-codecid> [<options>] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Supported codecs, <msdk-codecid>:\n"));
    msdk_printf(MSDK_STRING("   <codecid>=h264|mpeg2|vc1|mvc|jpeg - built-in Media SDK codecs\n"));
    msdk_printf(MSDK_STRING("   <codecid>=h265|vp8                - in-box Media SDK plugins (may require separate downloading and installation)\n"));
    msdk_printf(MSDK_STRING("   If codecid is jpeg, -q option is mandatory.)\n"));
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff] - input stream is interlaced, top|bottom fielf first, if not specified progressive is expected\n"));
    msdk_printf(MSDK_STRING("   [-bref] - arrange B frames in B pyramid reference structure\n"));
    msdk_printf(MSDK_STRING("   [-nobref] -  do not use B-pyramid (by default the decision is made by library)\n"));
    msdk_printf(MSDK_STRING("   [-idr_interval size] - idr interval, default 0 means every I is an IDR, 1 means every other I frame is an IDR etc\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (Kbits per second), valid for H.264, H.265, MPEG2 and MVC encoders \n"));
    msdk_printf(MSDK_STRING("   [-u speed|quality|balanced] - target usage, valid for H.264, H.265, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("   [-q quality] - mandatory quality parameter for JPEG encoder. In range [1,100]. 100 is the best quality. \n"));
    msdk_printf(MSDK_STRING("   [-r distance] - Distance between I- or P- key frames (1 means no B-frames) \n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (default 256)\n"));
    msdk_printf(MSDK_STRING("   [-x numRefs]   - number of reference frames\n"));
    msdk_printf(MSDK_STRING("   [-la] - use the look ahead bitrate control algorithm (LA BRC) (by default constant bitrate control method is used)\n"));
    msdk_printf(MSDK_STRING("           for H.264, H.265 encoder. Supported only with -hw option on 4th Generation Intel Core processors. \n"));
    msdk_printf(MSDK_STRING("   [-lad depth] - depth parameter for the LA BRC, the number of frames to be analyzed before encoding. In range [10,100].\n"));
    msdk_printf(MSDK_STRING("            may be 1 in the case when -mss option is specified \n"));
    msdk_printf(MSDK_STRING("   [-dstw width] - destination picture width, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-dsth height] - destination picture height, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-hw] - use platform specific SDK implementation (default)\n"));
    msdk_printf(MSDK_STRING("   [-sw] - use software implementation, if not specified platform specific SDK implementation is used\n"));
    msdk_printf(MSDK_STRING("   [-p guid] - 32-character hexadecimal guid string\n"));
    msdk_printf(MSDK_STRING("                              (optional for Media SDK in-box plugins, required for user-encoder ones)\n"));
    msdk_printf(MSDK_STRING("   [-path path] - path to plugin (valid only in pair with -p option)\n"));
    msdk_printf(MSDK_STRING("   [-async]                 - depth of asynchronous pipeline. default value is 4. must be between 1 and 20.\n"));
    msdk_printf(MSDK_STRING("   [-no_gpu_copy]           - disable GPU Copy functionality\n"));
    msdk_printf(MSDK_STRING("   [-cqp]                   - constant quantization parameter (CQP BRC) bitrate control method\n"));
    msdk_printf(MSDK_STRING("                              (by default constant bitrate control method is used), should be used along with -qpi, -qpp, -qpb.\n"));
    msdk_printf(MSDK_STRING("   [-qpi]                   - constant quantizer for I frames (if bitrace control method is CQP). In range [1,51]. 0 by default, i.e.no limitations on QP.\n"));
    msdk_printf(MSDK_STRING("   [-qpp]                   - constant quantizer for P frames (if bitrace control method is CQP). In range [1,51]. 0 by default, i.e.no limitations on QP.\n"));
    msdk_printf(MSDK_STRING("   [-qpb]                   - constant quantizer for B frames (if bitrace control method is CQP). In range [1,51]. 0 by default, i.e.no limitations on QP.\n"));
    msdk_printf(MSDK_STRING("   [-num_slice]             - number of slices in each video frame. 0 by default.\n"));
    msdk_printf(MSDK_STRING("                              If num_slice equals zero, the encoder may choose any slice partitioning allowed by the codec standard.\n"));
    msdk_printf(MSDK_STRING("   [-mss]                   - maximum slice size in bytes. Supported only with -hw and h264 codec. This option is not compatible with -num_slice option.\n"));
    msdk_printf(MSDK_STRING("   [-re]                    - enable region encode mode. Works only with h265 encoder\n"));
    msdk_printf(MSDK_STRING("Example: %s h265 -i InputYUVFile -o OutputEncodedFile -w width -h height -hw -p 2fca99749fdb49aeb121a5b63ef568f7\n"), strAppName);

	msdk_printf(MSDK_STRING("   [-d3d] - work with d3d surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11] - work with d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|h265|mpeg2|jpeg -i InputYUVFile -o OutputEncodedFile -w width -h height -d3d -hw \n"), strAppName);
    msdk_printf(MSDK_STRING("Example for MVC: %s mvc -i InputYUVFile_1 -i InputYUVFile_2 -o OutputEncodedFile -w width -h height \n"), strAppName);


	msdk_printf(MSDK_STRING("   [-viewoutput] - instruct the MVC encoder to output each view in separate bitstream buffer. Depending on the number of -o options behaves as follows:\n"));
    msdk_printf(MSDK_STRING("                   1: two views are encoded in single file\n"));
    msdk_printf(MSDK_STRING("                   2: two views are encoded in separate files\n"));
    msdk_printf(MSDK_STRING("                   3: behaves like 2 -o opitons was used and then one -o\n\n"));
    msdk_printf(MSDK_STRING("Example: %s mvc -i InputYUVFile_1 -i InputYUVFile_2 -o OutputEncodedFile_1 -o OutputEncodedFile_2 -viewoutput -w width -h height \n"), strAppName);
    // user module options
    msdk_printf(MSDK_STRING("User module options: \n"));
    msdk_printf(MSDK_STRING("   [-angle 180] - enables 180 degrees picture rotation before encoding, CPU implementation by default. Rotation requires NV12 input. Options -tff|bff, -dstw, -dsth, -d3d are not effective together with this one, -nv12 is required.\n"));
    msdk_printf(MSDK_STRING("   [-opencl] - rotation implementation through OPENCL\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|h265|mpeg2|mvc|jpeg -i InputYUVFile -o OutputEncodedFile -w width -h height -angle 180 -opencl \n"), strAppName);

    msdk_printf(MSDK_STRING("\n"));
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, EncodeParams* pParams)
{

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    // default implementation
    pParams->bUseHWLib = true;

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        if (MSDK_CHAR('-') != strInput[i][0])
        {
            mfxStatus sts = StrFormatToCodecFormatFourCC(strInput[i], pParams->CodecId);
            if (sts != MFX_ERR_NONE)
            {
                PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
                return MFX_ERR_UNSUPPORTED;
            }
            if (!IsEncodeCodecSupported(pParams->CodecId))
            {
                PrintHelp(strInput[0], MSDK_STRING("Unsupported codec"));
                return MFX_ERR_UNSUPPORTED;
            }
            continue;
        }

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nDstWidth))
            {
                PrintHelp(strInput[0], MSDK_STRING("Destination picture Width is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nDstHeight))
            {
                PrintHelp(strInput[0], MSDK_STRING("Destination picture Height is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sw")))
        {
            pParams->bUseHWLib = false;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            pParams->bUseHWLib = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bref")))
        {
            pParams->nBRefType = MFX_B_REF_PYRAMID;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nobref")))
        {
            pParams->nBRefType = MFX_B_REF_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-idr_interval")))
        {
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nIdrInterval))
            {
                PrintHelp(strInput[0], MSDK_STRING("IdrInterval is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-la")))
        {
            pParams->nRateControlMethod = MFX_RATECONTROL_LA;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-lad")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            pParams->nRateControlMethod = MFX_RATECONTROL_LA;
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nLADepth))
            {
                PrintHelp(strInput[0], MSDK_STRING("Look Ahead Depth is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mss")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nMaxSliceSize))
            {
                PrintHelp(strInput[0], MSDK_STRING("MaxSliceSize is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            pParams->memType = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-async")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);

            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nAsyncDepth))
            {
                PrintHelp(strInput[0], MSDK_STRING("Async Depth is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-no_gpu_copy")))
        {
            pParams->gpuCopy = MFX_GPUCOPY_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-cqp")))
        {
            pParams->nRateControlMethod = MFX_RATECONTROL_CQP;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qpi")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nQPI))
            {
                PrintHelp(strInput[0], MSDK_STRING("Quantizer for I frames is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qpp")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nQPP))
            {
                PrintHelp(strInput[0], MSDK_STRING("Quantizer for P frames is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qpb")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nQPB))
            {
                PrintHelp(strInput[0], MSDK_STRING("Quantizer for B frames is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-num_slice")))
        {
            VAL_CHECK(i+1 >= nArgNum, i, strInput[i]);
            if (MFX_ERR_NONE != msdk_opt_read(strInput[++i], pParams->nNumSlice))
            {
                PrintHelp(strInput[0], MSDK_STRING("Number of slices is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        } 
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('u'):
                if (++i < nArgNum) {
                    pParams->nTargetUsage = StrToTargetUsage(strInput[i]);
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-u' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('w'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nWidth))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Width is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-w' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('h'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nHeight))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Height is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-h' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('f'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->dFrameRate))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Frame Rate is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-f' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('b'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nBitRate))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Bit Rate is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-b' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('x'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nNumRefFrame))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Ref Num is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-x' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('g'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nGopPicSize))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Gop Size is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-g' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('r'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nGopRefDist))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Ref Dist is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-r' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('i'):
                if (++i < nArgNum) {
                    msdk_opt_read(strInput[i], pParams->strSrcFile);
                    pParams->srcFileBuff.push_back(strInput[i]);
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-i' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('o'):
                if (++i < nArgNum) {
                    pParams->dstFileBuff.push_back(strInput[i]);
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-o' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('q'):
                if (++i < nArgNum) {
                    if (MFX_ERR_NONE != msdk_opt_read(strInput[i], pParams->nQuality))
                    {
                        PrintHelp(strInput[0], MSDK_STRING("Quality is invalid"));
                        return MFX_ERR_UNSUPPORTED;
                    }
                }
                else {
                    msdk_printf(MSDK_STRING("error: option '-q' expects an argument\n"));
                }
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                PrintHelp(strInput[0], MSDK_STRING("Unknown options"));
            }
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (pParams->dstFileBuff.empty())
    {
        PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_CODEC_MPEG2 != pParams->CodecId &&
        MFX_CODEC_AVC != pParams->CodecId &&
        MFX_CODEC_JPEG != pParams->CodecId &&
        MFX_CODEC_VP8 != pParams->CodecId &&
        MFX_CODEC_HEVC != pParams->CodecId)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
        return MFX_ERR_UNSUPPORTED;
    }


    if (pParams->nQuality && (MFX_CODEC_JPEG != pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-q option is supported only for JPEG encoder"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nTargetUsage || pParams->nBitRate) && (MFX_CODEC_JPEG == pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-u and -b options are supported only for H.264, MPEG2 and MVC encoders. For JPEG encoder use -q"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pParams->nDstWidth == 0)
    {
        pParams->nDstWidth = pParams->nWidth;
    }

    if (pParams->nDstHeight == 0)
    {
        pParams->nDstHeight = pParams->nHeight;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
            pParams->nDstHeight, pParams->dFrameRate);
    }

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pParams->ColorFormat)
    {
        pParams->ColorFormat = MFX_FOURCC_YV12;
    }

    if (!pParams->nPicStruct)
    {
        pParams->nPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if ((pParams->nRateControlMethod == MFX_RATECONTROL_LA) && (!pParams->bUseHWLib))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nMaxSliceSize) && (!pParams->bUseHWLib))
    {
        PrintHelp(strInput[0], MSDK_STRING("MaxSliceSize option is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nMaxSliceSize) && (pParams->nNumSlice))
    {
        PrintHelp(strInput[0], MSDK_STRING("-mss and -num_slice options are not compatible!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nRateControlMethod == MFX_RATECONTROL_LA) && (pParams->CodecId != MFX_CODEC_AVC))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nMaxSliceSize) && (pParams->CodecId != MFX_CODEC_AVC))
    {
        PrintHelp(strInput[0], MSDK_STRING("MaxSliceSize option is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->nLADepth && (pParams->nLADepth < 10 || pParams->nLADepth > 100))
    {
        if ((pParams->nLADepth != 1) || (!pParams->nMaxSliceSize))
        {
            PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100] or 1 in case of -mss option!"));
            return MFX_ERR_UNSUPPORTED;
        }
    }


    if (pParams->nAsyncDepth == 0)
    {
        pParams->nAsyncDepth = 4; //set by default;
    }

    // Ignoring user-defined Async Depth for LA
    if (pParams->nMaxSliceSize)
    {
        pParams->nAsyncDepth = 1;
    }

    if (pParams->nRateControlMethod == 0)
    {
        pParams->nRateControlMethod = MFX_RATECONTROL_CBR;
    }

    return MFX_ERR_NONE;
}

CEncodingPipeline* CreatePipeline(const EncodeParams& params)
{

    {
        return new CEncodingPipeline;
    }
}

int _tmain_encode(int argc, msdk_char *argv[])
{
    EncodeParams Params = {};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    // Choosing which pipeline to use
    pPipeline.reset(CreatePipeline(Params));

    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);


    sts = pPipeline->Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

            sts = pPipeline->ResetMFXComponents(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }

    pPipeline->Close();

    msdk_printf(MSDK_STRING("\nProcessing finished\n"));

    return 0;
}
