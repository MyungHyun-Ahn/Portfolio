#include "pch.h"
#include <utility>
#include "CommonProtocol.h"
#include "Config.h"
#include "SClientNetworkSettings.h"
#include "ClientConfig.h"

namespace Chatting
{
	namespace
	{
		// 콘솔 클라이언트의 루프백 접속 주소와 포트를 읽는다.
		bool LoadEndpoint(MHLib::utils::CFileLoader &loader, const WCHAR *pFileName,
			std::string &outIp, USHORT &outPort)
		{
			constexpr const WCHAR *pSection = L"Client";
			std::wstring ip;
			if (!loader.Load(pSection, L"IP", &ip) || ip != L"127.0.0.1")
				return ConfigError(pFileName, L"IP must be 127.0.0.1 (local test only)");
			INT64 number = 0;
			if (!loader.Load(pSection, L"PORT", &number) || number < 1 || number > 65535)
				return ConfigError(pFileName, L"PORT is missing or invalid (1..65535)");
			outIp.assign(ip.begin(), ip.end());
			outPort = static_cast<USHORT>(number);
			return true;
		}
	}

	bool LoadClientConfig(const WCHAR *pFileName, ClientNetworkLib::SClientNetworkSettings &outNetwork,
		SChatClientConfig &outConfig)
	{
		MHLib::utils::CFileLoader loader;
		constexpr const WCHAR *pSection = L"Client";
		SChatClientConfig config;
		if (!LoadConfigFile(pFileName, loader) || !LoadEndpoint(loader, pFileName, config.m_ip, config.m_port))
			return false;
		ClientNetworkLib::SClientNetworkSettings network{};
		const wchar_t *pError = nullptr;
		if (!ClientNetworkLib::SClientNetworkSettings::Load(loader, network, pError))
			return ConfigError(pFileName, pError);

		INT64 number = 0;
		std::wstring connectTimeout;
		if (loader.Load(pSection, L"CONNECT_TIMEOUT_MS", &connectTimeout))
		{
			if (!loader.Load(pSection, L"CONNECT_TIMEOUT_MS", &number) || number < 1 || number > MAXDWORD)
				return ConfigError(pFileName, L"CONNECT_TIMEOUT_MS must be an integer (1..4294967295)");
			config.m_connectTimeoutMilliseconds = static_cast<DWORD>(number);
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
		outNetwork = network;
		outConfig = std::move(config);
		return true;
	}
}
