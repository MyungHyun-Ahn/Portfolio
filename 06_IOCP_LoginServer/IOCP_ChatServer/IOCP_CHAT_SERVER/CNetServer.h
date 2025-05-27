#pragma once

namespace NET_SERVER
{
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
			InterlockedExchange(&m_iCacelIoCalled, FALSE);
		}

		void Init(SOCKET socket, UINT64 sessionID) noexcept
		{
			m_sSessionSocket = socket;
			m_uiSessionID = sessionID;
			InterlockedExchange(&m_iSendFlag, FALSE);
			InterlockedExchange(&m_iCacelIoCalled, FALSE);
		}

		void Clear() noexcept;

		void RecvCompleted(int size) noexcept;

		bool SendPacket(CSerializableBuffer<SERVER_TYPE::WAN> *message) noexcept;
		void SendCompleted(int size) noexcept;

		bool PostRecv() noexcept;
		bool PostSend(bool isPQCS = FALSE) noexcept;

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
		CSerializableBufferView<SERVER_TYPE::WAN> *m_pDelayedBuffer = nullptr;
		CLFQueue<CSerializableBuffer<SERVER_TYPE::WAN> *> m_lfSendBufferQueue;
		// m_pMyOverlappedStartAddr
		//  + 0 : ACCEPTEX
		//  + 1 : RECV
		//  + 2 : SEND
		OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
		CSerializableBuffer<SERVER_TYPE::WAN> *m_arrPSendBufs[WSASEND_MAX_BUFFER_COUNT]; // 8 * 32 = 256

		LONG m_iCacelIoCalled = FALSE;

		// 총 460바이트

		inline static CTLSMemoryPoolManager<CNetSession, 16, 4> s_sSessionPool = CTLSMemoryPoolManager<CNetSession, 16, 4>();
		inline static LONG RELEASE_FLAG = 0x80000000;
	};


	class CNetServer
	{
	public:
		BOOL Start(const CHAR *openIP, const USHORT port) noexcept;
		void Stop();

		inline LONG GetSessionCount() const noexcept { return m_iSessionCount; }

		void SendPacket(const UINT64 sessionID, CSerializableBuffer<SERVER_TYPE::WAN> *sBuffer) noexcept;
		void EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<SERVER_TYPE::WAN> *sBuffer) noexcept;
		BOOL Disconnect(const UINT64 sessionID) noexcept;
		BOOL ReleaseSession(CNetSession *pSession) noexcept;

		virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept = 0;
		virtual void OnAccept(const UINT64 sessionID) noexcept = 0;
		virtual void OnClientLeave(const UINT64 sessionID) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<SERVER_TYPE::WAN> *message) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept = 0;
		virtual void OnError(int errorcode, WCHAR *errMsg) noexcept = 0;

		// 콘텐츠 프레임
		virtual DWORD OnUpdate() noexcept = 0;

		// AcceptEx
		void FristPostAcceptEx() noexcept;
		BOOL PostAcceptEx(INT index) noexcept;
		BOOL AcceptExCompleted(CNetSession *pSession) noexcept;

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
		int ServerFrameThread() noexcept;
		int TimerEventSchedulerThread() noexcept;

		void RegisterSystemTimerEvent();
		virtual void RegisterContentTimerEvent() noexcept = 0;

		void RegisterTimerEvent(TimerEvent *timerEvent) noexcept;
		static void RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept;

		void EventAPCEnqueue(BaseEvent *event) noexcept;
		static void EventHandlerAPCFunc(ULONG_PTR lpParam) noexcept;

		void TimerEventAPCEnqueue(TimerEvent *timerEvent) noexcept;
		static void TimerEventHandlerAPCFunc(ULONG_PTR lpParam) noexcept;

	private:
		// Session
		LONG					m_iSessionCount = 0;
		LONG64					m_iCurrentID = 0;

		USHORT					m_usMaxSessionCount;
		CNetSession				**m_arrPSessions;
		CLFStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		SOCKET					m_sListenSocket = INVALID_SOCKET;
		UINT32					m_uiMaxWorkerThreadCount;
		BOOL					m_bIsWorkerRun = TRUE;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		LPFN_ACCEPTEX			m_lpfnAcceptEx = NULL;
		GUID					m_guidAcceptEx = WSAID_ACCEPTEX;

		LPFN_GETACCEPTEXSOCKADDRS		m_lpfnGetAcceptExSockaddrs = NULL;
		GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		HANDLE							m_hServerFrameThread;
		BOOL							m_bIsServerFrameRun = TRUE;
		CNetSession **m_arrAcceptExSessions;

		HANDLE												m_hTimerEventSchedulerThread;
		BOOL												m_bIsTimerEventSchedulerRun = TRUE;
		std::priority_queue<TimerEvent *, std::vector<TimerEvent *>, TimerEventComparator>	m_TimerEventQueue;

		// IOCP 핸들
		HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;

		LONG m_isStop = FALSE;
	};

	extern CNetServer *g_NetServer;
}

