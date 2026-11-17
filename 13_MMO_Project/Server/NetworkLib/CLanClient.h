#pragma once

namespace NetworkLib
{
#pragma warning(disable : 6387) // ConnectEx Param 경고

	class CLanClientManager;

	class CLanSession
	{
	public:
		friend class CLanClient;
		friend class CLanClientManager;

		CLanSession() noexcept
			: m_sessionSocket(INVALID_SOCKET)
			, m_sessionId(0)
		{
			m_pMyOverlappedStartAddr = g_overlappedAllocator.Alloc();
		}

		~CLanSession() noexcept
		{
		}

		void Init(UINT64 sessionId, INT clientMgrIndex) noexcept
		{
			m_sessionId = sessionId;
			m_clientMgrIndex = clientMgrIndex;
			InterlockedExchange(&m_sendFlag, FALSE);
			InterlockedExchange(&m_cancelIoCalled, FALSE);
			InterlockedAnd(&m_ioCountAndRelease, ~sm_releaseFlag);
		}

		void Init(SOCKET socket, UINT64 sessionId, INT clientMgrIndex) noexcept
		{
			m_sessionSocket = socket;
			m_sessionId = sessionId;
			m_clientMgrIndex = clientMgrIndex;
			InterlockedExchange(&m_sendFlag, FALSE);
			InterlockedExchange(&m_cancelIoCalled, FALSE);
			InterlockedAnd(&m_ioCountAndRelease, ~sm_releaseFlag);
		}

		void Clear() noexcept;

		void RecvCompleted(int size) noexcept;

		bool SendPacket(CSerializableBuffer<TRUE> *pMessage) noexcept;
		void SendCompleted(int size) noexcept;

		bool PostRecv() noexcept;
		bool PostSend(bool isPqcs = FALSE) noexcept;

	public:
		inline static CLanSession *Alloc() noexcept
		{
			CLanSession *pSession = sm_sessionPool.Alloc();
			return pSession;
		}

		inline static void Free(CLanSession *pDelSession) noexcept
		{
			pDelSession->Clear();
			sm_sessionPool.Free(pDelSession);
		}

		inline static LONG GetPoolCapacity() noexcept { return sm_sessionPool.GetCapacity(); }
		inline static LONG GetPoolUsage() noexcept { return sm_sessionPool.GetUseCount(); }

	private:
		// 패딩 계산해서 세션 크기 최적화
	// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
	// Release + IoCount
		LONG m_ioCountAndRelease = sm_releaseFlag;
		LONG m_sendCount = 0;
		CRecvBuffer *m_pRecvBuffer = nullptr;
		SOCKET m_sessionSocket;
		UINT64 m_sessionId;
		WCHAR		m_clientAddrBuffer[16];
		// --- 64

		char	    m_acceptBuffer[64];
		// --- 128

		LONG		m_sendFlag = FALSE;
		USHORT		m_clientPort;
		char		m_dummy01[2]; // 패딩 계산용
		// 최대 무조건 1개 -> 있거나 없거나
		CSerializableBufferView<TRUE> *m_pDelayedBuffer = nullptr;
		MHLib::containers::CLfQueue<CSerializableBuffer<TRUE> *> m_lfSendBufferQueue;
		// m_pMyOverlappedStartAddr
		//  + 0 : ACCEPTEX
		//  + 1 : RECV
		//  + 2 : SEND
		OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
		CSerializableBuffer<TRUE> *m_pSendBuffers[kWsasendMaxBufferCount]; // 8 * 32 = 256

		LONG m_cancelIoCalled = FALSE;

		INT m_clientMgrIndex;

		inline static MHLib::memory::CTlsMemoryPoolManager<CLanSession, 16, 4> sm_sessionPool =
			MHLib::memory::CTlsMemoryPoolManager<CLanSession, 16, 4>();
		inline static LONG sm_releaseFlag = 0x80000000;
	};

	class CLanClient
	{
	public:
		friend class CLanClientManager;

		BOOL Init(INT maxSessionCount) noexcept;

		inline LONG GetSessionCount() const noexcept { return m_sessionCount; }

		void SendPacket(const UINT64 sessionId, CSerializableBuffer<TRUE> *pSBuffer) noexcept;
		void EnqueuePacket(const UINT64 sessionId, CSerializableBuffer<TRUE> *pSBuffer) noexcept;

		BOOL Disconnect(const UINT64 sessionId, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSession(CLanSession *pSession, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSessionPqcs(CLanSession *pSession) noexcept;

		virtual void OnConnect(const UINT64 sessionId) noexcept = 0;
		virtual void OnDisconnect(const UINT64 sessionId) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionId, CSerializableBufferView<TRUE> *pMessage) noexcept = 0;

		BOOL PostConnectEx(const CHAR *pIp, const USHORT port) noexcept;
		BOOL ConnectExCompleted(CLanSession *pSession) noexcept;

	private:
		inline static USHORT getIndex(UINT64 sessionId) noexcept
		{
			UINT64 mask64 = sessionId & kSessionIndexMask;
			mask64 = mask64 >> 48;
			return (USHORT)mask64;
		}

		inline static UINT64 getId(UINT64 sessionId) noexcept
		{
			UINT64 mask64 = sessionId & kSessionIdMask;
			return mask64;
		}

		inline static UINT64 combineIndex(USHORT index, UINT64 id) noexcept
		{
			UINT64 index64 = index;
			index64 = index64 << 48;
			return index64 | id;
		}

	private:
		INT m_clientMgrIndex;

		// Session
		LONG					m_sessionCount = 0;
		LONG64					m_currentId = 0;
		CLfStack<USHORT>		m_stackDisconnectIndex;

		INT			m_maxSessionCount;
		CLanSession **m_pSessions;
	};

	class CLanClientManager
	{
	public:
		friend class CLanSession;
		friend class CLanClient;

		// IOCP 핸들 생성 등
		BOOL Init() noexcept;

		BOOL Start() noexcept;
		BOOL Wait() noexcept;

		// 서버 처음 실행시에만 등록 돌고있을 때 추가는 없음
		inline void RegisterClient(CLanClient *pClient) noexcept
		{
			pClient->m_clientMgrIndex = m_clientSize++;
			m_arrNetClients.push_back(pClient);
		}

		inline void RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionId);

		inline BOOL ConnectEx(SOCKET sock, sockaddr *pName, int nameLen, LPOVERLAPPED pOverlapped)
		{
			return m_pConnectEx(sock, pName, nameLen, nullptr, 0, nullptr, pOverlapped);
		}

	public:
		int WorkerThread() noexcept;

	private:
		// Connector
		LPFN_CONNECTEX m_pConnectEx = NULL;
		GUID m_guidConnectEx = WSAID_CONNECTEX;

		// Worker
		UINT32					m_maxWorkerThreadCount;
		BOOL					m_isWorkerRun = TRUE;
		std::vector<HANDLE>		m_arrWorkerThreads;

		INT m_clientSize = 0;
		std::vector<CLanClient *>		m_arrNetClients;
		HANDLE							m_iocpHandle = INVALID_HANDLE_VALUE;
	};

	extern CLanClientManager *g_pLanClientManager;
;
}
