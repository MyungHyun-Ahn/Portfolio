#pragma once
class CSession
{
public:
	friend class CLanClients;

	CSession()
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
		, m_ConnectExOverlapped(IOOperation::CONNECTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
		, m_bIsValid(FALSE)
	{

	}

	CSession(SOCKET socket, UINT64 sessionID)
		: m_sSessionSocket(socket)
		, m_uiSessionID(sessionID)
		, m_ConnectExOverlapped(IOOperation::CONNECTEX)
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
		m_bIsConnected = FALSE;
	}

	void Init(SOCKET socket, UINT64 sessionID)
	{
		m_sSessionSocket = socket;
		m_uiSessionID = sessionID;
		m_iSendFlag = FALSE;
		m_bIsValid = TRUE;
		m_bIsConnected = FALSE;
	}

	void Clear()
	{
		// ReleaseSession 당시에 남아있는 send 링버퍼를 확인
		// * 남아있는 경우가 확인됨
		// * 남은 직렬화 버퍼를 할당 해제하고 세션 삭제


		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer::Free(m_arrPSendBufs[count]);
			}
		}

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
				CCrashDump::Crash();
			}

			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"LFQueue is not empty Error");
		}

		if (m_pRecvBuffer != nullptr)
		{
			if (m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_bIsConnected = FALSE;
		m_uiSessionID = 0;
		m_iIOCount = 0;
		m_iSendCount = 0;
		m_pRecvBuffer = nullptr;
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend(BOOL isCompleted = FALSE);

public:
	inline static CSession *Alloc()
	{
		CSession *pSession = s_sSessionPool.Alloc();
		return pSession;
	}

	inline static void Free(CSession *delSession)
	{
		delSession->Clear();
		s_sSessionPool.Free(delSession);
	}

	inline static LONG GetPoolCapacity() { return s_sSessionPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sSessionPool.GetUseCount(); }


private:
	LONG m_bIsValid = FALSE;
	LONG m_bIsConnected = FALSE;

	LONG m_iIOCount = 0;
	LONG m_iSendFlag = FALSE;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;
	
	SOCKADDR_IN m_SockAddr;

	CRecvBuffer *m_pRecvBuffer = nullptr;
	CSerializableBufferView *m_pDelayedBuffer = nullptr;

	CLFQueue<CSerializableBuffer *> m_lfSendBufferQueue;
	CSerializableBuffer				*m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT];
	LONG							m_iSendCount = 0;

	OverlappedEx m_ConnectExOverlapped;
	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	inline static CLFMemoryPool<CSession> s_sSessionPool = CLFMemoryPool<CSession>(1000, false);
};

