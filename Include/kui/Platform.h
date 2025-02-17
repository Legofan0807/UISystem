#pragma once
#if _WIN32
#define KLEMMUI_WINDOWS
#elif __linux__
#define KLEMMUI_LINUX
#elif KLEMMUI_WEB_BUILD
#define KLEMMUI_WEB
#else
#error Unknown Platform
#endif
#include "Window.h"

namespace kui::platform
{
	namespace win32
	{
		enum class WindowFlag : int
		{
			DarkTitleBar = 1 << 16,
		};
	}

	namespace linux
	{
#if __linux__
		bool GetUseWayland();

		void AlwaysUseWayland();
		void AlwaysUseX11();
#else
		constexpr bool GetUseWayland()
		{
			return false;
		}
		constexpr void AlwaysUseWayland() {}
		constexpr void AlwaysUseX11() {}
#endif
	}
}


inline kui::Window::WindowFlag operator|(kui::Window::WindowFlag a, kui::platform::win32::WindowFlag b)
{
	return kui::Window::WindowFlag(int(a) | int(b));
}
