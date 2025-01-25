#include <kui/Resource.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <kui/App.h>

const char* const RESOURCE_PREFIX = "res:";
const char* const FILE_PREFIX = "file:";

thread_local bool kui::resource::ErrorOnFail = true;

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

extern "C"
{
	// Functions generated by KlemmUIRC
	size_t App_GetResourceIndex(const char* FileName);
	size_t App_GetResourceSize(size_t ResourceIndex);
	const char* const App_GetResourceBytes(size_t ResourceIndex);

	// Functions for loading resources from the library itself.
	size_t KlemmUI_GetResourceIndex(const char* FileName);
	size_t KlemmUI_GetResourceSize(size_t ResourceIndex);
	const char* const KlemmUI_GetResourceBytes(size_t ResourceIndex);
}

kui::resource::BinaryData kui::resource::GetBinaryResource(const std::string& Path)
{
	size_t ResourceIndex = App_GetResourceIndex(Path.c_str());
	return BinaryData{
		.Data = reinterpret_cast<const uint8_t*>(App_GetResourceBytes(ResourceIndex)),
		.FileSize = App_GetResourceSize(ResourceIndex),
		.ResourceType = ResourceIndex,
	};
}

std::string kui::resource::GetStringResource(const std::string& Path)
{
	size_t KlemmUIResourceIndex = KlemmUI_GetResourceIndex(Path.c_str());
	if (KlemmUIResourceIndex != SIZE_MAX)
	{
		return std::string(
			KlemmUI_GetResourceBytes(KlemmUIResourceIndex),
			KlemmUI_GetResourceSize(KlemmUIResourceIndex)
		);
	}

	size_t ResourceIndex = App_GetResourceIndex(Path.c_str());
	return std::string(
		App_GetResourceBytes(ResourceIndex),
		App_GetResourceSize(ResourceIndex)
	);
}


bool kui::resource::ResourceExists(const std::string& Path)
{
	size_t KlemmUIResourceIndex = KlemmUI_GetResourceIndex(Path.c_str());
	if (KlemmUIResourceIndex != SIZE_MAX)
		return true;

	return App_GetResourceIndex(Path.c_str()) != SIZE_MAX;
}

std::string kui::resource::GetStringFile(const std::string& Path)
{
	if (!IsFilePath(Path) && (ResourceExists(ConvertResourcePath(Path)) || IsResourcePath(Path)))
	{
		return GetStringResource(ConvertResourcePath(Path));
	}
#ifndef KLEMMUI_WEB_BUILD
	if (std::filesystem::exists(Path) && !std::filesystem::is_directory(Path))
	{
		std::ifstream in = std::ifstream(ConvertFilePath(Path));
		std::stringstream instr;
		instr << in.rdbuf();
		return instr.str();
	}
#endif
	if (ErrorOnFail)
		app::error::Error("Failed to find file: " + Path);
	return std::string();
}

kui::resource::BinaryData kui::resource::GetBinaryFile(const std::string& Path)
{
	if (!IsFilePath(Path) && (ResourceExists(ConvertResourcePath(Path)) || IsResourcePath(Path)))
	{
		return GetBinaryResource(ConvertResourcePath(Path));
	}
	std::string FilePath = ConvertFilePath(Path);
#ifndef KLEMMUI_WEB_BUILD
	try
	{
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
	}
	catch (std::filesystem::filesystem_error)
	{

	}
#endif
	if (ErrorOnFail)
		app::error::Error("Failed to find file: " + Path);
	return BinaryData();
}

void kui::resource::FreeBinaryFile(BinaryData Data)
{
	if (Data.ResourceType == SIZE_MAX)
		delete[] Data.Data;
}

bool kui::resource::FileExists(const std::string& Path)
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