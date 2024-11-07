#pragma once
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
		InitializeCriticalSection(&m_Lock);
	}

	~CSession()
	{
		DeleteCriticalSection(&m_Lock);
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend();

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
	CRITICAL_SECTION m_Lock;
};

