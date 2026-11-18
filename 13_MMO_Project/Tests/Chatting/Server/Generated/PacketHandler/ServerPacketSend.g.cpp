// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"

namespace Chatting
{
	NetworkLib::CSerializableBuffer<FALSE> *MakePacketResMessage(const SResMessage &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResMessage) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *MakePacketResLogin(const SResLogin &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResLogin) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *MakePacketResSectorMove(const SResSectorMove &packet) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResSectorMove) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}
}
