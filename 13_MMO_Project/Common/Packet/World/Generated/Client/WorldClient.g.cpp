// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <utility>
#include "NetworkCommonLibPch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CClientManager.h"
#include "CClient.h"
#include "ClientPacketDispatch.g.h"
#include "WorldClient.g.h"

namespace WorldServer
{
	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqAdd(const SReqAdd &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqAdd) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}

	bool WorldClient::onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept
	{
		return DispatchClientPacket(message, *this);
	}

	bool WorldClient::SendReqAdd(
		std::uint64_t mapId,
		std::uint32_t amount) noexcept
	{
		SReqAdd packet;
		packet.m_mapId = mapId;
		packet.m_amount = amount;
		return this->sendPacket(MakePacketReqAdd(packet));
	}
}
