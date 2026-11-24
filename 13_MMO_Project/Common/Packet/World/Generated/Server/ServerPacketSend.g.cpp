// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "ServerNetworkLibPch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"

namespace WorldServer
{
	bool CServerPacketSender::SendPacket(UINT64 sessionId, const ServerNetworkLib::SendPacketPtr &spPacket) noexcept
	{
		auto *pPacket = spPacket.GetRealPtr();
		if (pPacket == nullptr)
			return false;
		sendPacket(sessionId, *pPacket);
		return true;
	}

	bool CServerPacketSender::SendResAdd(
		UINT64 sessionId,
		std::uint64_t mapId,
		std::uint64_t total,
		std::uint16_t result) noexcept
	{
		SResAdd packet;
		packet.m_mapId = mapId;
		packet.m_total = total;
		packet.m_result = result;
		return SendPacket(sessionId, MakePacket(packet));
	}
}
