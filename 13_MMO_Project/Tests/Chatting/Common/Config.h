#pragma once

namespace Chatting
{
	struct SChatConfig
	{
		std::string m_ip = "127.0.0.1";
		MHLib::utils::ELogLevel m_consoleLogLevel = MHLib::utils::ELogLevel::Info;
		MHLib::utils::ELogLevel m_fileLogLevel = MHLib::utils::ELogLevel::Debug;
		std::chrono::milliseconds m_logPollInterval{MHLib::utils::CLogger::kDefaultPollInterval};
		USHORT m_port = kDefaultPort;
		USHORT m_maxSessionCount = 256;
		DWORD m_connectTimeoutMilliseconds = 5000;
		int m_recvBufferBytes = 4096;
		INT64 m_accountNo = 0;
		std::wstring m_name = L"player";
		WORD m_sectorX = 10;
		WORD m_sectorY = 10;
	};

	[[nodiscard]] 
	bool GetExecutableDirectory(std::wstring &outDirectory);
	[[nodiscard]] 
	bool CheckConfigFile(const WCHAR *pPath);

	// 실행 파일 옆 설정을 읽는다. 실패하면 outConfig는 변경하지 않는다.
	bool LoadConfig(const WCHAR *pFileName, bool isServer, SChatConfig &outConfig);
}
