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

	CSession()
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
		, m_bIsValid(FALSE)
	{

	}

	CSession(SOCKET socket, UINT64 sessionID)
		: m_sSessionSocket(socket)
		, m_uiSessionID(sessionID)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
		, m_bIsValid(TRUE)
	{

	}

	~CSession()
	{
	}

	void Init(UINT64 sessionID)
	{
		m_uiSessionID = sessionID;
		m_iSendFlag = FALSE;
		m_bIsValid = TRUE;
	}

	void Init(SOCKET socket, UINT64 sessionID)
	{
		m_sSessionSocket = socket;
		m_uiSessionID = sessionID;
		m_iSendFlag = FALSE;
		m_bIsValid = TRUE;
	}

	void Clear()
	{
		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		m_iIOCount = 0;
		m_iSendCount = 0;
		m_RecvBuffer.Clear();
		m_SendBuffer.Clear();
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend(USHORT wher = 0);

public:
	inline static CSession *Alloc()
	{
		CObjectPool<CSession>::Lock();
		CSession *pSession = s_sSessionPool.Alloc();
		CObjectPool<CSession>::UnLock();
		pSession->Clear();
		return pSession;
	}

	inline static void Free(CSession *delSession)
	{
		CObjectPool<CSession>::Lock();
		s_sSessionPool.Free(delSession);
		CObjectPool<CSession>::UnLock();
	}

private:
	LONG m_bIsValid;

	LONG m_iIOCount = 0;
	LONG m_iSendFlag = FALSE;
	LONG m_iSendCount = 0;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;

	char	    m_AcceptBuffer[64];
	WCHAR		m_ClientAddrBuffer[16];
	USHORT		m_ClientPort;
	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

	OverlappedEx m_AcceptExOverlapped;
	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	inline static CObjectPool<CSession> s_sSessionPool = CObjectPool<CSession>(1000, false);

#ifdef POSTSEND_LOST_DEBUG
	UINT64 sendIndex = 0;
	SendDebug sendDebug[65535];
#endif
};

