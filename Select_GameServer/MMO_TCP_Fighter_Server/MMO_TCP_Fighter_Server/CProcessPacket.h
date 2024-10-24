#pragma once
class CProcessPacketInterface
{
public:
	virtual bool Process(CSession *session) = 0;
	virtual bool ConsumePacket(PACKET_CODE code, UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message) = 0;
	virtual bool PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message) = 0;

	void SetServer(CServerCore *server) { m_Server = server; }

protected:
	CServerCore *m_Server;
};

class CProcessPacket : public CProcessPacketInterface
{
public:
	bool Process(CSession *session)
	{
		int size = session->m_RecvBuffer.GetUseSize();

		while (size > 0)
		{
			PacketHeader header;
			int ret = session->m_RecvBuffer.Peek((char *)&header, sizeof(PacketHeader));
			// PacketHeader + PacketType + size
			if (session->m_RecvBuffer.GetUseSize() < sizeof(PacketHeader) + 1 + header.bySize)
				break;

			session->m_RecvBuffer.MoveFront(ret);

			CSerializableBuffer *buffer = new CSerializableBuffer;
			ret = session->m_RecvBuffer.Dequeue(buffer->GetContentBufferPtr(), header.bySize + 1);

			if (m_Server->OnRecv(session->m_iId, buffer))
				return false;

			delete buffer;
		}

		return true;
	}

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

extern CProcessPacket g_ProcessPacket;
extern CProcessPacketInterface *g_pProcessPacket;
