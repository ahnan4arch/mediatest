#pragma once
using System::Exception;
using System::IntPtr;
using System::String;
using System::Windows::Application;
using System::Windows::Int32Rect;
using System::Windows::Media::Imaging::BitmapImage;
using System::Windows::Media::PixelFormat;
using System::Windows::Media::PixelFormats;
using System::Runtime::InteropServices::Marshal;

#include "NativeMediaLog.h"

// fix a stupid compiler error
#undef FindResource
namespace WinRTCSDK
{
	class ResourceHelper
	{
	public:
		static IntPtr GetBitmapDataById(int %w, int %h, String ^ resId)
		{
			Application ^ app = Application::Current;
			BitmapImage ^ bmp = nullptr;
			IntPtr dptr = IntPtr(0);
			try 
			{
				bmp = (BitmapImage ^)app->FindResource(resId);  
				if (bmp != nullptr)
				{
					if (bmp->Format  == PixelFormats::Bgra32||
						bmp->Format  == PixelFormats::Pbgra32)
					{
						w = bmp->PixelWidth;
						h = bmp->PixelHeight;
						dptr = Marshal::AllocHGlobal(w*h* 4);
						bmp->CopyPixels(Int32Rect::Empty, dptr, w*h*4, w*4);

						return dptr;
					}
				}
			}
			catch ( Exception ^ex) {
				NativeMediaLogger::log(LogLevel::Error, "ResourceHelper", ex->StackTrace);
			}
			return IntPtr(0);
		}

	};


}
