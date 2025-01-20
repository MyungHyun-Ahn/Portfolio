#pragma once

#define MEM_LOG_MAX 1000

struct MemLog
{
	BYTE oper;
	LONG ioCount;
};

class CNetSession
{
public:
	friend class CNetServer;

	CNetSession() noexcept
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
	{

	}

	CNetSession(SOCKET socket, UINT64 sessionID) noexcept
		: m_sSessionSocket(socket)
		, m_uiSessionID(sessionID)
		, m_AcceptExOverlapped(IOOperation::ACCEPTEX)
		, m_RecvOverlapped(IOOperation::RECV)
		, m_SendOverlapped(IOOperation::SEND)
	{

	}

	~CNetSession() noexcept
	{
	}

	void Init(UINT64 sessionID) noexcept
	{
		m_uiSessionID = sessionID;
		InterlockedExchange(&m_iSendFlag, FALSE);
		InterlockedExchange(&m_iCacelIoCalled, FALSE );
		InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
	}

	void Init(SOCKET socket, UINT64 sessionID) noexcept
	{
		m_sSessionSocket = socket;
		m_uiSessionID = sessionID;
		InterlockedExchange(&m_iSendFlag, FALSE);
		InterlockedExchange(&m_iCacelIoCalled, FALSE);
		InterlockedAnd(&m_iIOCountAndRelease, ~RELEASE_FLAG);
	}

	void Clear() noexcept
	{
		// ReleaseSession 당시에 남아있는 send 링버퍼를 확인
		// * 남아있는 경우가 확인됨
		// * 남은 직렬화 버퍼를 할당 해제하고 세션 삭제

		if (m_pDelayedBuffer != nullptr)
		{
			CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
		}

		int sendCount = m_iSendCount;
		m_iSendCount = 0;
		for (int count = 0; count < sendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// 말도 안되는 상황
				CCrashDump::Crash();
			}

			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(pBuffer);
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

			m_pRecvBuffer = nullptr;
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
	}

	void RecvCompleted(int size) noexcept;

	bool SendPacket(CSerializableBuffer<FALSE> *message) noexcept;
	void SendCompleted(int size) noexcept;

	bool PostRecv() noexcept;
	bool PostSend(BOOL isCompleted = FALSE) noexcept;

public:
	inline static CNetSession *Alloc() noexcept
	{
		CNetSession *pSession = s_sSessionPool.Alloc();
		return pSession;
	}

	inline static void Free(CNetSession *delSession) noexcept
	{
		delSession->Clear();
		s_sSessionPool.Free(delSession);
	}

	inline static LONG GetPoolCapacity() noexcept { return s_sSessionPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return s_sSessionPool.GetUseCount(); }

private:
	LONG m_iIOCountAndRelease = RELEASE_FLAG;
	LONG m_iSendFlag = FALSE;
	LONG m_iCacelIoCalled = FALSE;

	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;

	char	    m_AcceptBuffer[64];
	WCHAR		m_ClientAddrBuffer[16];
	USHORT		m_ClientPort;
	// CRingBuffer m_RecvBuffer;
	CRecvBuffer *m_pRecvBuffer = nullptr;
	// 최대 무조건 1개 -> 있거나 없거나
	CSerializableBufferView<FALSE> *m_pDelayedBuffer = nullptr;

	CLFQueue<CSerializableBuffer<FALSE> *> m_lfSendBufferQueue;
	CSerializableBuffer<FALSE> *m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT];
	LONG							m_iSendCount = 0;

	OverlappedEx m_AcceptExOverlapped;
	OverlappedEx m_RecvOverlapped;
	OverlappedEx m_SendOverlapped;

	MemLog log[MEM_LOG_MAX];
	LONG logIndex = 0;

	inline static CTLSMemoryPoolManager<CNetSession, 16, 4> s_sSessionPool = CTLSMemoryPoolManager<CNetSession, 16, 4>();
	inline static LONG RELEASE_FLAG = 0x80000000;
};

