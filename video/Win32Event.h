
#pragma once
#include "stdafx.h"
namespace WinRTCSDK{

class  Win32Event {
public:
	// default auto-reset
	Win32Event(BOOL manualRest=FALSE) { 
		manualRest_ = manualRest;
		hEvent_ = ::CreateEventEx(NULL, NULL, 
			manualRest?CREATE_EVENT_MANUAL_RESET:0
			, EVENT_MODIFY_STATE | SYNCHRONIZE); 
	}
	~Win32Event() { ::CloseHandle(hEvent_); }
	void Wait () { ::WaitForSingleObject(hEvent_, INFINITE) ; }
	bool Wait (int milliSec) {
		DWORD dwRet = ::WaitForSingleObject(hEvent_, milliSec) ; 
		return ( WAIT_OBJECT_0 == dwRet);
	}
	void SetEvent () { ::SetEvent(hEvent_) ; }
	void Reset(){::ResetEvent(hEvent_);}
	bool IsAutoRest(){ return manualRest_ == FALSE;}
private:
	BOOL   manualRest_;
	HANDLE hEvent_;
private: // no-copy
	Win32Event(const Win32Event&);
    void operator=(const Win32Event&);
};

class Win32Semaphore
{
public:
	Win32Semaphore(int count)
	{
		hSemphore_ = ::CreateSemaphore(NULL, count, LONG_MAX, 0);
	}

	~Win32Semaphore(void)
	{
		::CloseHandle(hSemphore_);
	}

	bool Post(void)
	{
		return (::ReleaseSemaphore(hSemphore_, 1, NULL) == TRUE);
	}

	bool Wait(void)
	{
		return (::WaitForSingleObject(hSemphore_, INFINITE) == WAIT_OBJECT_0) ;
	}
private:
	HANDLE  hSemphore_;
private:
	Win32Semaphore(const Win32Semaphore&);
	void operator=(const Win32Semaphore&);
};

}