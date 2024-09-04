#include <KlemmUI/Resource.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <KlemmUI/Application.h>

const char* const RESOURCE_PREFIX = "res:";
const char* const FILE_PREFIX = "file:";
static bool IsResourcePath(const std::string& Path)
{
	return Path.substr(0, strlen(RESOURCE_PREFIX)) == RESOURCE_PREFIX;
}
static bool IsFilePath(const std::string& Path)
{
	return Path.substr(0, strlen(FILE_PREFIX)) == FILE_PREFIX;
}
static std::string ConvertResourcePath(const std::string& Path)
{
	if (IsResourcePath(Path))
		return Path.substr(strlen(RESOURCE_PREFIX));
	return Path;
}
static std::string ConvertFilePath(const std::string& Path)
{
	if (IsFilePath(Path))
		return Path.substr(strlen(FILE_PREFIX));
	return Path;
}


#ifdef KLEMMUI_USE_RESOURCES

#define CAT_HELPER(x, y) x ## y
#define CAT(x, y) CAT_HELPER(x, y)

#define RESOURCE_FUNCTION(y) CAT(CAT(KLEMMUI_RESOURCE_NAME, _), y)
#define KLEMMUI_RESOURCE_FUNCTION(y) CAT(KlemmUI_, y)

extern "C"
{
	// Functions generated by KlemmUIRC
	size_t RESOURCE_FUNCTION(GetResourceIndex)(const char* FileName);
	size_t RESOURCE_FUNCTION(GetResourceSize)(size_t ResourceIndex);
	const char* const RESOURCE_FUNCTION(GetResourceBytes)(size_t ResourceIndex);

	// Functions for loading resources from the library itself.
	size_t KLEMMUI_RESOURCE_FUNCTION(GetResourceIndex)(const char* FileName);
	size_t KLEMMUI_RESOURCE_FUNCTION(GetResourceSize)(size_t ResourceIndex);
	const char* const KLEMMUI_RESOURCE_FUNCTION(GetResourceBytes)(size_t ResourceIndex);
}

KlemmUI::Resource::BinaryData KlemmUI::Resource::GetBinaryResource(const std::string& Path)
{
	size_t ResourceIndex = RESOURCE_FUNCTION(GetResourceIndex)(Path.c_str());
	return BinaryData{
		.Data = reinterpret_cast<const uint8_t*>(RESOURCE_FUNCTION(GetResourceBytes)(ResourceIndex)),
		.FileSize = RESOURCE_FUNCTION(GetResourceSize)(ResourceIndex),
		.ResourceType = ResourceIndex,
	};
}

std::string KlemmUI::Resource::GetStringResource(const std::string& Path)
{
	size_t KlemmUIResourceIndex = KLEMMUI_RESOURCE_FUNCTION(GetResourceIndex)(Path.c_str());
	if (KlemmUIResourceIndex != SIZE_MAX)
	{
		return std::string(
			KLEMMUI_RESOURCE_FUNCTION(GetResourceBytes)(KlemmUIResourceIndex),
			KLEMMUI_RESOURCE_FUNCTION(GetResourceSize)(KlemmUIResourceIndex)
		);
	}

	size_t ResourceIndex = RESOURCE_FUNCTION(GetResourceIndex)(Path.c_str());
	return std::string(
		RESOURCE_FUNCTION(GetResourceBytes)(ResourceIndex),
		RESOURCE_FUNCTION(GetResourceSize)(ResourceIndex)
	);
}


bool KlemmUI::Resource::ResourceExists(const std::string& Path)
{
	size_t KlemmUIResourceIndex = KLEMMUI_RESOURCE_FUNCTION(GetResourceIndex)(Path.c_str());
	if (KlemmUIResourceIndex != SIZE_MAX)
		return true;

	return RESOURCE_FUNCTION(GetResourceIndex)(Path.c_str()) != SIZE_MAX;
}

std::string KlemmUI::Resource::GetStringFile(const std::string& Path)
{
	if (!IsFilePath(Path) && (ResourceExists(ConvertResourcePath(Path)) || IsResourcePath(Path)))
	{
		return GetStringResource(ConvertResourcePath(Path));
	}
	if (std::filesystem::exists(Path) && !std::filesystem::is_directory(Path))
	{
		std::ifstream in = std::ifstream(ConvertFilePath(Path));
		std::stringstream instr;
		instr << in.rdbuf();
		return instr.str();
	}
	Application::Error::Error("Failed to find file: " + Path);
	return std::string();
}

KlemmUI::Resource::BinaryData KlemmUI::Resource::GetBinaryFile(const std::string& Path)
{
	if (!IsFilePath(Path) && (ResourceExists(ConvertResourcePath(Path)) || IsResourcePath(Path)))
	{
		return GetBinaryResource(ConvertResourcePath(Path));
	}
	std::string FilePath = ConvertFilePath(Path);
	if (std::filesystem::exists(FilePath) && !std::filesystem::is_directory(FilePath))
	{
		std::ifstream File = std::ifstream(FilePath, std::ios::binary);

		File.seekg(0, std::ios::end);
		size_t Size = File.tellg();
		File.seekg(0, std::ios::beg);

		uint8_t* Buffer = new uint8_t[Size]();

		File.read((char*)Buffer, Size);
		File.close();

		return BinaryData{
			.Data = Buffer,
			.FileSize = Size,
			};
	}

	Application::Error::Error("Failed to find file: " + Path);
	return BinaryData();
}

void KlemmUI::Resource::FreeBinaryFile(BinaryData Data)
{
	if (Data.ResourceType == SIZE_MAX)
		delete[] Data.Data;
}

bool KlemmUI::Resource::FileExists(const std::string& Path)
{
	if (ResourceExists(ConvertResourcePath(Path)) && !IsFilePath(Path))
	{
		return true;
	}
	std::string FilePath = ConvertFilePath(Path);
	bool Exists = std::filesystem::exists(FilePath) && !std::filesystem::is_directory(FilePath);
	return Exists;
}

#else
#error Not using resources is not supported right now.
#endif