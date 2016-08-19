#pragma once
#include <string>
#include <stdarg.h>


#pragma warning( disable: 4793) 
namespace WinRTCSDK
{
enum LogLevel{
	Error=0, Warning, Info, Debug
};

class NativeMediaLogger4CPP
{
public:
	static void log (int level, std::string tag, const char * fmt, ...)
	{
		va_list args;
		int     len;
		char    buffer[1024];

		// retrieve the variable arguments
		va_start( args, fmt );

		len = _vscprintf( fmt, args ) + 1;  // _vscprintf doesn't countterminating '\0'	
		if (len > 1024 ){
			__log_thunck(level, tag, "===exceeded logger buffer size(1024)====\n" );
			return ;
		}

		vsprintf_s( buffer, fmt, args ); // C4996

		__log_thunck(level, tag, buffer );

		
	}
	static void logW (int level, std::wstring tag, const wchar_t * fmt, ...)
	{
		va_list args;
		int     len;
		wchar_t buffer[1024];

		// retrieve the variable arguments
		va_start( args, fmt );

		len = _vscwprintf( fmt, args ) + 1;  // _vscprintf doesn't countterminating '\0'	
		if (len > 1024 ){
			__log_thunckW(level, tag, L"===exceeded logger buffer size(1024)====\n" );
			return ;
		}

		vswprintf_s( buffer, fmt, args ); // C4996

		__log_thunckW(level, tag, buffer );

	}



private:
	static void __log_thunck (int level, std::string tag, char* log){
		//NativeMediaLogger::log(level, tag, log );
	}
	static void __log_thunckW (int level, std::wstring tag, wchar_t* log){
		//NativeMediaLogger::logW(level, tag, log );
	}
};

}
