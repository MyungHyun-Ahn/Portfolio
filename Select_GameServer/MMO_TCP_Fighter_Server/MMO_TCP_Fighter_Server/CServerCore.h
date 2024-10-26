#pragma once
/*
네트워크 코어 엔진
	* 게임 서버 등의 콘텐츠 서버는 이를 상속받아 구현할 것
*/
class CServerCore
{
public:
	friend class CProcessPacket;

	BOOL					Start(CONST CHAR *openIp, CONST USHORT port, INT maxSessionCount);
	VOID					Stop();

	BOOL					Select();
	VOID					TimeoutCheck();
	BOOL					Disconnect();

	BOOL					SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message);

private:
	BOOL					Accept();
	BOOL					Recv(CSession *pSession);
	BOOL					Send(CSession *pSession);
	BOOL					Process(CSession *pSession);

public:
	// 상속받는 쪽에서 호출할 콜백을 구현하여 등록해야함
	virtual VOID			OnAccept(CONST UINT64 sessionId) = 0;
	virtual VOID			OnClientLeave(CONST UINT64 sessionId) = 0;
	virtual BOOL			OnRecv(CONST UINT64 sessionId, CSerializableBuffer *message) = 0;

private:
	std::unordered_map<UINT64, CSession *>		m_mapSessions;

	// Session 관련
	UINT64									m_iCurSessionIDValue = 0; // 유저에게 부여할 ID 번호
	UINT64									m_iMaxSessionCount = 0;

	// DeleteQueue
	std::deque<CSession *>					m_deqDeleteQueue;

	// 소켓 관련
	SOCKET									m_listenSocket;
	fd_set									m_readSet;
	fd_set									m_writeSet;
};

