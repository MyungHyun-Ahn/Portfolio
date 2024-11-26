#pragma once
class CSession;

class CLanClients
{
public:
	bool Start(const CHAR *serverIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT sessionCount);

	inline LONG GetSessionCount() { return m_iSessionCount; }

	bool SetSockOpt(SOCKET sock);
	void SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer, bool sendPost = true);
	bool Disconnect(CSession *pSession);
	bool ReleaseSession(CSession *pSession);

	virtual void OnConnect(const UINT64 sessionID) = 0;
	virtual void OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView> message) = 0;
	virtual void OnReleaseSession(const UINT64 sessionID) = 0;

	void FristPostConnectEx();
	bool PostConnectEx(INT index);
	bool ConnectExCompleted(CSession *pSession);

	protected:
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
	int PostConnectExThread();

	// ReleaseSession 한번 당 1번
	void PostConnectExAPCEnqueue();
	static void PostConnectExAPCFunc(ULONG_PTR lpParam);

private:
	// Session
	LONG					m_iSessionCount = 0;
	INT64					m_iCurrentID = 0;

	SOCKADDR_IN				m_ServerAddr;
	const CHAR				*m_szServerIP;
	USHORT					m_usServerPort;

	USHORT					m_usMaxSessionCount = 65535;
	CSession				*m_arrPSessions[65535];
	CLFStack<USHORT>		m_stackDisconnectIndex;

	// Worker
	UINT32					m_uiMaxWorkerThreadCount;
	BOOL					m_bIsWorkerRun = TRUE;
	std::vector<HANDLE>		m_arrWorkerThreads;

	// Connector
	HANDLE				m_hPostConnectExThread;
	BOOL				m_bIsPostConnectExRun = TRUE;
	CSession			**m_arrConnectExSessions;
	CLFStack<USHORT>    m_stackFreeAcceptExIndex;

	// IOCP 핸들
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;
};

extern CLanClients *g_Clients;