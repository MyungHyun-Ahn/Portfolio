// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <utility>
#include <new>
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

	bool CClientPacketSender::SendReqMessage(

		std::int64_t accountNo,
		const std::wstring &message) noexcept
	{
		SReqMessage packet;
		packet.m_accountNo = accountNo;
		try
		{
			packet.m_message = message;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		auto spPacket = MakePacketReqMessage(packet);
		return spPacket != nullptr && this->sendPacket(std::move(spPacket));
	}

	bool CClientPacketSender::SendReqLogin(

		std::int64_t accountNo,
		const std::array<WCHAR, 20> &id,
		const std::array<WCHAR, 20> &nickname,
		const std::array<char, 64> &sessionKey) noexcept
	{
		SReqLogin packet;
		packet.m_accountNo = accountNo;
		packet.m_id = id;
		packet.m_nickname = nickname;
		packet.m_sessionKey = sessionKey;
		auto spPacket = MakePacketReqLogin(packet);
		return spPacket != nullptr && this->sendPacket(std::move(spPacket));
	}

	bool CClientPacketSender::SendReqSectorMove(

		std::int64_t accountNo,
		std::uint16_t sectorX,
		std::uint16_t sectorY) noexcept
	{
		SReqSectorMove packet;
		packet.m_accountNo = accountNo;
		packet.m_sectorX = sectorX;
		packet.m_sectorY = sectorY;
		auto spPacket = MakePacketReqSectorMove(packet);
		return spPacket != nullptr && this->sendPacket(std::move(spPacket));
	}

	bool CClientPacketSender::SendNotiHeartbeat(

		std::int64_t accountNo) noexcept
	{
		SHeartbeatNoti packet;
		packet.m_accountNo = accountNo;
		auto spPacket = MakePacketHeartbeatNoti(packet);
		return spPacket != nullptr && this->sendPacket(std::move(spPacket));
	}
}
