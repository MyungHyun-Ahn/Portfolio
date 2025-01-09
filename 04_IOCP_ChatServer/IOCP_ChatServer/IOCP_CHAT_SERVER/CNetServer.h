#pragma once
class CNetSession;

class CNetServer
{
public:
	BOOL Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount) noexcept;
	// void Stop();

	inline LONG GetSessionCount() const noexcept { return m_iSessionCount; }

	void SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept;
	BOOL Disconnect(const UINT64 sessionID) noexcept;
	BOOL ReleaseSession(CNetSession *pSession) noexcept;

	virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept = 0;
	virtual void OnAccept(const UINT64 sessionID) noexcept = 0;
	virtual void OnClientLeave(const UINT64 sessionID) noexcept = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept = 0;
	virtual void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept = 0;
	virtual void OnError(int errorcode, WCHAR *errMsg) noexcept = 0;

	virtual DWORD OnUpdate() noexcept = 0;
	virtual void OnHeartBeat() noexcept = 0;


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

	void AcceptAPCEnqueue(INT index, UINT64 sessionId) noexcept;
	static void AcceptAPCFunc(ULONG_PTR lpParam) noexcept;

	void ClientLeaveAPCEnqueue(UINT64 sessionId) noexcept;
	static void ClientLeaveAPCFunc(ULONG_PTR lpParam) noexcept;

private:
	// Session
	LONG					m_iSessionCount = 0;
	LONG64					m_iCurrentID = 0;

	USHORT					m_usMaxSessionCount = 65535;
	CNetSession				*m_arrPSessions[65535];
	CLFStack<USHORT>		m_stackDisconnectIndex;

	// Worker
	SOCKET					m_sListenSocket = INVALID_SOCKET;
	UINT32					m_uiMaxWorkerThreadCount;
	BOOL					m_bIsWorkerRun = TRUE;
	std::vector<HANDLE>		m_arrWorkerThreads;

	// Accepter
	LPFN_ACCEPTEX		m_lpfnAcceptEx = NULL;
	GUID				m_guidAcceptEx = WSAID_ACCEPTEX;

	LPFN_GETACCEPTEXSOCKADDRS		m_lpfnGetAcceptExSockaddrs = NULL;
	GUID							m_guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

	HANDLE				m_hServerFrameThread;
	BOOL				m_bIsServerFrameRun = TRUE;
	CNetSession			*m_arrAcceptExSessions[ACCEPTEX_COUNT];
	CLFStack<USHORT>    m_stackFreeAcceptExIndex;

	// IOCP วฺต้
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
};

extern CNetServer *g_NetServer;

