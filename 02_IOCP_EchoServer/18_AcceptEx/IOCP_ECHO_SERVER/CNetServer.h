#pragma once
class CNetSession;

class CNetServer
{
public:
	BOOL Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount);
	// void Stop();

	inline LONG GetSessionCount() { return m_iSessionCount; }

	void SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer);
	BOOL Disconnect(const UINT64 sessionID);
	BOOL ReleaseSession(CNetSession *pSession);

	virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) = 0;
	virtual void OnAccept(const UINT64 sessionID) = 0;
	virtual void OnClientLeave(const UINT64 sessionID) = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) = 0;
	virtual void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) = 0;
	virtual void OnError(int errorcode, WCHAR *errMsg) = 0;

	void FristPostAcceptEx();
	BOOL PostAcceptEx(INT index);
	BOOL AcceptExCompleted(CNetSession *pSession);

private:
	inline static USHORT GetIndex(UINT64 sessionId)
	{
		UINT64 mask64 = sessionId & SESSION_INDEX_MASK;
		mask64 = mask64 >> 48;
		return (USHORT)mask64;
	}

	inline static UINT64 GetId(UINT64 sessionId)
	{
		UINT64 mask64 = sessionId & SESSION_ID_MASK;
		return mask64;
	}

	inline static UINT64 CombineIndex(USHORT index, UINT64 id)
	{
		UINT64 index64 = index;
		index64 = index64 << 48;
		return index64 | id;
	}

public:
	int WorkerThread();
	int PostAcceptThread();

	void PostAcceptAPCEnqueue(INT index);
	static void PostAcceptAPCFunc(ULONG_PTR lpParam);

private:
	// Session
	LONG					m_iSessionCount = 0;
	INT						m_iCurrentID = 0;

	USHORT					m_usMaxSessionCount = 65535;
	CNetSession *m_arrPSessions[65535];
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

	HANDLE				m_hPostAcceptExThread;
	BOOL				m_bIsPostAcceptExRun = TRUE;
	CNetSession *m_arrAcceptExSessions[ACCEPTEX_COUNT];
	CLFStack<USHORT>    m_stackFreeAcceptExIndex;

	// IOCP วฺต้
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
};

extern CNetServer *g_NetServer;

