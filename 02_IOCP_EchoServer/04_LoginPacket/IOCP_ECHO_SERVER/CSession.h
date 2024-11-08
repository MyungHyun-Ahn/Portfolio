#pragma once

struct SendDebug
{
	UINT64 m_Index;
	USHORT m_ThreadId;
	USHORT m_Where;		// 0 - SendCompleted
						// 1 - SendPacket
						// 2 - RecvCompleted
						// ff - sendCompleted
	USHORT m_IsSuccess;
	USHORT m_Why;		// 실패시 이유
						// 1 - UseSize
						// 2 - SendFlag
};

class CSession
{
public:
	friend class CLanServer;

	CSession(SOCKET socket, UINT64 sessionID)
		: m_sSessionSocket(socket)
		, m_uiSessionID(sessionID)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
		, m_bIsValid(TRUE)
	{
	}

	~CSession()
	{
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend(USHORT wher = 0);

private:
	BOOL m_bIsValid;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;

	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	LONG m_iIOCount = 0;
	LONG m_iSendFlag = FALSE;

#ifdef POSTSEND_LOST_DEBUG
	UINT64 sendIndex = 0;
	SendDebug sendDebug[65535];
#endif
};

