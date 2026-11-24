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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqChange(const SReqChange &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqChange) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqMove(const SReqMove &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqMove) << packet;
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

	bool WorldClient::SendReqChange(
		std::uint64_t mapId,
		std::uint32_t amount) noexcept
	{
		SReqChange packet;
		packet.m_mapId = mapId;
		packet.m_amount = amount;
		return this->sendPacket(MakePacketReqChange(packet));
	}

	bool WorldClient::SendReqMove(
		std::uint64_t targetMapId) noexcept
	{
		SReqMove packet;
		packet.m_targetMapId = targetMapId;
		return this->sendPacket(MakePacketReqMove(packet));
	}
}
