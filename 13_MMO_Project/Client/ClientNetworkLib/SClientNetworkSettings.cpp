#include "ClientNetworkLibPch.h"
#include "SClientNetworkSettings.h"

namespace ClientNetworkLib
{
	bool SClientNetworkSettings::Load(MHLib::utils::CFileLoader &loader, SClientNetworkSettings &outSettings,
		const wchar_t *&outError)
	{
		constexpr const wchar_t *kSection = L"ClientNetwork";
		INT64 workerCount = 0;
		outError = L"ClientNetwork: WORKER_COUNT must be an integer (1..16)";
		if (!loader.Load(kSection, L"WORKER_COUNT", &workerCount) || workerCount < 1 || workerCount > 16)
			return false;

		constexpr int kMinRecvBufferBytes = static_cast<int>(NetworkCommonLib::CSerializableBuffer::EBufferDefine::PacketMaxSize);
		INT64 recvBufferBytes = 0;
		outError = L"ClientNetwork: RECV_BUFFER_BYTES must fit a full Net packet and the ring's cursor range";
		if (!loader.Load(kSection, L"RECV_BUFFER_BYTES", &recvBufferBytes)
			|| recvBufferBytes < kMinRecvBufferBytes || recvBufferBytes > NetworkCommonLib::kMaxRecvBufferBytes)
			return false;

		std::wstring mode;
		outError = kPacketModeError;
		if (!loader.Load(kSection, L"PACKET_MODE", &mode) || (mode != L"Net" && mode != L"Lan"))
			return false;

		outSettings.workerCount = static_cast<DWORD>(workerCount);
		outSettings.recvBufferBytes = static_cast<int>(recvBufferBytes);
		outSettings.packetMode = mode == L"Lan" ? NetworkCommonLib::EPacketMode::Lan : NetworkCommonLib::EPacketMode::Net;
		outError = nullptr;
		return true;
	}
}