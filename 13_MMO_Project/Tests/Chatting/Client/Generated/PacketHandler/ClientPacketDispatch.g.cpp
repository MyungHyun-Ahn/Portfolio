// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "ClientPacketDispatch.g.h"

namespace Chatting
{
	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer<FALSE> &message, IClientPacketHandler &handler) noexcept
	{
		std::uint16_t type{};
		message >> type;
		if (!message.IsValid())
		{
			return false;
		}

		switch (static_cast<EPacketType>(type))
		{
		case EPacketType::ResMessage:
		{
			SResMessage packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleResMessage(packet);
		}
		case EPacketType::MessageNoti:
		{
			SMessageNoti packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleMessageNoti(packet);
		}
		case EPacketType::ResLogin:
		{
			SResLogin packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleResLogin(packet);
		}
		case EPacketType::ResSectorMove:
		{
			SResSectorMove packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleResSectorMove(packet);
		}
		default:
			return false;
		}
	}
}
