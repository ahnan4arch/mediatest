// A helper class to write wav files
#include "Stdafx.h"
#include <string>

/////////////////////////////////////////////////////////////////////////////////////////////
//  A wave file consists of:
//
//  RIFF header: 8 bytes consisting of the signature "RIFF"
//               followed by a 4 byte file length (RIFF header excluded).
//  WAVE header: 4 bytes consisting of the signature "WAVE".
//  fmt header:  4 bytes consisting of the signature "fmt " followed by a WAVEFORMATEX 
//  WAVEFORMAT:  <n> bytes containing a waveformat structure.
//  DATA header: 8 bytes consisting of the signature "data" followed by a 4 byte data length.
//  wave data:   <m> bytes containing wave data.
/////////////////////////////////////////////////////////////////////////////////////////////


namespace WinRTCSDK
{

//  Static RIFF header, we'll append the format to it.
static BYTE _WaveHeader[] = 
{
    'R',   'I',   'F',   'F',  
	0x00,  0x00,  0x00,  0x00, 
	'W',   'A',   'V',   'E',   
	'f',   'm',   't',   ' ', 
	0x00, 0x00, 0x00, 0x00
};

//  Static wave DATA tag and data size.
static BYTE _WaveDataHeader[] = 
{ 
	'd', 'a', 't', 'a',
	0x00,  0x00,  0x00,  0x00
};


class WavHelper 
{
	struct WAVEHEADER
	{
		DWORD   dwRiff;     // "RIFF"
		DWORD   dwSize;     // Size
		DWORD   dwWave;     // "WAVE"
		DWORD   dwFmt;      // "fmt "
		DWORD   dwFmtSize;  // Wave Format Size
	};
private:
	FILE  * _pFile;
	long    _filePosOfDataSize; // the file position to update the data size (on close)
	long    _dataSize; // the data size in bytes ( has been written )

public:
	WavHelper () 
	{
		_pFile = NULL;
		_filePosOfDataSize = 0;
		_dataSize = 0;
	}
	~WavHelper () 
	{
	}

public:
	bool CreateWavFile(std::wstring fnPrefix, const WAVEFORMATEX *pWaveFormat)
	{
		errno_t err;
		size_t ret;
		WAVEHEADER * pHeader = reinterpret_cast<WAVEHEADER *>(_WaveHeader);
	
		_filePosOfDataSize = 0;
		_dataSize = 0;
		err = _wfopen_s(&_pFile, fnPrefix.c_str(), L"wb");
		if ( err != 0) return false;

		// update the sizes in the header.
		pHeader->dwSize = 0; // this will be updated on close
		pHeader->dwFmtSize = sizeof(WAVEFORMATEX) + pWaveFormat->cbSize;

		// write the header
		ret = fwrite(pHeader, sizeof(WAVEHEADER), 1, _pFile);
		_filePosOfDataSize += sizeof(WAVEHEADER);

		// write the fmt
		ret = fwrite(pWaveFormat, sizeof(WAVEFORMATEX) + pWaveFormat->cbSize, 1, _pFile);
		_filePosOfDataSize += sizeof(WAVEFORMATEX) + pWaveFormat->cbSize;

		// then the data header.
		ret = fwrite(_WaveDataHeader, sizeof(_WaveDataHeader), 1, _pFile);
		_filePosOfDataSize += 4; // Now _filePosOfDataSize point to the DataSize field!
	
		// Now the file is ready to write data to
		return true;
	}

	bool WriteData( BYTE * pData, DWORD len)
	{
		if ( _pFile == NULL) return false;
		size_t ret = fwrite(pData, len, 1, _pFile);
		_dataSize += len;
		return (ret == 1); 
	}

	bool Close()
	{
		if ( _pFile == NULL) return false;
		// update the file size
		DWORD fileSize = _filePosOfDataSize + _dataSize + 4 - 8;
		fseek(_pFile, 4, SEEK_SET); // point to the FileSize field
		size_t ret = fwrite(&fileSize, sizeof(DWORD), 1, _pFile);
		// update the data size
		fseek(_pFile, _filePosOfDataSize, SEEK_SET); // point to the data size field
		ret = fwrite(&_dataSize, sizeof(DWORD), 1, _pFile);
		fclose (_pFile);
		return true;
	}
};

}// namespace WinRTCSDK