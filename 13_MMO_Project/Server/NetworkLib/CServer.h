#pragma once

namespace ServerNetworkLib
{
using SendPacketPtr = MHLib::utils::CSmartPtr<NetworkCommonLib::CSerializableBuffer>;
// View의 소유권을 콘텐츠 단으로 넘기기 위한 목적
using RecvPacketPtr = std::unique_ptr<CSerializableBufferView, decltype(&CSerializableBufferView::Free)>;

	class CSession
	{
	public:
		friend class CServer;

		CSession() noexcept
			: m_sessionSocket(INVALID_SOCKET)
			, m_sessionId(0)
		{
			// + 0 : AcceptEx
			// + 1 : Recv
			// + 2 : Send
			m_pMyOverlappedStartAddr = g_overlappedAllocator.Alloc();
		}

		~CSession() noexcept
		{
			if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
			m_pRecvBuffer = nullptr;

			if (m_pDelayedBuffer != nullptr)
			{
				CSerializableBufferView::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;
			}
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

		[[nodiscard]]
		bool RecvCompleted(int size) noexcept;

		// 인큐할 때 직렬화 버퍼의 포인터를 인큐


		inline bool SendPacket(CSerializableBuffer *pMessage) noexcept
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
		inline static CSession *Alloc() noexcept
		{
			CSession *pSession = sm_sessionPool.Alloc();
			return pSession;
		}

		inline static void Free(CSession *pDelSession) noexcept
		{
			pDelSession->Clear();
			sm_sessionPool.Free(pDelSession);
		}

		inline static LONG GetPoolCapacity() noexcept
		{
			return sm_sessionPool.GetCapacity();
		}
		inline static LONG GetPoolUsage() noexcept
		{
			return sm_sessionPool.GetUseCount();
		}

	private:
		// 패딩 계산해서 세션 크기 최적화
		// + Interlock 사용하는 변수들은 캐시라인 띄워놓기
		// Release + IoCount
		LONG m_ioCountAndRelease = sm_releaseFlag;
		LONG m_sendCount = 0;
		CRecvBuffer *m_pRecvBuffer = nullptr;
		CSerializableBufferView *m_pDelayedBuffer = nullptr;
		SOCKET m_sessionSocket;
		UINT64 m_sessionId;
		WCHAR		m_clientAddrBuffer[16];
		// --- 64

		char	    m_acceptBuffer[64];
		// --- 128

		LONG		m_sendFlag = FALSE;
		USHORT		m_clientPort;
		char		m_dummy01[2]; // 패딩 계산용
		CLfQueue<CSerializableBuffer *> m_lfSendBufferQueue;
		// m_pMyOverlappedStartAddr
		//  + 0 : ACCEPTEX
		//  + 1 : RECV
		//  + 2 : SEND
		OVERLAPPED *m_pMyOverlappedStartAddr = nullptr;
		CSerializableBuffer *m_pSendBuffers[kWsasendMaxBufferCount]; // 8 * 32 = 256

		LONG m_cancelIoCalled = FALSE;

		// Recv 큐
		MHLib::containers::CLfQueue<CSerializableBufferView *> m_recvMsgQueue;
		// LONG m_ContentStatus = FALSE;

		// 총 460바이트

		inline static MHLib::memory::CTlsMemoryPoolManager<CSession, 16, 4> sm_sessionPool =
			MHLib::memory::CTlsMemoryPoolManager<CSession, 16, 4>();
		inline static LONG sm_releaseFlag = 0x80000000;
		inline static LONG sm_enqueueFlag = 0x80000000;
	};


	class CServer
	{
	public:
		BOOL Start(const CHAR *pOpenIp, const USHORT port, EPacketMode packetMode = EPacketMode::Net) noexcept;
		EPacketMode GetPacketMode() const noexcept { return m_packetMode; }
		// Start 호출 스레드가 반환하고 합류한 뒤에만 조회한다.
		bool HasStartedBackgroundWork() const noexcept { return m_hasStartedBackgroundWork; }
		void Stop();

		inline LONG GetSessionCount() noexcept
		{
			return InterlockedCompareExchange(&m_sessionCount, 0, 0);
		}

		void SendPacket(const UINT64 sessionId, CSerializableBuffer *pSBuffer) noexcept;
		// 담당 맵의 수신 큐에서 한 건을 꺼내 소유권을 넘긴다.
		[[nodiscard]]
		bool TryPopRecvPacket(UINT64 sessionId, RecvPacketPtr &outPacket) noexcept;

		BOOL Disconnect(const UINT64 sessionId, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSession(CSession *pSession, BOOL isPqcs = FALSE) noexcept;
		BOOL ReleaseSessionPqcs(CSession *pSession) noexcept;

		virtual bool OnConnectionRequest(const WCHAR *pIp, USHORT port) noexcept = 0;


		virtual void OnAccept(const UINT64 sessionId) noexcept = 0;
		virtual void OnClientLeave(const UINT64 sessionId) noexcept = 0;


		virtual void OnError(int errorcode, WCHAR *pErrMsg) noexcept = 0;

		// AcceptEx
		void FristPostAcceptEx() noexcept;
		BOOL PostAcceptEx(INT index) noexcept;
		BOOL AcceptExCompleted(CSession *pSession) noexcept;

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

		// IOCP 워커를 모두 시작한 뒤 준비 완료를 통지한다.
		virtual void OnServerReady() noexcept = 0;

	private:
		// Session
		LONG					m_sessionCount = 0;
		LONG64					m_currentId = 0;

		CSession				**m_pSessions;
		CLfStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		EPacketMode m_packetMode = EPacketMode::Net;
		SOCKET					m_listenSocket = INVALID_SOCKET;
		BOOL					m_isWorkerRun = TRUE;
		bool m_hasStartedBackgroundWork = false;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		LPFN_ACCEPTEX					m_pAcceptEx = NULL;
		GUID							m_guidAcceptEx = WSAID_ACCEPTEX;
		CSession						**m_pAcceptExSessions;
		LONG							m_lostAcceptSlotCount = 0; // 자원 오류로 다시 걸지 못한 accept 슬롯 수

		LPFN_GETACCEPTEXSOCKADDRS		m_pGetAcceptExSockaddrs = NULL;
		GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		// IOCP 핸들
		HANDLE m_iocpHandle = INVALID_HANDLE_VALUE;

		LONG m_isStop = FALSE;
	};

	extern CServer *g_pServer;
}
