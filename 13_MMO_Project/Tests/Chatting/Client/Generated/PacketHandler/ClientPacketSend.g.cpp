// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ClientPacketSend.g.h"

namespace Chatting
{
	std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqMessage(const SReqMessage &packet) noexcept
	{
		auto spBuffer = NetworkLib::CSerializableBuffer<FALSE>::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqMessage) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}

	std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqLogin(const SReqLogin &packet) noexcept
	{
		auto spBuffer = NetworkLib::CSerializableBuffer<FALSE>::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqLogin) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}

	std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketReqSectorMove(const SReqSectorMove &packet) noexcept
	{
		auto spBuffer = NetworkLib::CSerializableBuffer<FALSE>::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::ReqSectorMove) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}

	std::shared_ptr<NetworkLib::CSerializableBuffer<FALSE>> MakePacketHeartbeatNoti(const SHeartbeatNoti &packet) noexcept
	{
		auto spBuffer = NetworkLib::CSerializableBuffer<FALSE>::MakeShared();
		if (spBuffer == nullptr)
		{
			return nullptr;
		}

		*spBuffer << static_cast<std::uint16_t>(EPacketType::HeartbeatNoti) << packet;
		if (!spBuffer->IsValid())
		{
			return nullptr;
		}
		return spBuffer;
	}
}
