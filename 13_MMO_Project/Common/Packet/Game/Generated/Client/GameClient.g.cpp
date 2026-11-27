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
#include "GameClient.g.h"

namespace Game
{
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

	bool GameClient::onRecv(NetworkCommonLib::CSerializableBuffer &message) noexcept
	{
		return DispatchClientPacket(message, *this);
	}

	bool GameClient::SendReqLogin(
		const std::wstring &loginId) noexcept
	{
		SReqLogin packet;
		try
		{
			packet.m_loginId = loginId;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		return this->sendPacket(MakePacketReqLogin(packet));
	}
}
