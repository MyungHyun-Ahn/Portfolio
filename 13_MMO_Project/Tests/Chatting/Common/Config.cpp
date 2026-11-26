#include "pch.h"
#include <utility>
#include "Config.h"

namespace Chatting
{
	bool ConfigError(const WCHAR *pFileName, const WCHAR *pReason)
	{
		MHLib::utils::CLogger::GetInstance()->WriteLogConsole(MHLib::utils::ELogLevel::Error,
			L"CONFIG ERROR: %ls: %ls", pFileName, pReason);
		return false;
	}

	bool CheckConfigFile(const WCHAR *pPath)
	{
		// 기존 로더에 일반적인 파일/인코딩 오류만 넘기지 않는다.
		HANDLE file = CreateFileW(pPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER size{};
		BYTE bom[2]{};
		DWORD bytes = 0;
		bool isValid = GetFileSizeEx(file, &size) && size.QuadPart >= 4 && size.QuadPart % 2 == 0
			&& ReadFile(file, bom, sizeof(bom), &bytes, nullptr) && bytes == sizeof(bom) && bom[0] == 0xff && bom[1] == 0xfe;
		CloseHandle(file);
		return isValid;
	}

	bool GetExecutableDirectory(std::wstring &outDirectory)
	{
		constexpr DWORD kExecutablePathCapacity = 32768;
		std::wstring path(kExecutablePathCapacity, L'\0');
		DWORD length = GetModuleFileNameW(nullptr, path.data(), kExecutablePathCapacity);
		if (length == 0 || length >= kExecutablePathCapacity)
			return false;
		path.resize(length);
		size_t separator = path.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
			return false;
		path.resize(separator + 1);
		outDirectory = std::move(path);
		return true;
	}

	bool LoadConfigFile(const WCHAR *pFileName, MHLib::utils::CFileLoader &outLoader)
	{
		std::wstring path;
		if (!GetExecutableDirectory(path))
			return ConfigError(pFileName, L"cannot resolve executable directory");
		path += pFileName;
		if (!CheckConfigFile(path.c_str()))
			return ConfigError(pFileName, L"missing/unreadable file or invalid UTF-16LE BOM encoding");

		if (!outLoader.Parse(path.c_str()))
			return ConfigError(pFileName, L"failed to parse config file");
		return true;
	}


}
