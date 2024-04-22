#include <KlemmUI/Input.h>
#include <KlemmUI/Window.h>
#include <SDL.h>
#include <iostream>
#include <KlemmUI/Rendering/ScrollObject.h>
using namespace KlemmUI;

Window* KlemmUI::InputManager::GetWindowBySDLID(uint32_t ID)
{
	std::vector<Window*> ActiveWindows = Window::GetActiveWindows();
	for (Window* i : ActiveWindows)
	{
		SDL_Window* SDLWindow = static_cast<SDL_Window*>(i->GetSDLWindowPtr());
		if (SDL_GetWindowID(SDLWindow) == ID)
		{
			return i;
		}
	}
	return nullptr;
}


#define FOR_ALL_WINDOWS(Action) \
	for (auto& i : Window::GetActiveWindows()) \
	{ \
		i->Action; \
	} \

void KlemmUI::InputManager::MoveTextIndex(int Amount, bool RespectShiftPress)
{
	TextIndex = std::max(std::min(TextIndex + Amount, (int)Text.size()), 0);
	if ((!IsKeyDown(Key::LSHIFT) && !IsKeyDown(Key::RSHIFT)) || !RespectShiftPress)
	{
		TextSelectionStart = TextIndex;
	}
}

KlemmUI::InputManager::InputManager(Window* Parent)
{
	ParentWindow = Parent;
	
	// Text input callbacks
	RegisterOnKeyDownCallback(Key::LEFT, [](Window* Win) {
		Win->Input.MoveTextIndex(-1);
		if (Win->Input.Text[Win->Input.TextIndex] & 0b10000000)
		{
			Win->Input.MoveTextIndex(-1);
		}
		});

	RegisterOnKeyDownCallback(Key::RIGHT, [](Window* Win) {
		Win->Input.MoveTextIndex(1);
		if (Win->Input.Text[Win->Input.TextIndex] & 0b10000000)
		{
			Win->Input.MoveTextIndex(1);
		}
		});

	RegisterOnKeyDownCallback(Key::DELETE, [](Window* Win) {
		if (Win->Input.PollForText && Win->Input.TextIndex < Win->Input.Text.size() && Win->Input.TextIndex >= 0)
		{
			if (Win->Input.TextSelectionStart == Win->Input.TextIndex)
			{
				Win->Input.Text.erase(Win->Input.TextIndex, 1);
			}
			else
			{
				Win->Input.DeleteTextSelection();
			}
		}
		});

	RegisterOnKeyDownCallback(Key::BACKSPACE, [](Window* Win) {
		if (Win->Input.PollForText && Win->Input.Text.length() > 0)
		{
			if (Win->Input.TextIndex == Win->Input.Text.size())
			{
				int Difference = std::abs(Win->Input.TextSelectionStart - Win->Input.TextIndex);

				for (int i = 0; i < Difference; i++)
				{
					Win->Input.Text.pop_back();
				}

				if (Difference == 0)
				{
					Win->Input.Text.pop_back();
				}
			}
			else if (Win->Input.TextIndex > 0 || Win->Input.TextSelectionStart > 0)
			{
				if (Win->Input.TextSelectionStart == Win->Input.TextIndex)
				{
					Win->Input.Text.erase(--Win->Input.TextIndex, 1);
				}
				else
				{
					Win->Input.DeleteTextSelection();
				}
			}
			Win->Input.SetTextIndex(std::max(std::min(Win->Input.TextIndex, (int)Win->Input.Text.size()), 0), true);
		}
		});

	RegisterOnKeyDownCallback(Key::ESCAPE, [](Window* Win) {
		Win->Input.PollForText = false;
		});

	RegisterOnKeyDownCallback(Key::RETURN, [](Window* Win) {
		Win->Input.PollForText = false;
		});

	RegisterOnKeyDownCallback(Key::c, [](Window* Win) {
		if (Win->Input.IsKeyDown(Key::LCTRL) || Win->Input.IsKeyDown(Key::RCTRL))
			SDL_SetClipboardText(Win->Input.GetSelectedTextString().c_str());
		});

	RegisterOnKeyDownCallback(Key::x, [](Window* Win) {
		if (Win->Input.IsKeyDown(Key::LCTRL) || Win->Input.IsKeyDown(Key::RCTRL))
		{
			SDL_SetClipboardText(Win->Input.GetSelectedTextString().c_str());
			Win->Input.DeleteTextSelection();
		}
		});

	RegisterOnKeyDownCallback(Key::v, [](Window* Win) {
		if (Win->Input.IsKeyDown(Key::LCTRL) || Win->Input.IsKeyDown(Key::RCTRL))
			Win->Input.AddTextInput(SDL_GetClipboardText());
		});
}

void KlemmUI::InputManager::UpdateCursorPosition()
{
	SDL_Window* SDLWindow = static_cast<SDL_Window*>(ParentWindow->GetSDLWindowPtr());

	if (SDL_GetMouseFocus() != SDLWindow)
	{
		ParentWindow->Input.MousePosition = 99;
		return;
	}

	int Width, Height;
	SDL_GetWindowSize(SDLWindow, &Width, &Height);
	int x;
	int y;

	int PosX, PosY;
	SDL_GetWindowPosition(SDLWindow, &PosX, &PosY);

	SDL_GetGlobalMouseState(&x, &y);

	x -= PosX;
	y -= PosY;

	ParentWindow->Input.MousePosition = Vector2(((float)x / (float)Width - 0.5f) * 2.0f, 1.0f - ((float)y / (float)Height * 2.0f));
}

void InputManager::Poll()
{
	SDL_Event Event;
	int MouseState = 0;
	MouseState = SDL_GetGlobalMouseState(nullptr, nullptr);

	IsLMBClicked = false;
	IsRMBClicked = false;

	bool NewLMBDown = MouseState & SDL_BUTTON(SDL_BUTTON_LEFT);
	bool NewRMBDown = MouseState & SDL_BUTTON(SDL_BUTTON_RIGHT);
	if (!IsLMBDown && NewLMBDown)
	{
		IsLMBClicked = true;
	}
	if (!IsRMBDown && NewRMBDown)
	{
		IsRMBClicked = true;
	}

	IsLMBDown = NewLMBDown;
	IsRMBDown = NewRMBDown;

	while (SDL_PollEvent(&Event))
	{
		switch (Event.type)
		{
		case SDL_KEYDOWN:
			GetWindowBySDLID(Event.window.windowID)->Input.SetKeyDown(static_cast<KlemmUI::Key>(Event.key.keysym.sym), true);
			break;
		case SDL_KEYUP:
			GetWindowBySDLID(Event.window.windowID)->Input.SetKeyDown(static_cast<KlemmUI::Key>(Event.key.keysym.sym), false);
			break;
		case SDL_WINDOWEVENT:
			switch (Event.window.event)
			{
			case SDL_WINDOWEVENT_CLOSE:
				GetWindowBySDLID(Event.window.windowID)->Close();
				break;
			case SDL_WINDOWEVENT_RESIZED:
				GetWindowBySDLID(Event.window.windowID)->OnResized();
				break;
			default:
				break;
			}
			break;
		case SDL_TEXTINPUT:
			GetWindowBySDLID(Event.window.windowID)->Input.AddTextInput(Event.text.text);
			break;
		case SDL_MOUSEWHEEL:
			MoveMouseWheel(Event.wheel.y);
			break;
		default:
			break;
		}
	}

	return;
}

void KlemmUI::InputManager::MoveMouseWheel(int Amount)
{
	while (Amount)
	{
		for (ScrollObject* s : ScrollObject::GetAllScrollObjects())
		{
			if (Amount < 0)
				s->ScrollUp();
			else
				s->ScrollDown();
		}
		if (Amount < 0)
			Amount++;
		else
			Amount--;
	}

}

void KlemmUI::InputManager::AddTextInput(std::string Str)
{
	if (PollForText && (Str[0] >= 32 || Str[0] < 0))
	{
		if (Text.size() < TextIndex)
		{
			TextIndex = (int)Text.size();
		}
		DeleteTextSelection();
		Text.insert(TextIndex, Str);
		MoveTextIndex((int)Str.size(), false);
	}
}

void KlemmUI::InputManager::DeleteTextSelection()
{
	int Difference = std::abs(TextSelectionStart - TextIndex);
	Text.erase(std::min(TextIndex, TextSelectionStart), Difference);
	SetTextIndex(std::min(TextIndex, TextSelectionStart), true);
}

bool InputManager::IsKeyDown(Key PressedKey)
{
	auto Key = PressedKeys.find(PressedKey);

	if (Key == PressedKeys.end())
	{
		return false;
	}
	return Key->second;
}

void InputManager::SetKeyDown(Key PressedKey, bool KeyDown)
{
	auto Key = PressedKeys.find(PressedKey);

	if (Key == PressedKeys.end())
	{
		PressedKeys.insert(std::pair(PressedKey, KeyDown));
	}
	else
	{
		Key->second = KeyDown;
	}
	if (ButtonPressedCallbacks.contains(PressedKey) && KeyDown)
	{
		for (auto Function : ButtonPressedCallbacks[PressedKey])
		{
			Function(ParentWindow);
		}
	}
}

Vector2ui KlemmUI::InputManager::GetMouseScreenPosition()
{
	int x, y;
	SDL_GetGlobalMouseState(&x, &y);
	return Vector2ui(x, y);
}

void InputManager::RegisterOnKeyDownCallback(Key PressedKey, void(*Callback)(Window*))
{
	if (!ButtonPressedCallbacks.contains(PressedKey))
	{
		ButtonPressedCallbacks.insert(std::pair<Key, std::vector<void(*)(Window*)>>(PressedKey, { Callback }));
	}
	else
	{
		ButtonPressedCallbacks[PressedKey].push_back(Callback);
	}
}

std::string KlemmUI::InputManager::GetSelectedTextString() const
{
	int Start = std::min(TextIndex, TextSelectionStart), End = std::max(TextIndex, TextSelectionStart);
	return Text.substr(Start, End - Start);
}

void KlemmUI::InputManager::SetTextIndex(int NewIndex, bool ClearSelection)
{
	TextIndex = NewIndex;
	if (ClearSelection)
	{
		TextSelectionStart = TextIndex;
	}
}
