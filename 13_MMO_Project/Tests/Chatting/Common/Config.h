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
		std::string m_metricsBindAddress;
		USHORT m_metricsPort = 0;
		USHORT m_maxSessionCount = 256;
		int m_acceptExCount = 4;
		int m_iocpWorkerCount = 2;
		int m_iocpConcurrency = 2;
		int m_contentsWorkerCount = 2;
		int m_contentsFps = 25;
		int m_logQueueCapacity = 1024;
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
