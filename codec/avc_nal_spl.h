#pragma once
#include <vector>
#include "mfxstructures.h"

namespace ProtectedLibrary
{

class BytesSwapper
{
public:
    static void SwapMemory(mfxU8 *pDestination, mfxU32 &nDstSize, mfxU8 *pSource, mfxU32 nSrcSize);
};


class StartCodeIterator
{
public:

    StartCodeIterator();

    void Reset();

    mfxI32 Init(mfxBitstream * source);

    void SetSuggestedSize(mfxU32 size);

    mfxI32 CheckNalUnitType(mfxBitstream * source);

    mfxI32 GetNALUnit(mfxBitstream * source, mfxBitstream * destination);

    mfxI32 EndOfStream(mfxBitstream * destination);

private:
    std::vector<mfxU8>  m_prev;
    mfxU32   m_code;
    mfxU64   m_pts;

    mfxU8 * m_pSource;
    mfxU32  m_nSourceSize;

    mfxU8 * m_pSourceBase;
    mfxU32  m_nSourceBaseSize;

    mfxU32  m_suggestedSize;

    mfxI32 FindStartCode(mfxU8 * (&pb), mfxU32 & size, mfxI32 & startCodeSize);
};

class NALUnitSplitter
{
public:

    NALUnitSplitter();

    virtual ~NALUnitSplitter();

    virtual void Init();
    virtual void Release();

    virtual mfxI32 CheckNalUnitType(mfxBitstream * source);
    virtual mfxI32 GetNalUnits(mfxBitstream * source, mfxBitstream * &destination);

    virtual void Reset();

    virtual void SetSuggestedSize(mfxU32 size)
    {
        m_pStartCodeIter.SetSuggestedSize(size);
    }

protected:

    StartCodeIterator m_pStartCodeIter;

    mfxBitstream m_bitstream;
};

void SwapMemoryAndRemovePreventingBytes(mfxU8 *pDestination, mfxU32 &nDstSize, mfxU8 *pSource, mfxU32 nSrcSize);

} //namespace ProtectedLibrary

