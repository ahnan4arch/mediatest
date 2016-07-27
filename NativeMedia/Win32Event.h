//////////////////////////////////////////////////////////////////////////////
// This is an auto-reset event. Only one waiting thread will be released 
//  on the SetEvent call.
#pragma once
#include "stdafx.h"
namespace WinRTCSDK{

class  Win32Event {
public:
	Win32Event() { hEvent_ = ::CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE); }
	~Win32Event() { ::CloseHandle(hEvent_); }
	void Wait () { ::WaitForSingleObject(hEvent_, INFINITE) ; }
	bool Wait (int milliSec) {
		DWORD dwRet = ::WaitForSingleObject(hEvent_, milliSec) ; 
		return ( WAIT_OBJECT_0 == dwRet);
	}
	void SetEvent () { ::SetEvent(hEvent_) ; }
private:
	HANDLE hEvent_;
};

}