#include <KlemmUI/UI/UIBackground.h>
#include <GL/glew.h>
#include <iostream>
#include "../Rendering/VertexBuffer.h"
#include <KlemmUI/Rendering/Shader.h>
#include <KlemmUI/Application.h>
#include <KlemmUI/Rendering/ScrollObject.h>
#include <KlemmUI/Window.h>
using namespace KlemmUI;

void UIBackground::ScrollTick(Shader* UsedShader)
{
	if (CurrentScrollObject != nullptr)
	{
		glUniform3f(glGetUniformLocation(UsedShader->GetShaderID(), "u_offset"), -CurrentScrollObject->Percentage, CurrentScrollObject->Position.Y, CurrentScrollObject->Position.Y - CurrentScrollObject->Scale.Y);
	}
	else
		glUniform3f(glGetUniformLocation(UsedShader->GetShaderID(), "u_offset"), 0, -1000, 1000);
}

void UIBackground::MakeGLBuffers()
{
	if (BoxVertexBuffer)
		delete BoxVertexBuffer;
	BoxVertexBuffer = new VertexBuffer(
		{
			Vertex(Vector2f(0, 0), Vector2f(0, 0)),
			Vertex(Vector2f(0, 1), Vector2f(0, 1)),
			Vertex(Vector2f(1, 0), Vector2f(1, 0)),
			Vertex(Vector2f(1, 1), Vector2f(1, 1))
		},
		{
			0u, 1u, 2u,
			1u, 2u, 3u
		});
}

void UIBackground::DrawBackground()
{
}

UIBackground* UIBackground::SetOpacity(float NewOpacity)
{
	if (NewOpacity != Opacity)
	{
		Opacity = NewOpacity;
		ParentWindow->UI.RedrawUI();
	}
	return this;
}

float UIBackground::GetOpacity()
{
	return Opacity;
}

UIBackground* UIBackground::SetColor(Vector3f NewColor)
{
	if (NewColor != Color)
	{
		Color = NewColor;
		ParentWindow->UI.RedrawUI();
	}
	return this;
}

Vector3f UIBackground::GetColor()
{
	return Color;
}

UIBackground* UIBackground::SetUseTexture(bool UseTexture, unsigned int TextureID)
{
	if (this->UseTexture != UseTexture || TextureID != this->TextureID)
	{
		this->UseTexture = UseTexture;
		this->TextureID = TextureID;
		ParentWindow->UI.RedrawUI();
	}
	return this;
}

UIBackground::UIBackground(bool Horizontal, Vector2f Position, Vector3f Color, Vector2f MinScale, Shader* UsedShader) : UIBox(Horizontal, Position)
{
	SetMinSize(MinScale);
	this->Color = Color;
	if (!UsedShader)
	{
		this->BackgroundShader = Window::GetActiveWindow()->Shaders.LoadShader("uishader.vert", "uishader.frag", "UI Shader");
	}
	else
	{
		this->BackgroundShader = UsedShader;
	}
	MakeGLBuffers();
}

UIBackground::~UIBackground()
{
	delete BoxVertexBuffer;
}

void UIBackground::Draw()
{
	BackgroundShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, TextureID);
	BoxVertexBuffer->Bind();
	ScrollTick(BackgroundShader);
	glUniform4f(glGetUniformLocation(BackgroundShader->GetShaderID(), "u_color"), Color.X, Color.Y, Color.Z, 1.f);
	glUniform4f(glGetUniformLocation(BackgroundShader->GetShaderID(), "u_transform"), OffsetPosition.X, OffsetPosition.Y, Size.X, Size.Y);
	BackgroundShader->SetFloat("u_opacity", Opacity);
	BackgroundShader->SetInt("u_borderType", (int)BoxBorder);
	BackgroundShader->SetFloat("u_borderScale", BorderRadius / 20.0f);
	BackgroundShader->SetFloat("u_aspectratio", Window::GetActiveWindow()->GetAspectRatio());
	BackgroundShader->SetVec2("u_screenRes", Vector2f(
		(float)Window::GetActiveWindow()->GetSize().X,
		(float)Window::GetActiveWindow()->GetSize().Y));

	BackgroundShader->SetInt("u_usetexture", (int)UseTexture);
	BoxVertexBuffer->Draw();
	DrawBackground();
	BoxVertexBuffer->Unbind();
}

void UIBackground::Update()
{
}

void UIBackground::OnAttached()
{
}
