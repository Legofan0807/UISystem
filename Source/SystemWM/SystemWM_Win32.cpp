#if _WIN32
#include "SystemWM.h"
#include "SystemWM_Win32.h"
#include <kui/App.h>
#include <Windows.h>
#include <windowsx.h>
#include "../Internal/Internal.h"
#include <GL/glew.h>
#include <kui/Platform.h>
#include <GL/wglew.h>
#include <dwmapi.h>
#include <iostream>
#include <array>
#include <map>

#undef IsMaximized

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static bool IsMaximized(HWND hWnd)
{
	WINDOWPLACEMENT placement;
	ZeroMemory(&placement, sizeof(placement));
	if (!::GetWindowPlacement(hWnd, &placement))
	{
		return false;
	}

	return placement.showCmd == SW_MAXIMIZE;
}

static std::wstring ToWstring(std::string utf8)
{
	int wchars_num = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
	wchar_t* wstr = new wchar_t[wchars_num];
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr, wchars_num);
	std::wstring str = wstr;
	delete[] wstr;
	return str;
}

static std::array<Vec2ui, 2> AdjustWindowSize(Vec2ui Pos, Vec2ui InSize, DWORD Style, DWORD ExStyle)
{
	RECT SizeRect = RECT
	{
		.left = LONG(Pos.X),
		.top = LONG(Pos.Y),
		.right = LONG(Pos.X + InSize.X),
		.bottom = LONG(Pos.Y + InSize.Y),
	};

	AdjustWindowRectEx(&SizeRect, Style, false, ExStyle);

	return
	{
		Vec2ui(SizeRect.left, SizeRect.top),
		Vec2ui(SizeRect.right - SizeRect.left, SizeRect.bottom - SizeRect.top)
	};
}

namespace kui::systemWM::Borderless
{
	// Based on: https://github.com/melak47/BorderlessWindow/blob/main/src/BorderlessWindow.cpp

	static bool IsCompositionEnabled()
	{
		BOOL composition_enabled = FALSE;
		bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
		return composition_enabled && success;
	}

	static void SetShadow(HWND handle, bool enabled)
	{
		if (IsCompositionEnabled())
		{
			static const MARGINS shadow_state[2]{ { 0,0,0,0 },{ 1,1,1,1 } };
			::DwmExtendFrameIntoClientArea(handle, &shadow_state[enabled]);
		}
	}
	/*
	 * Adjust client rect to not spill over monitor edges when maximized.
	 * rect(in/out): in: proposed window rect, out: calculated client rect
	 * Does nothing if the window is not maximized.
	 */
	static void AdjustFullScreen(HWND window, RECT& rect)
	{
		if (!IsMaximized(window))
		{
			return;
		}

		auto monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
		if (!monitor)
		{
			return;
		}

		MONITORINFO monitor_info{};
		monitor_info.cbSize = sizeof(monitor_info);
		if (!GetMonitorInfo(monitor, &monitor_info))
		{
			return;
		}

		// when maximized, make the client area fill just the monitor (without task bar) rect,
		// not the whole window rect which extends beyond the monitor.
		rect = monitor_info.rcWork;
	}

	static LRESULT HitTest(POINT cursor, kui::systemWM::SysWindow* Window)
	{
		// identify borders and corners to allow resizing the window.
		// Note: On Windows 10, windows behave differently and
		// allow resizing outside the visible window frame.
		// This implementation does not replicate that behavior.
		const POINT border
		{
			::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
			::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
		};
		RECT window;
		if (!::GetWindowRect(Window->WindowHandle, &window))
		{
			return HTNOWHERE;
		}

		const LRESULT drag = Window->Parent->IsAreaGrabbableCallback
			&& Window->Parent->IsAreaGrabbableCallback(Window->Parent) ? HTCAPTION : HTCLIENT;

		if (!Window->Resizable)
		{
			return drag;
		}

		enum region_mask
		{
			client = 0b0000,
			left = 0b0001,
			right = 0b0010,
			top = 0b0100,
			bottom = 0b1000,
		};

		const int result =
			left * (cursor.x < (window.left + border.x)) |
			right * (cursor.x >= (window.right - border.x)) |
			top * (cursor.y < (window.top + border.y)) |
			bottom * (cursor.y >= (window.bottom - border.y));

		switch (result)
		{
		case left: return HTLEFT;
		case right: return HTRIGHT;
		case top: return HTTOP;
		case bottom: return HTBOTTOM;
		case top | left: return HTTOPLEFT;
		case top | right: return HTTOPRIGHT;
		case bottom | left: return HTBOTTOMLEFT;
		case bottom | right: return HTBOTTOMRIGHT;
		case client: return drag;
		default: return HTNOWHERE;
		}
	}
}

static HCURSOR WindowCursors[3] =
{
	LoadCursor(NULL, IDC_ARROW),
	LoadCursor(NULL, IDC_HAND),
	LoadCursor(NULL, IDC_IBEAM),
};

// Used to make touch screen clicking work. When the user touches a point on the screen,
// just a single WM_LBUTTONDOWN is sent. This doesn't work well with the method used to
// actually check if the left mouse button is down, so this timer is used to make sure
// that the left mouse button is down for at least 2 frames (1 to detect the hover, another
// to register the click)
thread_local static int LeftClickOverride = 0;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	using namespace kui;

#undef DELETE
	// Map for mapping Windows virtual key codes to the keycode values of the library.
	// Some vk codes do not have a #define, but why?
	static std::map<int, kui::Key> Keys =
	{
		{VK_ESCAPE, Key::ESCAPE},
		{VK_BACK, Key::BACKSPACE},
		{VK_TAB, Key::TAB},
		{VK_SPACE, Key::SPACE},
		{VK_DELETE, Key::DELETE},
		{VK_OEM_PLUS, Key::PLUS},
		{VK_OEM_COMMA, Key::COMMA},
		{VK_OEM_PERIOD, Key::PERIOD},
		{VK_OEM_2, Key::SLASH},
		{0x30, Key::k0},
		{0x31, Key::k1},
		{0x32, Key::k2},
		{0x33, Key::k3},
		{0x34, Key::k4},
		{0x35, Key::k5},
		{0x36, Key::k6},
		{0x37, Key::k7},
		{0x38, Key::k8},
		{0x39, Key::k9},
		{VK_OEM_1, Key::SEMICOLON},
		{VK_OEM_102, Key::LESS},
		{VK_RETURN, Key::RETURN},
		{VK_OEM_4, Key::LEFTBRACKET},
		{VK_OEM_6, Key::RIGHTBRACKET},
		{VK_RIGHT, Key::RIGHT},
		{VK_LEFT, Key::LEFT},
		{VK_UP, Key::UP},
		{VK_DOWN, Key::DOWN},
		{VK_SHIFT, Key::LSHIFT},
		{VK_CONTROL, Key::LCTRL},
		{VK_MENU, Key::LALT},
		{0x41, Key::a},
		{0x42, Key::b},
		{0x43, Key::c},
		{0x44, Key::d},
		{0x45, Key::e},
		{0x46, Key::f},
		{0x47, Key::g},
		{0x48, Key::h},
		{0x49, Key::i},
		{0x4a, Key::j},
		{0x4b, Key::k},
		{0x4c, Key::l},
		{0x4d, Key::m},
		{0x4e, Key::n},
		{0x4f, Key::o},
		{0x50, Key::p},
		{0x51, Key::q},
		{0x52, Key::r},
		{0x53, Key::s},
		{0x54, Key::t},
		{0x55, Key::u},
		{0x56, Key::v},
		{0x57, Key::w},
		{0x58, Key::x},
		{0x59, Key::y},
		{0x5a, Key::z},
	};

	systemWM::SysWindow* SysWindow =
		reinterpret_cast<systemWM::SysWindow*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

	// Do not do anything if this window doesn't have a Window class associated with it.
	if (!SysWindow && uMsg != WM_NCCREATE)
	{
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_NCCREATE:
	{
		const auto caller = reinterpret_cast<systemWM::SysWindow*>(
			reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

		::SetWindowLongPtr(hWnd, GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(caller));

		break;
	}
	case WM_ACTIVATE:
	{
		return 0;
	}

	case WM_SETCURSOR:
	{
		// If the cursor is in the client area, intercept the cursor signal and replace the cursor
		// with the cursor set by the window. If we don't do this, this will break the cursor changing
		// when hovering the resizable corners of the window.
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(WindowCursors[int(SysWindow->ActiveCursor)]);
			return 0;
		}
		break;
	}

	case WM_NCCALCSIZE:
	{
		if (wParam == TRUE && SysWindow->Borderless)
		{
			// Windows does borderless windows weirdly.
			// This causes the edges of a full screen borderless window to be outside of the actual screen.
			// This code adjusts the size of a borderless window to match the size of the screen
			// when it is put into full screen.
			NCCALCSIZE_PARAMS& Parameters = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
			systemWM::Borderless::AdjustFullScreen(hWnd, Parameters.rgrc[0]);
			return 0;
		}
		break;
	}
	case WM_CHAR:
	case WM_SYSCHAR:
	{
		if (wParam < '\t')
		{
			break;
		}
		if (wParam <= 0x7F)
		{
			// Plain ASCII
			SysWindow->TextInput.push_back((char)wParam);
		}
		else if (wParam <= 0x07FF)
		{
			// 2-byte unicode
			SysWindow->TextInput.push_back((char)(((wParam >> 6) & 0x1F) | 0xC0));
			SysWindow->TextInput.push_back((char)(((wParam >> 0) & 0x3F) | 0x80));
		}
		// More than 2 byte utf-8 is not supported right now.
		break;
	}
	case WM_NCHITTEST:
	{
		// Borderless windows need to define their own hit test for defining the resizable
		// areas and things such as the title bar.
		if (SysWindow->Borderless)
		{
			return systemWM::Borderless::HitTest(POINT{
				GET_X_LPARAM(lParam),
				GET_Y_LPARAM(lParam)
				}, SysWindow);
		}
		break;
	}

	case WM_LBUTTONDOWN:
		LeftClickOverride = 2;
		break;

	case WM_MOUSEWHEEL:
	{
		const int Amount = int16_t(HIWORD(wParam)) / WHEEL_DELTA;
		SysWindow->Parent->Input.MoveMouseWheel(Amount);
		break;
	}

	case WM_CLOSE:
	case WM_QUIT:
	{
		SysWindow->Parent->Close();
		return 0;
	}

	case WM_GETMINMAXINFO:
	{
		// The min and max size of a window is checked by windows using the WM_GETMINMAXINFO message.
		// This returns the min and max size of this window.
		LPMINMAXINFO MinMaxInfo = (LPMINMAXINFO)lParam;
		double Scale = GetDPIScale(SysWindow);
		MinMaxInfo->ptMinTrackSize.x = double(SysWindow->MinSize.X) * Scale;
		MinMaxInfo->ptMinTrackSize.y = double(SysWindow->MinSize.Y) * Scale;
		MinMaxInfo->ptMaxTrackSize.x = std::min(double(SysWindow->MaxSize.X) * Scale, double(INT32_MAX));
		MinMaxInfo->ptMaxTrackSize.y = std::min(double(SysWindow->MaxSize.Y) * Scale, double(INT32_MAX));
		return 0;
	}

	case WM_SYSCOMMAND:
	{
		switch (wParam)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
		break;
	}

	case WM_KEYDOWN:
	{
		const std::map<int, Key>::iterator k = Keys.find(int(wParam));
		if (k != Keys.end())
		{
			SysWindow->Parent->Input.SetKeyDown(k->second, true);
		}
		return 0;
	}

	case WM_KEYUP:
	{
		const std::map<int, Key>::iterator k = Keys.find(int(wParam));
		if (k != Keys.end())
		{
			SysWindow->Parent->Input.SetKeyDown(k->second, false);
		}
		return 0;
	}

	case WM_PAINT:
	{
		SysWindow->Parent->OnResized();
		SysWindow->Parent->RedrawInternal();
		break;
	}

	case WM_SIZE:
	{
		SysWindow->Size = Vec2ui(LOWORD(lParam), HIWORD(lParam));
		break;
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static const CHAR KUI_WINDOW_CLASS_NAME[] = "KlemmUIWindow";

static HICON GetAppIcon(HMODULE Instance)
{
	return LoadIcon(Instance, "APPICON");
}

static bool CheckFlag(kui::Window::WindowFlag Flag, kui::Window::WindowFlag Value)
{
	return (Flag & Value) == Value;
}

static void SetTitleBarDark(kui::systemWM::SysWindow* Target, bool NewIsDark)
{
	BOOL UseDarkMode = NewIsDark;
	DwmSetWindowAttribute(
		Target->WindowHandle, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE,
		&UseDarkMode, sizeof(UseDarkMode));
}

static std::pair<DWORD, DWORD> GetStyleFromFlags(kui::Window::WindowFlag Flags)
{
	using namespace kui;
	DWORD ExStyle;
	DWORD Style;

	ExStyle = WS_EX_APPWINDOW;
	Style = WS_POPUP | WS_CAPTION | WS_SYSMENU;

	if (CheckFlag(Flags, Window::WindowFlag::AlwaysOnTop))
	{
		ExStyle |= WS_EX_TOPMOST;
	}

	if (CheckFlag(Flags, Window::WindowFlag::Resizable))
	{
		// Maximize button. Only a resizable window should have this.
		Style |= WS_MAXIMIZEBOX;
		// WS_THICKFRAME enables window resizing.
		Style |= WS_THICKFRAME;
	}

	if (!CheckFlag(Flags, Window::WindowFlag::Popup))
	{
		Style |= WS_MINIMIZEBOX;
	}
	return { Style, ExStyle };
}

kui::systemWM::SysWindow* kui::systemWM::NewWindow(
	Window* Parent, Vec2ui Size, Vec2ui Pos, std::string Title, Window::WindowFlag Flags)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	SysWindow* OutWindow = new SysWindow();
	OutWindow->Parent = Parent;
	OutWindow->Size = Size;
	OutWindow->Borderless = CheckFlag(Flags, Window::WindowFlag::Borderless);
	OutWindow->Resizable = CheckFlag(Flags, Window::WindowFlag::Resizable);

	const HMODULE Instance = GetModuleHandle(NULL);
	static bool WindowClassExists = false;

	if (!WindowClassExists)
	{
		WNDCLASS NewWindowClass =
		{
			.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
			.lpfnWndProc = &WndProc,
			.hInstance = Instance,
			.hIcon = GetAppIcon(Instance),
			.hCursor = WindowCursors[0],
			.hbrBackground = NULL,
			.lpszMenuName = NULL,
			.lpszClassName = KUI_WINDOW_CLASS_NAME,
		};
		WindowClassExists = true;

		if (!RegisterClass(&NewWindowClass))
		{
			delete OutWindow;
			app::error::Error("Failed to register Win32 window class.", true);
			return nullptr;
		}
	}

	std::pair Style = GetStyleFromFlags(Flags);
	std::array<Vec2ui, 2> WindowSizes = AdjustWindowSize(Pos, Size, Style.first, Style.second);

	OutWindow->WindowHandle = CreateWindowExW(
		Style.second,
		ToWstring(KUI_WINDOW_CLASS_NAME).c_str(),
		ToWstring(Title).c_str(),
		Style.first,
		WindowSizes[0].X,
		WindowSizes[0].Y,
		WindowSizes[1].X,
		WindowSizes[1].Y,
		NULL,
		NULL,
		Instance,
		OutWindow
	);

	if (!OutWindow->WindowHandle)
	{
		delete OutWindow;
		app::error::Error("Failed to create window", true);
		return nullptr;
	}

	if (!CheckFlag(Flags, Window::WindowFlag::IgnoreDPI))
		SetWindowSize(OutWindow, Vec2ui(Vec2f(Size) * GetDPIScale(OutWindow)));

	static PIXELFORMATDESCRIPTOR PixelFormatDescr =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		0,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	OutWindow->DeviceContext = GetDC(OutWindow->WindowHandle);

	int PixelFormat = 0;

	if (!(PixelFormat = ChoosePixelFormat(OutWindow->DeviceContext, &PixelFormatDescr)))
	{
		delete OutWindow;
		app::error::Error("Can't find suitable pixel format.", true);
		return nullptr;
	}

	if (!SetPixelFormat(OutWindow->DeviceContext, PixelFormat, &PixelFormatDescr))
	{
		delete OutWindow;
		app::error::Error("Can't set pixel format.", true);
		return nullptr;
	}

	OutWindow->GLContext = wglCreateContext(OutWindow->DeviceContext);

	if (!OutWindow->GLContext)
	{
		delete OutWindow;
		app::error::Error("Can't create wgl context.", true);
		return nullptr;
	}

	if (OutWindow->Borderless)
	{
		Borderless::SetShadow(OutWindow->WindowHandle, true);
		SetWindowPos(OutWindow->WindowHandle, NULL, Pos.X, Pos.Y, 0, 0,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	}

	if (CheckFlag(Flags, Window::WindowFlag(platform::win32::WindowFlag::DarkTitleBar)))
		SetTitleBarDark(OutWindow, true);

	systemWM::SetWindowCursor(OutWindow, Window::Cursor::Default);
	ShowWindow(OutWindow->WindowHandle, SW_SHOW);

	OutWindow->MakeContextActive();

	return OutWindow;
}

void kui::systemWM::DestroyWindow(SysWindow* Target)
{
	wglDeleteContext(Target->GLContext);
	DestroyWindow(Target->WindowHandle);
	delete Target;
}

void kui::systemWM::SwapWindow(SysWindow* Target)
{
	wglSwapIntervalEXT(0);
	SwapBuffers(Target->DeviceContext);
}

void kui::systemWM::WaitFrame(SysWindow* Target, float RemainingTime)
{
	DwmFlush();
}

void kui::systemWM::ActivateContext(SysWindow* Target)
{
	Target->MakeContextActive();
}

Vec2ui kui::systemWM::GetWindowSize(SysWindow* Target)
{
	return Target->Size;
}

void kui::systemWM::UpdateWindow(SysWindow*)
{
	MSG msg;
	if (LeftClickOverride > 0)
		LeftClickOverride--;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

bool kui::systemWM::WindowHasFocus(SysWindow* Target)
{
	return GetFocus() == Target->WindowHandle;
}

Vec2i kui::systemWM::GetCursorPosition(SysWindow* Target)
{
	POINT p;
	if (GetCursorPos(&p) && ScreenToClient(Target->WindowHandle, &p))
	{
		return Vec2i(p.x, p.y);
	}
	return 0;
}

Vec2ui kui::systemWM::GetScreenSize()
{
	HWND Desktop = GetDesktopWindow();
	RECT Size;
	GetWindowRect(Desktop, &Size);
	return Vec2ui(Size.left + Size.right, Size.top + Size.bottom);
}

std::string kui::systemWM::GetTextInput(SysWindow* Target)
{
	std::string Out = Target->TextInput;
	Target->TextInput.clear();
	return Out;
}

uint32_t kui::systemWM::GetDesiredRefreshRate(SysWindow* From)
{
	HMONITOR Monitor = MonitorFromWindow(From->WindowHandle, MONITOR_DEFAULTTONEAREST);

	MONITORINFOEX Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.cbSize = sizeof(Info);
	GetMonitorInfo(Monitor, &Info);

	DEVMODE DeviceMode;
	ZeroMemory(&DeviceMode, sizeof(DeviceMode));
	DeviceMode.dmSize = sizeof(DeviceMode);
	EnumDisplaySettings(Info.szDevice, ENUM_CURRENT_SETTINGS, &DeviceMode);
	return DeviceMode.dmDisplayFrequency;
}

void kui::systemWM::SetWindowCursor(SysWindow* Target, Window::Cursor NewCursor)
{
	if (Target->ActiveCursor != int(NewCursor))
	{
		Target->ActiveCursor = int(NewCursor);
		SetCursor(WindowCursors[int(NewCursor)]);
	}
}

float kui::systemWM::GetDPIScale(SysWindow* Target)
{
	return (float)GetDpiForWindow(Target->WindowHandle) / 96.0f;
}

void kui::systemWM::SetClipboardText(std::string NewText)
{
	if (!OpenClipboard(nullptr))
	{
		return;
	}

	HGLOBAL ClipBuffer;
	char* buffer;
	EmptyClipboard();
	ClipBuffer = GlobalAlloc(GMEM_DDESHARE, NewText.size() + 1);
	if (!ClipBuffer)
	{
		return;
	}
	buffer = (char*)GlobalLock(ClipBuffer);
	if (!buffer)
	{
		return;
	}

	strcpy(buffer, NewText.c_str());
	GlobalUnlock(ClipBuffer);
	SetClipboardData(CF_TEXT, ClipBuffer);
	CloseClipboard();
}

std::string kui::systemWM::GetClipboardText()
{
	if (!OpenClipboard(nullptr))
	{
		return "";
	}

	HANDLE ClipboardDataHandle = GetClipboardData(CF_TEXT);
	char* pszText = static_cast<char*>(GlobalLock(ClipboardDataHandle));
	if (pszText == nullptr)
	{
		return "";
	}
	std::string Text = pszText;
	GlobalUnlock(ClipboardDataHandle);
	CloseClipboard();
	return Text;
}

bool kui::systemWM::IsLMBDown()
{
	return GetKeyState(VK_LBUTTON) & 0x8000 || LeftClickOverride;
}

bool kui::systemWM::IsRMBDown()
{
	return GetKeyState(VK_RBUTTON) & 0x8000;
}

void kui::systemWM::SysWindow::SetSize(Vec2ui NewSize) const
{
	RECT WindowRect = {};
	GetWindowRect(WindowHandle, &WindowRect);

	RECT NewSizeRect = RECT
	{
		.left = LONG(WindowRect.left),
		.top = LONG(WindowRect.top),
		.right = LONG(WindowRect.left + NewSize.X),
		.bottom = LONG(WindowRect.top + NewSize.Y),
	};

	AdjustWindowRectEx(&NewSizeRect,
		GetWindowLong(WindowHandle, GWL_STYLE),
		false,
		GetWindowLong(WindowHandle, GWL_EXSTYLE));

	MoveWindow(WindowHandle,
		WindowRect.left, WindowRect.top,
		NewSizeRect.right - NewSizeRect.left, NewSizeRect.bottom - NewSizeRect.top, true);
}

void kui::systemWM::SetWindowSize(SysWindow* Target, Vec2ui Size)
{
	Target->SetSize(Size);
}

void kui::systemWM::SetWindowPosition(SysWindow* Target, Vec2ui NewPosition)
{
	NewPosition = AdjustWindowSize(NewPosition, 0,
		GetWindowLong(Target->WindowHandle, GWL_STYLE),
		GetWindowLong(Target->WindowHandle, GWL_EXSTYLE))[0];

	RECT WindowRect = {};
	GetWindowRect(Target->WindowHandle, &WindowRect);
	MoveWindow(Target->WindowHandle,
		NewPosition.X, NewPosition.Y,
		WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, true);
}

void kui::systemWM::SetTitle(SysWindow* Target, std::string Text)
{
	SetWindowTextW(Target->WindowHandle, ToWstring(Text).c_str());
}

bool kui::systemWM::IsWindowFullScreen(SysWindow* Target)
{
	return IsMaximized(Target->WindowHandle);
}

void kui::systemWM::SetWindowMinSize(SysWindow* Target, Vec2ui MinSize)
{
	Target->MinSize = AdjustWindowSize(0, MinSize,
		GetWindowLong(Target->WindowHandle, GWL_STYLE),
		GetWindowLong(Target->WindowHandle, GWL_EXSTYLE))[1];
}

void kui::systemWM::SetWindowMaxSize(SysWindow* Target, Vec2ui MaxSize)
{
	Target->MaxSize = AdjustWindowSize(0, MaxSize,
		GetWindowLong(Target->WindowHandle, GWL_STYLE),
		GetWindowLong(Target->WindowHandle, GWL_EXSTYLE))[1];
}

void kui::systemWM::RestoreWindow(SysWindow* Target)
{
	::ShowWindow(Target->WindowHandle, SW_RESTORE);
}

void kui::systemWM::MinimizeWindow(SysWindow* Target)
{
	::ShowWindow(Target->WindowHandle, SW_MINIMIZE);
}

void kui::systemWM::MaximizeWindow(SysWindow* Target)
{
	::ShowWindow(Target->WindowHandle, SW_MAXIMIZE);
}

void kui::systemWM::UpdateWindowFlags(SysWindow* Target, Window::WindowFlag NewFlags)
{
	RECT WindowRect;
	GetWindowRect(Target->WindowHandle, &WindowRect);

	std::pair Styles = GetStyleFromFlags(NewFlags);
	SetWindowLongPtr(Target->WindowHandle, GWL_STYLE, Styles.first);
	SetWindowLongPtr(Target->WindowHandle, GWL_EXSTYLE, Styles.second);

	Target->Borderless = CheckFlag(NewFlags, Window::WindowFlag::Borderless);
	Borderless::SetShadow(Target->WindowHandle, Target->Borderless);
	SetTitleBarDark(Target, CheckFlag(NewFlags, Window::WindowFlag(platform::win32::WindowFlag::DarkTitleBar)));

	SetWindowPos(Target->WindowHandle, NULL, WindowRect.left, WindowRect.top, 0, 0,
		SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	ShowWindow(Target->WindowHandle, SW_SHOW);
}

bool kui::systemWM::IsWindowMinimized(SysWindow* Target)
{
	WINDOWPLACEMENT placement;
	ZeroMemory(&placement, sizeof(placement));
	if (!::GetWindowPlacement(Target->WindowHandle, &placement))
	{
		return false;
	}

	return placement.showCmd == SW_MINIMIZE;
}

void kui::systemWM::HideWindow(SysWindow* Target)
{
	::ShowWindow(Target->WindowHandle, SW_HIDE);
}

#undef MessageBox

void kui::systemWM::MessageBox(std::string Text, std::string Title, int Type)
{
	std::array<UINT, 3> Types = { 0, MB_ICONWARNING, MB_ICONERROR };

	::MessageBoxW(NULL, ToWstring(Text).c_str(), ToWstring(Title).c_str(), Types[Type]);
}

void kui::systemWM::SysWindow::MakeContextActive() const
{
	if (wglGetCurrentContext() == GLContext)
	{
		return;
	}
	if (!wglMakeCurrent(DeviceContext, GLContext))
	{
		app::error::Error("Failed to make OpenGL context current.", true);
	}
}
#endif
