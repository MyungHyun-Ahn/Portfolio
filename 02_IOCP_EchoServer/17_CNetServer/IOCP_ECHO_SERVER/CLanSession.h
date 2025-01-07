#pragma once

//struct SendDebug
//{
//	UINT64 m_Index;
//	USHORT m_ThreadId;
//	USHORT m_Where;		// 0 - SendCompleted
//						// 1 - SendPacket
//						// 2 - RecvCompleted
//						// ff - sendCompleted
//	USHORT m_IsSuccess;
//	USHORT m_Why;		// ���н� ����
//						// 1 - UseSize
//						// 2 - SendFlag
//};

class CLanSession
{
public:
	friend class CLanServer;

	CLanSession()
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
	{

	}

	CLanSession(SOCKET socket, UINT64 sessionID)
		: m_sSessionSocket(socket)
		, m_uiSessionID(sessionID)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
	{

	}

	~CLanSession()
	{
	}

	void Init(UINT64 sessionID)
	{
		m_uiSessionID = sessionID;
		m_iSendFlag = FALSE;
	}

	void Init(SOCKET socket, UINT64 sessionID)
	{
		m_sSessionSocket = socket;
		m_uiSessionID = sessionID;
		m_iSendFlag = FALSE;
	}

	void Clear()
	{
		// ReleaseSession ��ÿ� �����ִ� send �����۸� Ȯ��
		// * �����ִ� ��찡 Ȯ�ε�
		// * ���� ����ȭ ���۸� �Ҵ� �����ϰ� ���� ����


		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<TRUE>::Free(m_arrPSendBufs[count]);
			}
		}

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<TRUE> *pBuffer;
			// ������ ��
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// ���� �ȵǴ� ��Ȳ
				CCrashDump::Crash();
			}

			// RefCount�� ���߰� 0�̶�� ���� �� ����
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer<TRUE>::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue is not empty Error");
		}

		if (m_pRecvBuffer != nullptr)
		{
			if (m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		m_iIOCountAndRelease = RELEASE_FLAG;
		m_iSendCount = 0;
		m_pRecvBuffer = nullptr;
	}

	void RecvCompleted(int size);

	bool SendPacket(CSerializableBuffer<TRUE> *message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend(BOOL isCompleted = FALSE);

public:
	inline static CLanSession *Alloc()
	{
		CLanSession *pSession = s_sSessionPool.Alloc();
		return pSession;
	}

	inline static void Free(CLanSession *delSession)
	{
		delSession->Clear();
		s_sSessionPool.Free(delSession);
	}

	inline static LONG GetPoolCapacity() { return s_sSessionPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sSessionPool.GetUseCount(); }

private:
	LONG m_iIOCountAndRelease = RELEASE_FLAG;
	LONG m_iSendFlag = FALSE;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;

	char	    m_AcceptBuffer[64];
	WCHAR		m_ClientAddrBuffer[16];
	USHORT		m_ClientPort;
	// CRingBuffer m_RecvBuffer;
	CRecvBuffer *m_pRecvBuffer = nullptr;
	// �ִ� ������ 1�� -> �ְų� ���ų�
	CSerializableBufferView<TRUE> *m_pDelayedBuffer = nullptr;

	CLFQueue<CSerializableBuffer<TRUE> *> m_lfSendBufferQueue;
	CSerializableBuffer<TRUE> *m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT];
	LONG							m_iSendCount = 0;

	OverlappedEx m_AcceptExOverlapped;
	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	inline static CTLSMemoryPoolManager<CLanSession, 16, 4> s_sSessionPool = CTLSMemoryPoolManager<CLanSession, 16, 4>();
	inline static LONG RELEASE_FLAG = 0x80000000;

#ifdef POSTSEND_LOST_DEBUG
	UINT64 sendIndex = 0;
	SendDebug sendDebug[65535];
#endif
};

