// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <new>
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"

namespace Chatting
{
	NetworkCommonLib::CSerializableBuffer<FALSE> *MakePacketResMessage(const SResMessage &packet) noexcept
	{
		auto *pBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResMessage) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkCommonLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *MakePacketMessageNoti(const SMessageNoti &packet) noexcept
	{
		auto *pBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::MessageNoti) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkCommonLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *MakePacketResLogin(const SResLogin &packet) noexcept
	{
		auto *pBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResLogin) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkCommonLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *MakePacketResSectorMove(const SResSectorMove &packet) noexcept
	{
		auto *pBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::Alloc();
		if (pBuffer == nullptr)
		{
			return nullptr;
		}

		*pBuffer << static_cast<std::uint16_t>(EPacketType::ResSectorMove) << packet;
		if (!pBuffer->IsValid())
		{
			NetworkCommonLib::CSerializableBuffer<FALSE>::Free(pBuffer);
			return nullptr;
		}
		return pBuffer;
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *CServerPacketSender::submitPreparedPacket(
		UINT64 sessionId, EPacketType type, NetworkCommonLib::CSerializableBuffer<FALSE> *pPacket) noexcept
	{
		if (pPacket == nullptr)
		{
			return nullptr;
		}
		pPacket->IncreaseRef();
		sendPacket(sessionId, type, *pPacket);
		return pPacket;
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *CServerPacketSender::SendResMessage(

		UINT64 sessionId,
		std::int64_t accountNo,
		const std::wstring &id,
		const std::wstring &nickname,
		const std::wstring &message) noexcept
	{
		SResMessage packet;
		packet.m_accountNo = accountNo;
		try
		{
			packet.m_id = id;
			packet.m_nickname = nickname;
			packet.m_message = message;
		}
		catch (const std::bad_alloc &)
		{
			return nullptr;
		}
		return this->submitPreparedPacket(sessionId, EPacketType::ResMessage,
			MakePacketResMessage(packet));
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *CServerPacketSender::SendNotiMessage(

		UINT64 sessionId,
		std::int64_t accountNo,
		const std::wstring &id,
		const std::wstring &nickname,
		const std::wstring &message) noexcept
	{
		SMessageNoti packet;
		packet.m_accountNo = accountNo;
		try
		{
			packet.m_id = id;
			packet.m_nickname = nickname;
			packet.m_message = message;
		}
		catch (const std::bad_alloc &)
		{
			return nullptr;
		}
		return this->submitPreparedPacket(sessionId, EPacketType::MessageNoti,
			MakePacketMessageNoti(packet));
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *CServerPacketSender::SendResLogin(

		UINT64 sessionId,
		std::uint8_t status,
		std::int64_t accountNo) noexcept
	{
		SResLogin packet;
		packet.m_status = status;
		packet.m_accountNo = accountNo;
		return this->submitPreparedPacket(sessionId, EPacketType::ResLogin,
			MakePacketResLogin(packet));
	}

	NetworkCommonLib::CSerializableBuffer<FALSE> *CServerPacketSender::SendResSectorMove(

		UINT64 sessionId,
		std::int64_t accountNo,
		std::uint16_t sectorX,
		std::uint16_t sectorY) noexcept
	{
		SResSectorMove packet;
		packet.m_accountNo = accountNo;
		packet.m_sectorX = sectorX;
		packet.m_sectorY = sectorY;
		return this->submitPreparedPacket(sessionId, EPacketType::ResSectorMove,
			MakePacketResSectorMove(packet));
	}
}
