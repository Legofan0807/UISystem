#pragma once
#include "MarkupStructure.h"
#include "StringParse.h"

namespace kui::MarkupParse
{
	struct FileEntry
	{
		std::string Content;
		std::string Name;
	};

	MarkupStructure::ParseResult ParseFiles(std::vector<FileEntry> Files);

	struct ParsedElement
	{
		std::string Name;
		std::string File;
		size_t Start;
		size_t StartLine;
		size_t End;
		MarkupStructure::MarkupElement StructureElement;
	};

	struct FileResult
	{
		std::vector<ParsedElement> Elements;
		std::vector<MarkupStructure::Constant> Constants;
	};

	FileResult ReadFile(std::vector<stringParse::Line>& Lines, std::string FileName);
	MarkupStructure::MarkupElement ParseElement(ParsedElement& Elem, std::vector<stringParse::Line>& Lines);

	void ParseScope(MarkupStructure::UIElement& Elem, std::vector<stringParse::Line> Lines, size_t Start, bool IsRoot);
}