// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "ServerNetworkLibPch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"

namespace Game
{
	bool CServerPacketSender::SendPacket(UINT64 sessionId, const ServerNetworkLib::SendPacketPtr &spPacket) noexcept
	{
		auto *pPacket = spPacket.GetRealPtr();
		if (pPacket == nullptr)
			return false;
		sendPacket(sessionId, *pPacket);
		return true;
	}

	bool CServerPacketSender::SendResLogin(
		UINT64 sessionId,
		::GameData::EResultCode result,
		const SCharacterData &characterData) noexcept
	{
		SResLogin packet;
		packet.m_result = result;
		packet.m_characterData = characterData;
		return SendPacket(sessionId, MakePacket(packet));
	}
}
