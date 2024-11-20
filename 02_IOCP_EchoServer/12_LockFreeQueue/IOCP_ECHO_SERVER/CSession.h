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
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend(USHORT wher = 0);

public:
	inline static CSession *Alloc()
	{
		CSession *pSession = s_sSessionPool.Alloc();
		pSession->Clear();
		return pSession;
	}

	inline static void Free(CSession *delSession)
	{
		s_sSessionPool.Free(delSession);
	}

	inline static LONG GetPoolCapacity() { return s_sSessionPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sSessionPool.GetUseCount(); }

private:
	LONG m_bIsValid;

	LONG m_iIOCount = 0;
	LONG m_iSendFlag = FALSE;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;

	char	    m_AcceptBuffer[64];
	WCHAR		m_ClientAddrBuffer[16];
	USHORT		m_ClientPort;
	CRingBuffer m_RecvBuffer;

	CLFQueue<CSerializableBuffer *> m_lfQueue;
	CSerializableBuffer				*m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT];
	LONG							m_iSendCount = 0;

	OverlappedEx m_AcceptExOverlapped;
	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	inline static CLFMemoryPool<CSession> s_sSessionPool = CLFMemoryPool<CSession>(1000, false);

#ifdef POSTSEND_LOST_DEBUG
	UINT64 sendIndex = 0;
	SendDebug sendDebug[65535];
#endif
};

