// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "CSerializableBuffer.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IServerPacketHandler.g.h"
#include "ServerPacketDispatch.g.h"

namespace Chatting
{
	bool DispatchServerPacket(std::uint16_t type, UINT64 sessionId,
NetworkCommonLib::CSerializableBuffer<FALSE> &message, IServerPacketHandler &handler) noexcept
	{
		switch (static_cast<EPacketType>(type))
		{
		case EPacketType::ReqMessage:
		{
			SReqMessage packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleReqMessage(sessionId, packet);
		}
		case EPacketType::ReqLogin:
		{
			SReqLogin packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleReqLogin(sessionId, packet);
		}
		case EPacketType::ReqSectorMove:
		{
			SReqSectorMove packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleReqSectorMove(sessionId, packet);
		}
		case EPacketType::HeartbeatNoti:
		{
			SHeartbeatNoti packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0
				&& handler.HandleHeartbeatNoti(sessionId, packet);
		}
		default:
			return false;
		}
	}
}
