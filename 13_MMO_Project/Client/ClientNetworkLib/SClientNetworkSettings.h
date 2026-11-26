#pragma once

namespace MHLib::utils
{
	class CFileLoader;
}

namespace ClientNetworkLib
{
	struct SClientNetworkSettings
	{
		static constexpr wchar_t kPacketModeError[] = L"ClientNetwork: PACKET_MODE must be Net or Lan";

		DWORD workerCount = 1;
		int recvBufferBytes = 4096;
		NetworkCommonLib::EPacketMode packetMode = NetworkCommonLib::EPacketMode::Net;

		// ClientNetwork 섹션을 검증하고 성공했을 때만 설정을 채운다.
		[[nodiscard]]
		static bool Load(MHLib::utils::CFileLoader &loader, SClientNetworkSettings &outSettings, const wchar_t *&outError);
	};
}