#pragma once
// class CAuthProcessPacketInterface
// {
// public:
// 	virtual bool	ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
// 	virtual bool 	PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
// 	virtual bool 	PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
// 	virtual bool 	PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
// 	virtual bool 	PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
// 	void			SetChatServer(CAuthContent *authContent) noexcept { m_pAuthContent = authContent; }
// 
// protected:
// 	CAuthContent *m_pAuthContent= nullptr;
// };
// 
// class CAuthProcessPacket : public CAuthProcessPacket
// {
// public:
// 	bool ConsumPacket(en_PACKET_TYPE type, UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override
// 	{
// 		switch (type)
// 		{
// 		case en_PACKET_CS_GAME_REQ_LOGIN:
// 			return PacketProcReqLogin(sessionId, message);
// 		default:
// 			break;
// 		}
// 
// 		return false;
// 	}
// 
// 	bool PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
// 	bool PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
// 	bool PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
// 	bool PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept override;
// };

