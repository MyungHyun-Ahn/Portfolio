// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <utility>
#include <new>
#include "NetworkCommonLibPch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CClientManager.h"
#include "CClient.h"
#include "ClientPacketDispatch.g.h"
#include "CChatNetClient.g.h"

namespace Chatting
{
	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqMessage(const SReqMessage &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqLogin(const SReqLogin &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketReqSectorMove(const SReqSectorMove &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer> MakePacketHeartbeatNoti(const SHeartbeatNoti &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer::MakeShared();
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

	bool CChatNetClient::onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept
	{
		return DispatchClientPacket(message, *this);
	}

	bool CChatNetClient::SendReqMessage(
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
		return this->sendPacket(MakePacketReqMessage(packet));
	}

	bool CChatNetClient::SendReqLogin(
		std::int64_t accountNo,
		const std::wstring &id,
		const std::wstring &nickname,
		const std::array<char, 64> &sessionKey) noexcept
	{
		SReqLogin packet;
		packet.m_accountNo = accountNo;
		packet.m_sessionKey = sessionKey;
		try
		{
			packet.m_id = id;
			packet.m_nickname = nickname;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		return this->sendPacket(MakePacketReqLogin(packet));
	}

	bool CChatNetClient::SendReqSectorMove(
		std::int64_t accountNo,
		std::uint16_t sectorX,
		std::uint16_t sectorY) noexcept
	{
		SReqSectorMove packet;
		packet.m_accountNo = accountNo;
		packet.m_sectorX = sectorX;
		packet.m_sectorY = sectorY;
		return this->sendPacket(MakePacketReqSectorMove(packet));
	}

	bool CChatNetClient::SendNotiHeartbeat(
		std::int64_t accountNo) noexcept
	{
		SHeartbeatNoti packet;
		packet.m_accountNo = accountNo;
		return this->sendPacket(MakePacketHeartbeatNoti(packet));
	}
}
