#include "Internal.h"
#include "OpenGL.h"
#include <kui/Rendering/Shader.h>
#include <kui/App.h>
#include "../SystemWM/SystemWM.h"
#include <mutex>
#include <cassert>

bool IsGLEWStarted = false;
std::mutex kui::internal::WindowCreationMutex;

void kui::internal::InitGLContext(Window* From)
{
	From->MakeContextCurrent();

#ifndef KLEMMUI_WEB_BUILD
	if (!IsGLEWStarted)
	{
#if _WIN32
		GLenum GLEWStatus = glewInit();
#else
		GLenum GLEWStatus = glewContextInit();  
#endif
		if (GLEWStatus != GLEW_OK)
		{
			app::error::Error((const char*)glewGetErrorString(GLEWStatus), true);
		}
		IsGLEWStarted = true;
	}
#else
#endif

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	From->Shaders.LoadShader("res:shaders/postprocess.vert", "res:shaders/postprocess.frag", "WindowShader");
}

void kui::internal::DrawWindow(Window* Target)
{
	if (!Target->UI.DrawToWindow)
		return;

	systemWM::SysWindow* SysWindow = static_cast<systemWM::SysWindow*>(Target->GetSysWindow());

	Shader* WindowShader = Target->Shaders.GetShader("WindowShader");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	WindowShader->Bind();
	glViewport(0, 0, (GLsizei)Target->GetSize().X, (GLsizei)Target->GetSize().Y);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Target->UI.GetUIFramebuffer());
	WindowShader->SetInt("u_ui", 0);
	WindowShader->SetInt("u_hasWindowBorder", int((Target->GetWindowFlags() & Window::WindowFlag::Borderless) == Window::WindowFlag::Borderless && !Target->GetWindowFullScreen()));
	WindowShader->SetVec2("u_screenRes", Target->GetSize());
	WindowShader->SetVec3("u_borderColor", Target->BorderColor);

	glDrawArrays(GL_TRIANGLES, 0, 3);
	WindowShader->Unbind();

	systemWM::SwapWindow(SysWindow);
}

std::u32string kui::internal::GetUnicodeString(std::string utf8, bool SameLength)
{
	std::u32string unicode;
	unicode.reserve(utf8.size());
	size_t i = 0;
	while (i < utf8.size())
	{
		unsigned long uni;
		size_t todo;
		unsigned char ch = utf8[i++];
		if (ch <= 0x7F)
		{
			uni = ch;
			todo = 0;
		}
		else if (ch <= 0xBF)
		{
			return std::u32string(utf8.begin(), utf8.end());
		}
		else if (ch <= 0xDF)
		{
			uni = ch & 0x1F;
			todo = 1;
		}
		else if (ch <= 0xEF)
		{
			uni = ch & 0x0F;
			todo = 2;
		}
		else if (ch <= 0xF7)
		{
			uni = ch & 0x07;
			todo = 3;
		}
		else
		{
			return std::u32string(utf8.begin(), utf8.end());
		}
		for (size_t j = 0; j < todo; ++j)
		{
			if (i == utf8.size())
				return std::u32string(utf8.begin(), utf8.end());
			unsigned char ch = utf8[i++];
			if (ch < 0x80 || ch > 0xBF)
				return std::u32string(utf8.begin(), utf8.end());
			uni <<= 6;
			uni += ch & 0x3F;
		}
		if (uni >= 0xD800 && uni <= 0xDFFF)
			return std::u32string(utf8.begin(), utf8.end());
		if (uni > 0x10FFFF)
			return std::u32string(utf8.begin(), utf8.end());
		unicode.push_back(uni);
		if (SameLength)
		{
			for (size_t j = 0; j < todo; j++)
			{
				unicode.push_back(1);
			}
		}
	}
	if (SameLength)
	{
		assert(utf8.size() == unicode.size());
	}
	return unicode;
}