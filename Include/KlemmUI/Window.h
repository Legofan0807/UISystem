#pragma once
#include <string>
#include "Vector2.h"
#include "Input.h"
#include "Timer.h"
#include <vector>
#include <atomic>
#include "Rendering/ShaderManager.h"
#include "UI/UIManager.h"

class UIBox;

namespace KlemmUI
{
	/**
	* @brief
	* A KlemmUI window.
	* 
	* Every new UI element will be added to the active window. By default, the active window is the window that was last updated or created.
	* The active window can be set using the KlemmUI::Window::SetAsActiveWindow() function.
	* The active window is thread local. On a different thread, another window might be active.
	*/
	class Window
	{
		void* SDLWindowPtr = nullptr;

		Vector2ui WindowSize;

		void UpdateSize();
		Timer WindowDeltaTimer;

		float FrameDelta = 0;

		void WaitFrame();

		std::atomic<bool> ShouldClose = false;
		std::atomic<bool> ShouldUpdateSize = false;
		static std::vector<Window*> ActiveWindows;

		void* GLContext = nullptr;
	public:
		
		void SetAsActiveWindow();
		
		void SetWindowActive();

		void OnResized();

		/**
		* @brief
		* Sets a minimum size for the window.
		*/
		void SetMinSize(Vector2ui MinimumSize);
		/**
		* @brief
		* Sets a maximum size for the window.
		*/
		void SetMaxSize(Vector2ui MaximumSize);

		/**
		* @brief
		* Gets a list of all currently active windows.
		*/
		static const std::vector<Window*>& GetActiveWindows();

		/**
		* @brief
		* Target framerate of the window.
		* A value of 0 uses the framerate of the window's monitor.
		*/
		uint32_t TargetFPS = 0;

		/**
		* @brief
		*/
		float Time = 0;

		/**
		* @brief
		* Closes the window.
		* 
		* This function can be called from any thread.
		*/
		void Close();

		/**
		* @brief
		* Returns the time between the last two calls to UpdateWindow.
		*/
		float GetDeltaTime() const;

		InputManager Input;
		ShaderManager Shaders;
		UIManager UI;

		static const Vector2ui POSITION_CENTERED;
		static const Vector2ui SIZE_DEFAULT;

		/**
		* @brief
		* Flags for window construction.
		*/
		enum class WindowFlag
		{
			///No window flags.
			None           = 0b00000,
			/// Borderless window.
			Borderless     = 0b00001,
			/// The window is resizable.
			Resizable      = 0b00010,
			/// The window should appear on top of all other windows.
			AlwaysOnTop    = 0b00100,
			/// The window should start maximized.
			FullScreen     = 0b01000,
		};

		/**
		* @brief
		* Constructs a window with the given parameters
		*/
		Window(std::string Name, WindowFlag Flags, Vector2ui WindowPos = POSITION_CENTERED, Vector2ui WindowSize = SIZE_DEFAULT);
		virtual ~Window();

		static Window* GetActiveWindow();

		void* GetSDLWindowPtr() const;

		/**
		* @brief
		* Updates the current window, re-draws the screen if necessary.
		* 
		* ```
		* while (MyWindow.UpdateWindow())
		* {
		*     // ...
		* }
		* ```
		* 
		* @return
		* Returns if the window should continue being shown.
		*/
		bool UpdateWindow();

		/**
		* @brief
		* Gets the window's aspect ratio.
		* 
		* Aspect ratio = Width / Height.
		*/
		float GetAspectRatio() const;

		/**
		* @brief
		* Gets the window size in pixels.
		*/
		Vector2ui GetSize() const;

		/**
		* @brief
		* Sets the grabbable callback.
		* 
		* For a borderless window, the window manager has to know which area is grabbable by the mouse cursor, eg. the mouse cursor.
		*/
		bool(*IsAreaGrabbableCallback)(KlemmUI::Window* Target);

		/**
		* @brief
		*/
		void SetWindowFlags(WindowFlag NewFlags);
		WindowFlag GetWindowFlags() const;
		void MakeContextCurrent();

	private:
		int ToSDLWindowFlags(WindowFlag Flags);
		WindowFlag CurrentWindowFlags;
	};

	Window::WindowFlag operator|(Window::WindowFlag a, Window::WindowFlag b);
	Window::WindowFlag operator&(Window::WindowFlag a, Window::WindowFlag b);
}