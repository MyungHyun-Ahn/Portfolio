#pragma once

namespace ServerNetworkLib
{
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
}
