// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include <utility>
#include <new>
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CNetClientManager.h"
#include "CNetClient.h"
#include "IClientPacketHandler.g.h"
#include "ClientPacketDispatch.g.h"
#include "CChatNetClient.g.h"

namespace Chatting
{
	std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> MakePacketReqMessage(const SReqMessage &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> MakePacketReqLogin(const SReqLogin &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> MakePacketReqSectorMove(const SReqSectorMove &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::MakeShared();
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

	std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> MakePacketHeartbeatNoti(const SHeartbeatNoti &packet) noexcept
	{
		auto spBuffer = NetworkCommonLib::CSerializableBuffer<FALSE>::MakeShared();
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

	bool CChatNetClient::onRecv(NetworkCommonLib::CSerializableBuffer<FALSE> &message) noexcept
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
		return this->sendNetworkPacket(MakePacketReqMessage(packet));
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
		return this->sendNetworkPacket(MakePacketReqLogin(packet));
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
		return this->sendNetworkPacket(MakePacketReqSectorMove(packet));
	}

	bool CChatNetClient::SendNotiHeartbeat(

		std::int64_t accountNo) noexcept
	{
		SHeartbeatNoti packet;
		packet.m_accountNo = accountNo;
		return this->sendNetworkPacket(MakePacketHeartbeatNoti(packet));
	}
}
