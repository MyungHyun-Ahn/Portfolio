#include "pch.h"
#include "CSerializableBuffer.h"
#include "NetworkLimits.h"
#include "CommonProtocol.h"
#include "Config.h"

namespace Chatting
{
	namespace
	{
		bool ConfigError(const WCHAR *pFileName, const WCHAR *pReason)
		{
			MHLib::utils::CLogger::GetInstance()->WriteLogConsole(MHLib::utils::ELogLevel::Error,
				L"CONFIG ERROR: %ls: %ls", pFileName, pReason);
			return false;
		}

		bool LoadLogLevel(MHLib::utils::CFileLoader &loader, const WCHAR *pKey, MHLib::utils::ELogLevel &outLevel)
		{
			using MHLib::utils::ELogLevel;
			std::wstring value;
			if (!loader.Load(L"Server", pKey, &value))
				return true;
			if (value == L"Debug")
				outLevel = ELogLevel::Debug;
			else if (value == L"Info")
				outLevel = ELogLevel::Info;
			else if (value == L"Error")
				outLevel = ELogLevel::Error;
			else
				return false;
			return true;
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
	}

	bool LoadConfig(const WCHAR *pFileName, bool isServer, SChatConfig &outConfig)
	{
		constexpr DWORD kExecutablePathCapacity = 32768;
		std::wstring path(kExecutablePathCapacity, L'\0');
		DWORD length = GetModuleFileNameW(nullptr, path.data(), kExecutablePathCapacity);
		if (length == 0 || length >= kExecutablePathCapacity)
			return ConfigError(pFileName, L"cannot resolve executable directory");
		path.resize(length);
		size_t separator = path.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
			return ConfigError(pFileName, L"cannot resolve executable directory");
		path.resize(separator + 1);
		path += pFileName;
		if (!CheckConfigFile(path.c_str()))
			return ConfigError(pFileName, L"missing/unreadable file or invalid UTF-16LE BOM encoding");

		MHLib::utils::CFileLoader loader;
		loader.Parse(path.c_str());
		const WCHAR *pSection = isServer ? L"Server" : L"Client";
		SChatConfig config;
		std::wstring ip;
		if (!loader.Load(pSection, L"IP", &ip) || ip != L"127.0.0.1")
			return ConfigError(pFileName, L"IP must be 127.0.0.1 (local test only)");
		config.m_ip.assign(ip.begin(), ip.end());
		INT64 number = 0;
		if (!loader.Load(pSection, L"PORT", &number) || number < 1 || number > 65535)
			return ConfigError(pFileName, L"PORT is missing or invalid (1..65535)");
		config.m_port = static_cast<USHORT>(number);
		if (isServer && (!LoadLogLevel(loader, L"CONSOLE_LOG_LEVEL", config.m_consoleLogLevel)
			|| !LoadLogLevel(loader, L"FILE_LOG_LEVEL", config.m_fileLogLevel)))
			return ConfigError(pFileName, L"log levels must be Debug, Info or Error");
		if (isServer)
		{
			std::wstring logPollInterval;
			if (loader.Load(pSection, L"LOG_POLL_INTERVAL_MS", &logPollInterval))
			{
				if (!loader.Load(pSection, L"LOG_POLL_INTERVAL_MS", &number) || number < 1 || number > 1000)
					return ConfigError(pFileName, L"LOG_POLL_INTERVAL_MS must be an integer (1..1000)");
				config.m_logPollInterval = std::chrono::milliseconds(number);
			}
		}
		if (!isServer)
		{
			std::wstring connectTimeout;
			if (loader.Load(pSection, L"CONNECT_TIMEOUT_MS", &connectTimeout))
			{
				if (!loader.Load(pSection, L"CONNECT_TIMEOUT_MS", &number) || number < 1 || number > MAXDWORD)
					return ConfigError(pFileName, L"CONNECT_TIMEOUT_MS must be an integer (1..4294967295)");
				config.m_connectTimeoutMilliseconds = static_cast<DWORD>(number);
			}
			std::wstring recvBufferBytes;
			if (loader.Load(pSection, L"RECV_BUFFER_BYTES", &recvBufferBytes))
			{
				constexpr int kMinRecvBufferBytes = static_cast<int>(NetworkLib::CSerializableBuffer<FALSE>::EBufferDefine::PacketMaxSize);
				if (!loader.Load(pSection, L"RECV_BUFFER_BYTES", &number) || number < kMinRecvBufferBytes
					|| number > NetworkLib::kMaxNetRecvBufferBytes)
					return ConfigError(pFileName, L"RECV_BUFFER_BYTES must fit a full Net packet and the ring's cursor range");
				config.m_recvBufferBytes = static_cast<int>(number);
			}
			if (!loader.Load(pSection, L"ACCOUNT_NO", &number) || number < 0)
				return ConfigError(pFileName, L"ACCOUNT_NO is missing or invalid (0..INT64_MAX)");
			// 0은 프로세스 ID 자동 선택. 같은 설정으로 여러 클라이언트를 실행한다.
			config.m_accountNo = number == 0 ? static_cast<INT64>(GetCurrentProcessId()) : number;
			if (!loader.Load(pSection, L"NICKNAME", &config.m_name)
				|| !IsValidString(config.m_name, kMaxNameCodeUnits))
				return ConfigError(pFileName, L"NICKNAME is missing or invalid (1..19 UTF-16 code units)");
			if (!loader.Load(pSection, L"SECTOR_X", &number) || number < 0 || number >= kMaxSectorX)
				return ConfigError(pFileName, L"SECTOR_X is missing or invalid (0..49)");
			config.m_sectorX = static_cast<WORD>(number);
			if (!loader.Load(pSection, L"SECTOR_Y", &number) || number < 0 || number >= kMaxSectorY)
				return ConfigError(pFileName, L"SECTOR_Y is missing or invalid (0..49)");
			config.m_sectorY = static_cast<WORD>(number);
		}
		outConfig = config;
		return true;
	}
}
