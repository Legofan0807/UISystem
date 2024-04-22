#pragma once
#include "UIBox.h"
#include "../Vector3.h"
#include "../Vector2.h"

namespace KlemmUI
{
	class Shader;
	struct VertexBuffer;

	/**
	 * @brief
	 * UI element that draws a square over the space it occupies.
	 *
	 * The UIBackground class can have a color, opacity and texture.
	 */
	class UIBackground : public UIBox
	{
		void ScrollTick(Shader* UsedShader);
		void MakeGLBuffers();
		bool UseTexture = false;
		unsigned int TextureID = 0;
	protected:
		Shader* BackgroundShader;
		virtual void DrawBackground();
		Vector3f Color;
		Vector3f BorderColor;
		VertexBuffer* BoxVertexBuffer = nullptr;
		float Opacity = 1;
		Vector3f ColorMultiplier = 1;
		static float GetBorderSize(float InSize, UIBox::SizeMode Mode);
		SizeMode BorderSizeMode = SizeMode::ScreenRelative;

	public:
		/**
		 * @brief
		 * Sets the opacity of the UIBackground.
		 * 
		 * An opacity of 1 is fully visible, an opacity of 0 is completely transparent.
		 */
		UIBackground* SetOpacity(float NewOpacity);

		UIBackground* SetBorderSizeMode(SizeMode NewBorderSize);

		UIBackground* SetBorderColor(Vector3f NewColor);

		/**
		 * @brief
		 * Returns the opacity of the UIBackground.
		 */
		float GetOpacity() const;

		/**
		 * @brief
		 * Sets the color of the UIBackground.
		 */
		virtual UIBackground* SetColor(Vector3f NewColor);

		/**
		 * @brief
		 * Returns the color of the UIBackground.
		 */
		virtual Vector3f GetColor();

		/**
		 * @brief
		 * Sets an image used by this UIBackground.
		 * 
		 * The texture will have the color of the UIBackground.
		 * 
		 * @param UseTexture
		 * True if the background should use a texture. False if not.
		 * 
		 * @param TextureID
		 * The ID of any texture generated by KlemmUI::Texture::LoadTexture() (or any OpenGL texture object).
		 */
		UIBackground* SetUseTexture(bool UseTexture, unsigned int TextureID = 0);

		/**
		 * @brief
		 * UIBackground constructor.
		 * 
		 * @param Horizontal
		 * True if should the children be laid out horizontally, false if vertically.
		 * 
		 * @param Position
		 * The position of the UIBackground. If it has a parent, this will be ignored.
		 * 
		 * @param Color
		 * The color of the UIBackground.
		 * 
		 * @param MinScale
		 * The MinScale value of the UIBackground.
		 * 
		 * @param UsedShader
		 * The shader used by the UIBackground. If this is nullptr, the default shader is used.
		 */
		UIBackground(bool Horizontal, Vector2f Position, Vector3f Color, Vector2f MinScale = Vector2f(0), Shader* UsedShader = nullptr);
		virtual ~UIBackground();
		void Draw() override;
		void Update() override;
		void OnAttached() override;
	};
}