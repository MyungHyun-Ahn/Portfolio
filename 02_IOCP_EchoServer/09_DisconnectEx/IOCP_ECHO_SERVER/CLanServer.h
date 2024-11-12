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

	BOOL PostDisconnectEx(CSession *pSession);
	BOOL DisconnectExCompleted(CSession *pSession);

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

	// 세션 배열
	USHORT					m_usMaxSessionCount = 65535;
	CSession				*m_arrPSessions[65535];

	// 연결 해제 스택
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

	HANDLE					m_hPostAcceptExThread;
	BOOL					m_bIsPostAcceptExRun = TRUE;

	// AcceptEx 세션 배열
	CSession				*m_arrAcceptExSessions[ACCEPTEX_COUNT];

	// AcceptEx index 스택
	USHORT					m_arrFreeAcceptExIndex[ACCEPTEX_COUNT];
	LONG					m_freeAcceptExArrTop;
	SRWLOCK					m_freeAcceptExStackLock;

	LPFN_DISCONNECTEX		m_lpfnDisconnectEx = NULL;
	GUID					m_guidDisconnectEx = WSAID_DISCONNECTEX;

	// IOCP 핸들
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
	HANDLE m_hDummyIOCPHandle = INVALID_HANDLE_VALUE;
};

extern CLanServer *g_Server;