#pragma once
class CProcessPacketInterface
{
public:
	virtual bool	ConsumePacket(PACKET_CODE code, UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool	PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message) = 0;
	inline void		SetServer(CServerCore *server) { m_pServerCore = server; }

protected:
	CServerCore *m_pServerCore;

};

class CGameProcessPacket : public CProcessPacketInterface
{
public:
	bool ConsumePacket(PACKET_CODE code, UINT64 sessionId, CSerializableBuffer *message)
	{
		switch (code)
		{
		case PACKET_CODE::CSMoveStart:
			return PacketProcCSMoveStart(sessionId, message);
		case PACKET_CODE::CSMoveStop:
			return PacketProcCSMoveStop(sessionId, message);
		case PACKET_CODE::CSAttack1:
			return PacketProcCSAttack1(sessionId, message);
		case PACKET_CODE::CSAttack2:
			return PacketProcCSAttack2(sessionId, message);
		case PACKET_CODE::CSAttack3:
			return PacketProcCSAttack3(sessionId, message);
		case PACKET_CODE::CSEcho:
			return PacketProcCSEcho(sessionId, message);
		default:
			break;
		}

		return false;
	}
	bool PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message);
	bool PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message);
	bool PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message);
	bool PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message);
	bool PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message);
	bool PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message);
};

extern CGameProcessPacket g_ProcessPacket;
extern CProcessPacketInterface *g_pProcessPacket;
