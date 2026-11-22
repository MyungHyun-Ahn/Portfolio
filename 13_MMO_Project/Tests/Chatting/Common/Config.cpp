#include "pch.h"
#include <limits>
#include <utility>
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

		bool LoadServerNumber(MHLib::utils::CFileLoader &loader, const WCHAR *pFileName,
			const WCHAR *pKey, int maxValue, int &outValue)
		{
			std::wstring text;
			if (!loader.Load(L"Server", pKey, &text))
				return true;
			INT64 value = 0;
			if (!loader.Load(L"Server", pKey, &value) || value < 1 || value > maxValue)
			{
				const std::wstring reason = std::wstring(pKey) + L" must be an integer (1.." + std::to_wstring(maxValue) + L")";
				return ConfigError(pFileName, reason.c_str());
			}
			outValue = static_cast<int>(value);
			return true;
		}
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

	bool LoadConfig(const WCHAR *pFileName, bool isServer, SChatConfig &outConfig)
	{
		std::wstring path;
		if (!GetExecutableDirectory(path))
			return ConfigError(pFileName, L"cannot resolve executable directory");
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
			std::wstring metricsPort;
			if (loader.Load(pSection, L"METRICS_PORT", &metricsPort))
			{
				if (!loader.Load(pSection, L"METRICS_PORT", &number) || number < 0 || number > 65535)
					return ConfigError(pFileName, L"METRICS_PORT must be an integer (0..65535; 0 disables metrics)");
				config.m_metricsPort = static_cast<USHORT>(number);
			}
			std::wstring metricsBindAddress;
			IN_ADDR metricsAddress{};
			if (loader.Load(pSection, L"METRICS_BIND_ADDRESS", &metricsBindAddress))
			{
				if (InetPtonW(AF_INET, metricsBindAddress.c_str(), &metricsAddress) != 1 || metricsAddress.S_un.S_addr == INADDR_ANY)
					return ConfigError(pFileName, L"METRICS_BIND_ADDRESS must be a specific IPv4 address");
				config.m_metricsBindAddress.assign(metricsBindAddress.begin(), metricsBindAddress.end());
			}
			if (config.m_metricsPort != 0 && config.m_metricsBindAddress.empty())
				return ConfigError(pFileName, L"METRICS_BIND_ADDRESS is required when metrics are enabled");

			constexpr int kMaxAcceptExCount = 65535;
			constexpr int kMaxContentsWorkerCount = 64;
			constexpr int kMaxContentsFps = 1000; // 타이머 간격은 최소 1ms를 유지한다.
			constexpr int kMaxLogQueueCapacity = 1024 * 1024;
			if (!LoadServerNumber(loader, pFileName, L"ACCEPTEX_COUNT", kMaxAcceptExCount, config.m_acceptExCount)
				|| !LoadServerNumber(loader, pFileName, L"IOCP_WORKER_COUNT", MAXIMUM_WAIT_OBJECTS, config.m_iocpWorkerCount)
				|| !LoadServerNumber(loader, pFileName, L"IOCP_CONCURRENCY", config.m_iocpWorkerCount, config.m_iocpConcurrency)
				|| !LoadServerNumber(loader, pFileName, L"CONTENTS_WORKER_COUNT", kMaxContentsWorkerCount, config.m_contentsWorkerCount)
				|| !LoadServerNumber(loader, pFileName, L"CONTENTS_FPS", kMaxContentsFps, config.m_contentsFps)
				|| !LoadServerNumber(loader, pFileName, L"LOG_QUEUE_CAPACITY", kMaxLogQueueCapacity, config.m_logQueueCapacity))
				return false;
			if (config.m_iocpConcurrency > config.m_iocpWorkerCount)
				return ConfigError(pFileName, L"IOCP_CONCURRENCY must not exceed IOCP_WORKER_COUNT");

			std::wstring maxSessionCount;
			if (loader.Load(pSection, L"MAX_SESSION_COUNT", &maxSessionCount))
			{
				constexpr auto kMaxSessionCount = (std::numeric_limits<USHORT>::max)();
				if (!loader.Load(pSection, L"MAX_SESSION_COUNT", &number) || number < 1 || number > kMaxSessionCount)
					return ConfigError(pFileName, L"MAX_SESSION_COUNT must be an integer (1..65535)");
				config.m_maxSessionCount = static_cast<USHORT>(number);
			}
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
