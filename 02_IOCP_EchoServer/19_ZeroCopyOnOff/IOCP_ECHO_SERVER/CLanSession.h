#pragma once

enum class WHERE_SEND : DWORD
{
	ACQUIRE_SENDCOMPLETED = 1,
	ACQUIRE_SENDPACKET = 2,
	RELEASE_SENDCOMPLETED = 3,
	RELEASE_POSTSEND = 4,
	MESSAGE_ENQUEUE = 5,
};

struct SendDebug
{
	DWORD threadId;
	WHERE_SEND where;
};

class CLanSession
{
public:
	friend class CLanServer;

	CLanSession() noexcept
		: m_sSessionSocket(INVALID_SOCKET)
		, m_uiSessionID(0)
	{
		// + 0 : AcceptEx
		// + 1 : Recv
		// + 2 : Send
		m_pMyOverlappedStartAddr = g_OverlappedAlloc.Alloc();
	}

	~CLanSession() noexcept
	{
	}

	void Init(UINT64 sessionID) noexcept
	{
		m_uiSessionID = sessionID;
		InterlockedExchange(&m_iSendFlag, FALSE);
		InterlockedExchange(&m_iCacelIoCalled, FALSE);
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

	void Clear() noexcept;

	void RecvCompleted(int size) noexcept;

	bool SendPacket(CSerializableBuffer<TRUE> *message) noexcept;
	void SendCompleted(int size) noexcept;

	bool PostRecv() noexcept;
	bool PostSend(WHERE_SEND where) noexcept;

public:
	inline static CLanSession *Alloc() noexcept
	{
		CLanSession *pSession = s_sSessionPool.Alloc();
		return pSession;
	}

	inline static void Free(CLanSession *delSession) noexcept
	{
		delSession->Clear();
		s_sSessionPool.Free(delSession);
	}

	inline static LONG GetPoolCapacity() noexcept { return s_sSessionPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return s_sSessionPool.GetUseCount(); }

private:
	// 패딩 계산해서 세션 크기 최적화
	// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
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
	char		m_dummy01[2]; // 패딩 계산용
	// 최대 무조건 1개 -> 있거나 없거나
	CSerializableBufferView<TRUE>				*m_pDelayedBuffer = nullptr;
	CLFQueue<CSerializableBuffer<TRUE> *>		m_lfSendBufferQueue;
	CRingBuffer									m_SendRingBuffer;
	// m_pMyOverlappedStartAddr
	//  + 0 : ACCEPTEX
	//  + 1 : RECV
	//  + 2 : SEND
	OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
	CSerializableBuffer<TRUE> *m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT]; // 8 * 32 = 256

	LONG m_iCacelIoCalled = FALSE;

	// 총 460바이트

	inline static CTLSMemoryPoolManager<CLanSession, 16, 4> s_sSessionPool = CTLSMemoryPoolManager<CLanSession, 16, 4>();
	inline static LONG RELEASE_FLAG = 0x80000000;

	LONG sendDebugIndex = 0;
	SendDebug sendDebug[65535];
};

