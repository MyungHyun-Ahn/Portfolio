#pragma once

namespace NetworkLib
{
struct SContentsFrameEvent;
class CBaseContents;


	class CNetSession
	{
	public:
		friend class CNetServer;
		friend class CBaseContents;
		friend struct SContentsFrameEvent;

		CNetSession() noexcept
			: m_sessionSocket(INVALID_SOCKET)
			, m_sessionId(0)
		{
			// + 0 : AcceptEx
			// + 1 : Recv
			// + 2 : Send
			m_pMyOverlappedStartAddr = g_overlappedAllocator.Alloc();
		}

		~CNetSession() noexcept
		{
		}

		void Init(UINT64 sessionId) noexcept
		{
			m_sessionId = sessionId;
			InterlockedExchange(&m_sendFlag, FALSE);
			InterlockedExchange(&m_cancelIoCalled, FALSE);
			InterlockedAnd(&m_ioCountAndRelease, ~sm_releaseFlag);
		}

		void Init(SOCKET socket, UINT64 sessionId) noexcept
		{
			m_sessionSocket = socket;
			m_sessionId = sessionId;
			InterlockedExchange(&m_sendFlag, FALSE);
			InterlockedExchange(&m_cancelIoCalled, FALSE);
			InterlockedAnd(&m_ioCountAndRelease, ~sm_releaseFlag);
		}

		void Clear() noexcept;

		void RecvCompleted(int size) noexcept;

		// 인큐할 때 직렬화 버퍼의 포인터를 인큐


		inline bool SendPacket(CSerializableBuffer<FALSE> *pMessage) noexcept
		{
			pMessage->IncreaseRef();
			m_lfSendBufferQueue.Enqueue(pMessage);
			if (m_lfSendBufferQueue.GetUseSize() > kWsasendMaxBufferCount)
				return FALSE;

			InterlockedOr(&m_sendFlag, sm_enqueueFlag);
			return TRUE;
		}

		void SendCompleted(int size) noexcept;

		bool PostRecv() noexcept;
		bool PostSend(bool isSendFlag = FALSE) noexcept;

	public:
		inline static CNetSession *Alloc() noexcept
		{
			CNetSession *pSession = sm_sessionPool.Alloc();
			return pSession;
		}

		inline static void Free(CNetSession *pDelSession) noexcept
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
		CRingBuffer m_recvBuffer;
		// CRecvBuffer *m_pRecvBuffer = nullptr;
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
		// CSerializableBufferView<FALSE> *m_pDelayedBuffer = nullptr;
		CLfQueue<CSerializableBuffer<FALSE> *> m_lfSendBufferQueue;
		// m_pMyOverlappedStartAddr
		//  + 0 : ACCEPTEX
		//  + 1 : RECV
		//  + 2 : SEND
		OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
		CSerializableBuffer<FALSE> *m_pSendBuffers[kWsasendMaxBufferCount]; // 8 * 32 = 256

		LONG m_cancelIoCalled = FALSE;

		// Recv 큐
		MHLib::containers::CLfQueue<CSerializableBuffer<FALSE> *> m_recvMsgQueue;
		// LONG m_ContentStatus = FALSE;

		// 총 460바이트

		// ContentPtr
		CBaseContents *m_pCurrentContent = nullptr;

		inline static MHLib::memory::CTlsMemoryPoolManager<CNetSession, 16, 4> sm_sessionPool =
			MHLib::memory::CTlsMemoryPoolManager<CNetSession, 16, 4>();
		inline static LONG sm_releaseFlag = 0x80000000;
		inline static LONG sm_enqueueFlag = 0x80000000;
	};


	class CNetServer
	{
	public:
		friend class CBaseContents;
		friend struct SContentsFrameEvent;

		BOOL Start(const CHAR *pOpenIp, const USHORT port) noexcept;
		void Stop();

		inline LONG GetSessionCount() const noexcept { return m_sessionCount; }

		void SendPacket(const UINT64 sessionId, CSerializableBuffer<FALSE> *pSBuffer) noexcept;
		// Send 시도는 하지 않음
		void EnqueuePacket(const UINT64 sessionId, CSerializableBuffer<FALSE> *pSBuffer) noexcept;
		void SendPqcs(const CNetSession *pSession);
		void SendAll();
		void SendAllPqcs();

		BOOL Disconnect(const UINT64 sessionId, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSession(CNetSession *pSession, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSessionPqcs(CNetSession *pSession) noexcept;

		virtual bool OnConnectionRequest(const WCHAR *pIp, USHORT port) noexcept = 0;


		virtual void OnAccept(const UINT64 sessionId) noexcept = 0;
		virtual void OnClientLeave(const UINT64 sessionId) noexcept = 0;
		virtual void OnRecv(const UINT64 sessionId, CSerializableBufferView<FALSE> *pMessage) noexcept = 0;


		virtual void OnError(int errorcode, WCHAR *pErrMsg) noexcept = 0;

		// AcceptEx
		void FristPostAcceptEx() noexcept;
		BOOL PostAcceptEx(INT index) noexcept;
		BOOL AcceptExCompleted(CNetSession *pSession) noexcept;

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

	public:
		int WorkerThread() noexcept;

		void RegisterSystemTimerEvent();
		virtual void RegisterContentTimerEvent() noexcept = 0;

	private:
		// Session
		LONG					m_sessionCount = 0;
		LONG64					m_currentId = 0;

		USHORT					m_maxSessionCount;
		CNetSession				**m_pSessions;
		CLfStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		SOCKET					m_listenSocket = INVALID_SOCKET;
		UINT32					m_maxWorkerThreadCount;
		BOOL					m_isWorkerRun = TRUE;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		LPFN_ACCEPTEX					m_pAcceptEx = NULL;
		GUID							m_guidAcceptEx = WSAID_ACCEPTEX;
		CNetSession						**m_pAcceptExSessions;

		LPFN_GETACCEPTEXSOCKADDRS		m_pGetAcceptExSockaddrs = NULL;
		GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		// IOCP 핸들
		HANDLE m_iocpHandle = INVALID_HANDLE_VALUE;

		LONG m_sendAllFlag = FALSE;

		LONG m_isStop = FALSE;
	};

	extern CNetServer *g_pNetServer;
}
