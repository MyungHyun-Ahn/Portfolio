#pragma once
class CChatProcessPacketInterface
{
public:
	virtual bool	ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual bool 	PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual bool 	PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual bool 	PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual bool 	PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	void			SetChatServer(CChatServer *chatServer) noexcept { m_pChatServer = chatServer; }

protected:
	CChatServer *m_pChatServer = nullptr;
};

class CChatProcessPacket : public CChatProcessPacketInterface
{
	
	bool ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override
	{
		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			InterlockedIncrement(&g_monitor.m_loginReq);
			return PacketProcReqLogin(sessionId, message);
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			InterlockedIncrement(&g_monitor.m_sectorMoveReq);
			return PacketProcReqSectorMove(sessionId, message);
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
			InterlockedIncrement(&g_monitor.m_chatMsgReq);
			return PacketProcReqMessage(sessionId, message);
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			return PacketProcReqHeartBeat(sessionId, message);
		default:
			break;
		}

		return false;
	}

	bool PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	bool PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	bool PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
	bool PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
};

