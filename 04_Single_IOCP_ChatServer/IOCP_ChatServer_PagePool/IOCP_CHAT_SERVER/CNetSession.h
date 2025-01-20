#pragma once

class CNetSession
{
public:
	friend class CNetServer;

	CNetSession() noexcept
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
	{
		// + 0 : AcceptEx
		// + 1 : Recv
		// + 2 : Send
		m_pMyOverlappedStartAddr = g_OverlappedAlloc.Alloc();
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
		// ReleaseSession ��ÿ� �����ִ� send �����۸� Ȯ��
		// * �����ִ� ��찡 Ȯ�ε�
		// * ���� ����ȭ ���۸� �Ҵ� �����ϰ� ���� ����

		if (m_pDelayedBuffer != nullptr)
		{
			CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
		}

		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
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
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		m_pRecvBuffer = nullptr;
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
	// �е� ����ؼ� ���� ũ�� ����ȭ
	// + Interlock ����ϴ� �������� ĳ�ö��� �������
	// Release + IoCount
	LONG m_iIOCountAndRelease = RELEASE_FLAG;
	LONG m_iSendCount = 0;
	CRecvBuffer *m_pRecvBuffer = nullptr;
	SOCKET m_sSessionSocket;
	UINT64 m_uiSessionID;
	WCHAR		m_ClientAddrBuffer[16];
	// --- 64

	char	    m_AcceptBuffer[64];
	// --- 128

	LONG		m_iSendFlag = FALSE;
	USHORT		m_ClientPort;
	char		m_dummy01[2]; // �е� ����
	// �ִ� ������ 1�� -> �ְų� ���ų�
	CSerializableBufferView<FALSE> *m_pDelayedBuffer = nullptr;
	CLFQueue<CSerializableBuffer<FALSE> *> m_lfSendBufferQueue;
	// m_pMyOverlappedStartAddr
	//  + 0 : ACCEPTEX
	//  + 1 : RECV
	//  + 2 : SEND
	OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
	CSerializableBuffer<FALSE> *m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT]; // 8 * 32 = 256

	LONG m_iCacelIoCalled = FALSE;

	// �� 460����Ʈ

	inline static CTLSMemoryPoolManager<CNetSession, 16, 4> s_sSessionPool = CTLSMemoryPoolManager<CNetSession, 16, 4>();
	inline static LONG RELEASE_FLAG = 0x80000000;
};

