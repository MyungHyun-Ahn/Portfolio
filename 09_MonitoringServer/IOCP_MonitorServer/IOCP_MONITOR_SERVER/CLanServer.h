#pragma once

namespace LAN_SERVER
{
	class CLanSession
	{
	public:
		friend class CLanServer;

		CLanSession() noexcept
			: m_sSessionSocket(INVALID_SOCKET)
			, m_uiSessionID(0)
		{
			// + 0 : ConnectEx
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
		bool PostSend(bool isPQCS = FALSE) noexcept;

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
		CSerializableBufferView<TRUE> *m_pDelayedBuffer = nullptr;
		CLFQueue<CSerializableBuffer<TRUE> *> m_lfSendBufferQueue;
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
	};

	class CLanServer
	{
	public:
		BOOL Start(const CHAR *openIP, const USHORT port) noexcept;
		void Stop();
		void WaitLanServerStop();

		inline LONG GetSessionCount() const noexcept { return m_iSessionCount; }

		void SendPacket(const UINT64 sessionID, CSerializableBuffer<TRUE> *sBuffer) noexcept;
		// Send 시도는 하지 않음
		void EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<TRUE> *sBuffer) noexcept;

		BOOL Disconnect(const UINT64 sessionID, BOOL isPQCS = FALSE) noexcept;
		BOOL ReleaseSession(CLanSession *pSession, BOOL isPQCS = FALSE) noexcept;
		BOOL ReleaseSessionPQCS(CLanSession *pSession) noexcept;

		virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept = 0;
		virtual void OnAccept(const UINT64 sessionID) noexcept = 0;
		virtual void OnClientLeave(const UINT64 sessionID) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept = 0;
		virtual void OnError(int errorcode, WCHAR *errMsg) noexcept = 0;

		// AcceptEx
		void FristPostAcceptEx() noexcept;
		BOOL PostAcceptEx(INT index) noexcept;
		BOOL AcceptExCompleted(CLanSession *pSession) noexcept;

	private:
		inline static USHORT GetIndex(UINT64 sessionId) noexcept
		{
			UINT64 mask64 = sessionId & SESSION_INDEX_MASK;
			mask64 = mask64 >> 48;
			return (USHORT)mask64;
		}

		inline static UINT64 GetId(UINT64 sessionId) noexcept
		{
			UINT64 mask64 = sessionId & SESSION_ID_MASK;
			return mask64;
		}

		inline static UINT64 CombineIndex(USHORT index, UINT64 id) noexcept
		{
			UINT64 index64 = index;
			index64 = index64 << 48;
			return index64 | id;
		}

	public:
		int WorkerThread() noexcept;

		int TimerEventSchedulerThread() noexcept;

		void RegisterSystemTimerEvent();
		virtual void RegisterContentTimerEvent() noexcept = 0;

		void RegisterTimerEvent(BaseEvent *timerEvent) noexcept;
		static void RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept;

		inline LONG GetSessionCount() noexcept { return m_iSessionCount; };

	private:
		// Session
		LONG					m_iSessionCount = 0;
		LONG64					m_iCurrentID = 0;

		USHORT					m_usMaxSessionCount;
		CLanSession **m_arrPSessions;
		CLFStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		SOCKET					m_sListenSocket = INVALID_SOCKET;
		UINT32					m_uiMaxWorkerThreadCount;
		BOOL					m_bIsWorkerRun = TRUE;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		LPFN_ACCEPTEX					m_lpfnAcceptEx = NULL;
		GUID							m_guidAcceptEx = WSAID_ACCEPTEX;
		CLanSession **m_arrAcceptExSessions;

		LPFN_GETACCEPTEXSOCKADDRS		m_lpfnGetAcceptExSockaddrs = NULL;
		GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		HANDLE												m_hTimerEventSchedulerThread;
		BOOL												m_bIsTimerEventSchedulerRun = TRUE;
		std::priority_queue<TimerEvent *, std::vector<TimerEvent *>, TimerEventComparator>	m_TimerEventQueue;

		// IOCP 핸들
		HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;

		LONG m_isStop = FALSE;
	};

	extern CLanServer *g_LanServer;
};

