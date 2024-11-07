#pragma once
class CSession;

class CLanServer
{
public:
	BOOL Start(const CHAR *openIP, const USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount);
	// void Stop();

	inline INT GetSessionCount() { return m_iSessionCount; }

	void SendPacket(const UINT64 sessionID, CSerializableBuffer *sBuffer);
	BOOL Disconnect(CSession *pSession);
	BOOL ReleaseSession(CSession *pSession);

	// void Monitor();

	virtual bool OnConnectionRequest(const WCHAR *ip, USHORT port) = 0;
	virtual void OnAccept(const UINT64 sessionID) = 0;
	virtual void OnClientLeave(const UINT64 sessionID) = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBuffer *message) = 0;
	virtual void OnError(int errorcode, WCHAR *errMsg) = 0;

public:
	int WorkerThread();
	int AccepterThread();

private:
	// Session
	INT						m_iSessionCount = 0;
	INT						m_iCurrentID = 0;

	USHORT					m_usMaxSessionCount = 65535;
	CSession				*m_arrPSessions[65535];
	std::vector<USHORT>		m_stackDisconnectIndex;
	SRWLOCK					m_disconnectStackLock;

	// Worker
	SOCKET					m_sListenSocket = INVALID_SOCKET;
	UINT32					m_uiMaxWorkerThreadCount;
	BOOL					m_bIsWorkerRun = TRUE;
	std::vector<HANDLE>		m_arrWorkerThreads;

	// Accepter
	BOOL m_bIsAcceptorRun = TRUE;
	HANDLE m_hAcceptorThread;

	// IOCP วฺต้
	HANDLE m_hIOCPHandle = INVALID_HANDLE_VALUE;

	// TPS
	LONG m_iAcceptTPS = 0;
	LONG m_iRecvTPS = 0;
	LONG m_iSendTPS = 0;
};

extern CLanServer *g_Server;
extern std::unordered_map<UINT64, CSession *> g_mapSessions;
extern CRITICAL_SECTION g_SessionMapLock;