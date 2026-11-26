#pragma once

namespace ServerNetworkLib
{
using SendPacketPtr = MHLib::utils::CSmartPtr<NetworkCommonLib::CSerializableBuffer>;
// View의 소유권을 콘텐츠 단으로 넘기기 위한 목적
using RecvPacketPtr = std::unique_ptr<CSerializableBufferView, decltype(&CSerializableBufferView::Free)>;

class CSession;
struct SServerNetworkSettings;

	class CServer
	{
	public:
		BOOL Start(const SServerNetworkSettings &settings) noexcept;
		BYTE GetPacketCode() const noexcept { return m_packetCode; }
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
		virtual void OnServerReady(USHORT port) noexcept = 0;

	private:
		// Session
		LONG					m_sessionCount = 0;
		LONG64					m_currentId = 0;
		INT m_maxSessionCount = 0;

		CSession				**m_pSessions;
		CLfStack<USHORT>		m_stackDisconnectIndex;

		// Worker
		BYTE m_packetCode = 0;
		EPacketMode m_packetMode = EPacketMode::Net;
		SOCKET					m_listenSocket = INVALID_SOCKET;
		BOOL					m_isWorkerRun = TRUE;
		bool m_hasStartedBackgroundWork = false;
		std::vector<HANDLE>		m_arrWorkerThreads;

		// Accepter
		INT m_acceptExCount = 0;
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
