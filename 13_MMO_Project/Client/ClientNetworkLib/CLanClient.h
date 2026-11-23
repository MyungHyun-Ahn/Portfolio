#pragma once

namespace NetworkLib
{
	class CLanClientManager;

	class CLanSession : public std::enable_shared_from_this<CLanSession>
	{
	public:
		friend class CLanClient;
		friend class CLanClientManager;

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

		bool PostRecv() noexcept;
		void RecvCompleted(int size) noexcept;

		bool PostSend(bool isPqcs = FALSE) noexcept;
		bool SendPacket(std::shared_ptr<CSerializableBuffer<TRUE>> spMessage) noexcept;
		void SendCompleted(int size) noexcept;

	private:
		// 패딩 계산해서 세션 크기 최적화
	// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
	// Release + IoCount
		LONG m_ioCountAndRelease = sm_releaseFlag;
		LONG m_sendCount = 0;
		std::shared_ptr<CRecvBuffer> m_spRecvBuffer;
		SOCKET m_sessionSocket = INVALID_SOCKET;
		UINT64 m_sessionId = 0;
		WCHAR		m_clientAddrBuffer[16];
		// --- 64

		char	    m_acceptBuffer[64];
		// --- 128

		LONG		m_sendFlag = FALSE;
		USHORT		m_clientPort;
		char		m_dummy01[2]; // 패딩 계산용
		// 최대 무조건 1개 -> 있거나 없거나
		std::shared_ptr<CSerializableBufferView<TRUE>> m_spDelayedBuffer;
		std::mutex m_sendStateLock;
		std::deque<std::shared_ptr<CSerializableBuffer<TRUE>>> m_sendQueue;
		// ConnectEx, Recv, Send 순서. 완료를 소비할 때까지 세션이 소유한다.
		std::array<OVERLAPPED, 3> m_overlappeds{};
		std::array<std::shared_ptr<CSerializableBuffer<TRUE>>, kWsasendMaxBufferCount> m_spSendBuffers;

		LONG m_cancelIoCalled = FALSE;
		bool m_isConnected = false;

		INT m_clientMgrIndex;

		inline static LONG sm_releaseFlag = 0x80000000;
	};

	class CLanClient
	{
	public:
		friend class CLanClientManager;

		BOOL Init(INT maxSessionCount) noexcept;

		inline LONG GetSessionCount() const noexcept
		{
			return m_sessionCount;
		}

		// MakeShared()으로 만든 새 패킷을 넘긴다. 접수 후에는 내용이나 커서를 변경하지 않는다.
		void SendPacket(const UINT64 sessionId, std::shared_ptr<CSerializableBuffer<TRUE>> spBuffer) noexcept;
		void EnqueuePacket(const UINT64 sessionId, std::shared_ptr<CSerializableBuffer<TRUE>> spBuffer) noexcept;

		BOOL Disconnect(const UINT64 sessionId, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSession(CLanSession *pSession, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSessionPqcs(CLanSession *pSession) noexcept;

		virtual void OnConnect(const UINT64 sessionId) noexcept = 0;
		virtual void OnDisconnect(const UINT64 sessionId) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionId, std::shared_ptr<CSerializableBufferView<TRUE>> spMessage) noexcept = 0;

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

		std::shared_ptr<CLanSession> acquireSession(UINT64 sessionId) noexcept;
		void retireSession(CLanSession &session, bool isNotify) noexcept;

	private:
		INT m_clientMgrIndex;

		// Session
		LONG					m_sessionCount = 0;
		LONG64 m_currentId = 0;
		// 슬롯과 빈 인덱스는 같은 락으로 보호한다. 콜백은 락 밖에서 실행한다.
		std::mutex m_sessionTableLock;
		std::vector<USHORT> m_freeIndices;
		std::vector<std::shared_ptr<CLanSession>> m_sessions;
	};

	class CLanClientManager
	{
	public:
		friend class CLanSession;
		friend class CLanClient;
		friend class CClientLifecycleProbe;

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

		[[nodiscard]]
		bool RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionId) noexcept;

		inline BOOL ConnectEx(SOCKET sock, sockaddr *pName, int nameLen, LPOVERLAPPED pOverlapped)
		{
			DWORD bytesSent;
			return m_pConnectEx(sock, pName, nameLen, nullptr, 0, &bytesSent, pOverlapped);
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
}
