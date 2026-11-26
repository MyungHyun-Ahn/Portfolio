#pragma once

namespace ClientNetworkLib
{
	struct SClientNetworkSettings;
}

namespace Chatting
{
	struct SChatClientConfig
	{
		std::string m_ip = "127.0.0.1";
		USHORT m_port = kDefaultPort;
		DWORD m_connectTimeoutMilliseconds = 5000;
		INT64 m_accountNo = 0;
		std::wstring m_name = L"player";
		WORD m_sectorX = 10;
		WORD m_sectorY = 10;
	};

	// 클라이언트 설정을 한 번 읽고 두 설정을 모두 검증한 뒤 넘긴다.
	[[nodiscard]]
	bool LoadClientConfig(const WCHAR *pFileName, ClientNetworkLib::SClientNetworkSettings &outNetwork,
		SChatClientConfig &outConfig);
}
