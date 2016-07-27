#pragma once

//////////////////////////////////////////////////
// This is a managed class for UI to integrate 
// the native video
using System::Exception;
using System::IntPtr;
using System::String;
using System::EventArgs;
using System::EventHandler;
using System::Environment;
using System::Windows::Rect;
using System::Windows::Int32Rect;
using System::Windows::FrameworkElement;
using System::Windows::Media::CompositionTarget;
using System::Windows::Interop::HwndHost;
using System::Runtime::InteropServices::HandleRef;
using System::Windows::DependencyPropertyChangedEventHandler;
using System::Windows::DependencyPropertyChangedEventArgs;
using System::Windows::Media::RenderOptions;
using System::Windows::Media::BitmapScalingMode;
using System::Windows::UIElement;
using System::Windows::Media::Visual;
// for message handling
using System::Windows::Input::Mouse;
using System::Windows::Input::MouseButton;
using System::Windows::Input::InputManager;
using System::Windows::Input::MouseButtonEventArgs;
using System::Windows::Input::CommandManager;
using System::Windows::Input::InputEventArgs;
using System::Reflection::Assembly;
using System::Reflection::BindingFlags;
using System::Windows::RoutedEvent;
using System::Windows::PresentationSource;
using System::Windows::Input::InputMode;
using System::Reflection::ConstructorInfo;
using System::Reflection::BindingFlags;
using System::Reflection::FieldInfo;
using System::Type;

#include "BaseWnd.h"
#include "NativeMediaLog.h"

namespace WinRTCSDK
{
	class NativeVideoHWND
	{
	public:
		NativeVideoHWND (int maxNum):maxNum_(maxNum)
		{
			dummyMain_ = new BaseWnd();
			dummyMain_->Create(0, L"", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, NULL);
			wndList_ = (BaseWnd **) malloc ( sizeof (BaseWnd *) * maxNum_);
			for (int i=0; i<maxNum_; i++)
			{
				BaseWnd * wnd = new BaseWnd();
				wnd->Create(0, L"", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 1, 1, *dummyMain_, NULL, NULL);
				wndList_[i] = wnd;
			}
		}

		HWND Get (int idx)
		{
			if ( idx >=0 && idx< maxNum_){
				return *wndList_[idx];
			}else{
				return NULL;
			}
		}

		BOOL ResetParent(int idx)
		{
			if ( idx<0 || idx>maxNum_) return FALSE;
			::SetParent( *wndList_[idx], *dummyMain_);
			return TRUE;
		}

		BOOL SetParent(int idx, HWND hParent)
		{
			if ( idx<0 || idx>maxNum_) return FALSE;
			::SetParent( *wndList_[idx], hParent);
			return TRUE;
		}

		~NativeVideoHWND()
		{
			for (int i=0; i<maxNum_; i++)
			{
				delete wndList_[i];
			}
			free (wndList_);
		}
	private:
		int maxNum_;
		BaseWnd ** wndList_;
		BaseWnd *  dummyMain_;

	};

	private ref class VideoHWND :public HwndHost
	{
	public:
		IntPtr GetHwnd()
		{
			if ( hwndManager_ == NULL) return IntPtr(0);
			return IntPtr(hwndManager_->Get(index_));
		}
		
		void ClearNative()
		{
			hwndManager_ = NULL;
			index_ = -1;
		}

		VideoHWND (NativeVideoHWND * hwndManager, int index) 
		{
			hwndManager_ = hwndManager;
			index_ = index;
		}

	protected:
		virtual HandleRef  BuildWindowCore(HandleRef hwndParent) override
		{
			if ( hwndManager_ == NULL) return HandleRef(this, IntPtr(0));
			hwndManager_->SetParent(index_, (HWND)(void*)hwndParent.Handle);
			return HandleRef(this, GetHwnd());
		}

		virtual IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam,  bool % handled) override
		{
			switch (msg)
			{
			case WM_LBUTTONDOWN:
				{
					// following method using reflection of the framework types, 
					// and the types can  be changed by MS. so put it in a try block
					try{
						// currently x, y is not used. just set to none-zero
						RaiseMouseInputReportEvent(this, Environment::TickCount, 100, 100, 0);
					}catch( Exception ^ e){
						NativeMediaLogger::log(LogLevel::Error, "NativeMedia - VideoHwnd", e->StackTrace->ToString());
					}
					MouseButtonEventArgs^ eventArgs = gcnew MouseButtonEventArgs(
						Mouse::PrimaryDevice,
						Environment::TickCount,
						MouseButton::Left);

					eventArgs->RoutedEvent = UIElement::PreviewMouseDownEvent;
					eventArgs->Source = this;
					InputManager::Current->ProcessInput(eventArgs);

					eventArgs->RoutedEvent = UIElement::MouseDownEvent;
					InputManager::Current->ProcessInput(eventArgs);

					CommandManager::InvalidateRequerySuggested();

					handled = true;
					return IntPtr::Zero;

				}
			default:
				break;
			}
			return HwndHost::WndProc(hwnd, msg, wParam, lParam, handled);

		}

		void RaiseMouseInputReportEvent(Visual ^ eventSource, int timestamp, int pointX, int pointY, int wheel)
		{
			Assembly^ targetAssembly = Assembly::GetAssembly(InputEventArgs::typeid);
			Type^ mouseInputReportType = targetAssembly->GetType("System.Windows.Input.RawMouseInputReport");
			Type^ rawMouseActionsType = targetAssembly->GetType("System.Windows.Input.RawMouseActions");
			Type^ inputReportEventArgsType = targetAssembly->GetType("System.Windows.Input.InputReportEventArgs");
					
			int AbsoluteMove = (int)rawMouseActionsType
				->GetField("AbsoluteMove", BindingFlags::NonPublic | BindingFlags::Public | BindingFlags::Static)
				->GetValue(nullptr);
			int Activate = (int)rawMouseActionsType
				->GetField("Activate", BindingFlags::NonPublic | BindingFlags::Public | BindingFlags::Static)
				->GetValue(nullptr);

			array < Object ^> ^ args = gcnew array < Object ^> (8);
			args[0] = InputMode::Foreground;
			args[1] = timestamp;
			args[2] = PresentationSource::FromVisual(eventSource);
			args[3] = AbsoluteMove | Activate,
			args[4] = pointX;
			args[5] = pointY;
			args[6] = wheel;
			args[7] = IntPtr(0);
			array<ConstructorInfo^>^  ctors = mouseInputReportType->GetConstructors();
			Object^ mouseInputReport = ctors[0]->Invoke(args);

			mouseInputReportType->
					GetField("_isSynchronize", BindingFlags::NonPublic | BindingFlags::Instance)
					->SetValue(mouseInputReport, true);

			args = gcnew array < Object ^> (2);
			args[0] = Mouse::PrimaryDevice;
			args[1] = mouseInputReport;
			InputEventArgs^ inputReportEventArgs = (InputEventArgs^) inputReportEventArgsType
					->GetConstructors()[0]->Invoke(args);

			inputReportEventArgs->RoutedEvent = (RoutedEvent^) (InputManager::typeid)
				->GetField("PreviewInputReportEvent", BindingFlags::NonPublic | BindingFlags::Instance | BindingFlags::Static)
				->GetValue(nullptr);

			InputManager::Current->ProcessInput((InputEventArgs^) inputReportEventArgs);
		}


		virtual void DestroyWindowCore(HandleRef hwnd) override
		{
			if ( hwndManager_ == NULL) return;
			hwndManager_->ResetParent(index_);
		}

	private:
	    NativeVideoHWND * hwndManager_;
		int index_;
	};


}
