// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <new>
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"

namespace Chatting
{
	bool CServerPacketSender::SendPacket(UINT64 sessionId, const ServerNetworkLib::SendPacketPtr &spPacket) noexcept
	{
		auto *pPacket = spPacket.GetRealPtr();
		if (pPacket == nullptr)
			return false;
		sendPacket(sessionId, *pPacket);
		return true;
	}

	bool CServerPacketSender::SendResMessage(

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
			return false;
		}
		return SendPacket(sessionId, MakePacket(packet));
	}

	bool CServerPacketSender::SendNotiMessage(

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
			return false;
		}
		return SendPacket(sessionId, MakePacket(packet));
	}

	bool CServerPacketSender::SendResLogin(

		UINT64 sessionId,
		std::uint8_t status,
		std::int64_t accountNo) noexcept
	{
		SResLogin packet;
		packet.m_status = status;
		packet.m_accountNo = accountNo;
		return SendPacket(sessionId, MakePacket(packet));
	}

	bool CServerPacketSender::SendResSectorMove(

		UINT64 sessionId,
		std::int64_t accountNo,
		std::uint16_t sectorX,
		std::uint16_t sectorY) noexcept
	{
		SResSectorMove packet;
		packet.m_accountNo = accountNo;
		packet.m_sectorX = sectorX;
		packet.m_sectorY = sectorY;
		return SendPacket(sessionId, MakePacket(packet));
	}
}
