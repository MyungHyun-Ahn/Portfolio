#pragma once

namespace MHLib::utils
{
	class CFileLoader;
}

namespace ServerNetworkLib
{
	struct SServerNetworkSettings
	{
		std::string ip;
		USHORT port;
		NetworkCommonLib::EPacketMode packetMode;
		INT maxSessionCount;
		INT acceptExCount;
		INT iocpWorkerCount;
		INT iocpConcurrency;
		BYTE packetCode;
		bool useZeroCopy = false;

		// Server 섹션을 검증한다. 파일에 없는 프로토콜·내부 값은 변경하지 않는다.
		[[nodiscard]]
		static bool Load(MHLib::utils::CFileLoader &loader, SServerNetworkSettings &outSettings, const wchar_t *&outError);
	};
}
