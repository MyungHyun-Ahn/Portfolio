#pragma once
class CSession;

class CLanServer
{
public:
	BOOL Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount);
	// void Stop();

	inline LONG GetSessionCount() { return m_iSessionCount; }

	void SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer);
	BOOL Disconnect(CSession *pSession);
	BOOL ReleaseSession(CSession *pSession);

	virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) = 0;
	virtual void OnAccept(const UINT64 sessionID) = 0;
	virtual void OnClientLeave(const UINT64 sessionID) = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBuffer *message) = 0;
	virtual void OnError(int errorcode, WCHAR *errMsg) = 0;

	void FristPostAcceptEx();
	BOOL PostAcceptEx(INT index);
	BOOL AcceptExCompleted(CSession *pSession);

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

private:
	// Session
	LONG					m_iSessionCount = 0;
	INT						m_iCurrentID = 0;

	USHORT					m_usMaxSessionCount = 65535;
	CSession				*m_arrPSessions[65535];
	USHORT					m_arrDisconnectIndex[65535];
	LONG					m_disconnectArrTop;
	SRWLOCK					m_disconnectStackLock;

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

	CSession			*m_AcceptExSessions[ACCEPTEX_COUNT];

	// IOCP วฺต้
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
};

extern CLanServer *g_Server;