#pragma once
#include "UIBackground.h"
#include "../Vec3.h"
#include "../Rendering/ScrollObject.h"

namespace kui
{

	class Font;
	class UIText;
	struct VertexBuffer;
	class Shader;
	/**
	 * @brief
	 * Functions like a UIButton, but you can input text into it when clicking it.
	 *
	 * The Text field can have a color and opacity.
	 */
	class UITextField : public UIBackground
	{
		bool IsHovered = false;
		bool IsPressed = false;
		bool ClickStartedOnField = false;
		Vec2f IBeamPosition;
		Vec2f IBeamScale = Vec2(0.001, 0.03);
		bool ShowIBeam = false;
		Vec3f TextColor = Vec3f(1);
		Vec3f TextFieldColor = Vec3f(1);
		UIText* TextObject = nullptr;
		bool IsEdited = false;
		std::string EnteredText = "";
		float TextSize = 0.5f;
		void Tick() override;
		float TextTimer = 0.0f;
		std::string HintText; // Will be displayed when the text field is empty
		bool Dragging = false;

		Vec2f TextHighlightStart;
		ScrollObject TextScroll = ScrollObject(0, 1, 1);
		ScrollObject TextRenderScroll = ScrollObject(0, 1, 1, false);
		Vec2f TextHighlightEnd;
	public:
		bool CanEdit = true;
		bool AllowNewLine = false;
		UITextField* SetAllowNewLine(bool NewValue);
		UITextField* SetCanEdit(bool NewValue);
		std::function<void()> OnChanged = nullptr;
		UIBox* ParentOverride = nullptr;
		bool GetIsEdited() const { return IsEdited; }
		UITextField* SetText(std::string NewText);
		UITextField* SetTextSize(float NewTextSize);

		/**
		 * @brief
		 * Sets the font used by the text field to the given font.
		 */
		UITextField* SetFont(Font* NewFont);
		UITextField* SetHintText(std::string NewHintText);
		UITextField* SetColor(Vec3f NewColor) override;
		Vec3f GetColor() const override;
		UITextField* SetTextColor(Vec3f NewColor);
		Vec3f GetTextColor();
		float GetTextSize() const;
		std::string GetText();
		bool GetIsHovered() const;
		bool GetIsPressed() const;
		void Edit();

		kui::UITextField* SetTextSizeMode(UIBox::SizeMode Mode);

		/**
		 * @brief
		 * UITextField constructor.
		 * 
		 * @param Position
		 * The position of the text field.
		 * 
		 * @param Color
		 * The background color of the text field.
		 * 
		 * @param Renderer
		 * The font used in the text field.
		 * 
		 * @param OnChanged
		 * Function called when a new text has been entered.
		 */
		UITextField(Vec2f Position, Vec3f Color, Font* Renderer, std::function<void()> OnChanged);
		~UITextField() override;
		void Update() override;
		void DrawBackground() override;
	};
}