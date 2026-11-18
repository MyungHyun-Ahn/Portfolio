// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ClientPacketSend.g.h"

namespace Chatting
{
	NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqMessage(const SReqMessage &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ReqMessage) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqLogin(const SReqLogin &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ReqLogin) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *MakePacketReqSectorMove(const SReqSectorMove &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ReqSectorMove) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *MakePacketHeartbeatNoti(const SHeartbeatNoti &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::HeartbeatNoti) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}
}
