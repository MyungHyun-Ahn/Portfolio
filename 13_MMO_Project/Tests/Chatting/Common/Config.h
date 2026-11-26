#pragma once

namespace Chatting
{
	struct SChatClientConfig
	{
		std::string m_ip = "127.0.0.1";
		USHORT m_port = kDefaultPort;
		DWORD m_connectTimeoutMilliseconds = 5000;
		int m_recvBufferBytes = 4096;
		NetworkCommonLib::EPacketMode m_packetMode = NetworkCommonLib::EPacketMode::Net;
		INT64 m_accountNo = 0;
		std::wstring m_name = L"player";
		WORD m_sectorX = 10;
		WORD m_sectorY = 10;
	};

	[[nodiscard]] 
	bool GetExecutableDirectory(std::wstring &outDirectory);
	[[nodiscard]] 
	bool CheckConfigFile(const WCHAR *pPath);

	// 설정 오류를 콘솔에 남기고 실패를 반환한다.
	bool ConfigError(const WCHAR *pFileName, const WCHAR *pReason);
	// 실행 파일 옆의 UTF-16LE BOM 설정을 파싱한다.
	[[nodiscard]]
	bool LoadConfigFile(const WCHAR *pFileName, MHLib::utils::CFileLoader &outLoader);
	// 리슨 또는 접속에 사용할 주소·포트·모드를 읽는다.
	[[nodiscard]]
	bool LoadEndpoint(MHLib::utils::CFileLoader &loader, const WCHAR *pFileName, const WCHAR *pSection,
		std::string &outIp, USHORT &outPort, NetworkCommonLib::EPacketMode &outPacketMode);

	// 실행 파일 옆 클라이언트 설정을 읽는다. 실패하면 outConfig는 변경하지 않는다.
	[[nodiscard]]
	bool LoadClientConfig(const WCHAR *pFileName, SChatClientConfig &outConfig);
}
