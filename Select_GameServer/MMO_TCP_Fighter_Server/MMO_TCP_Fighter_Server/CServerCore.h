#pragma once
/*
��Ʈ��ũ �ھ� ����
	* ���� ���� ���� ������ ������ �̸� ��ӹ޾� ������ ��
*/
class CServerCore
{
public:
	friend class CProcessPacket;

	BOOL					Start(const CHAR *openIp, const USHORT port, INT maxSessionCount);
	VOID					Stop();

	BOOL					Select();
	BOOL					SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message);
	VOID					TimeoutCheck();
	BOOL					Disconnect();

private:
	BOOL					Accept();
	BOOL					Recv(CSession *pSession);
	BOOL					Send(CSession *pSession);
	BOOL					Process(CSession *pSession);

public:
	// ��ӹ޴� �ʿ��� ȣ���� �ݹ��� �����Ͽ� ����ؾ���
	virtual void			OnAccept(CONST UINT64 sessionId) = 0;
	virtual void			OnClientLeave(CONST UINT64 sessionId) = 0;
	virtual bool			OnRecv(CONST UINT64 sessionId, CSerializableBuffer *message) = 0;

private:
	std::unordered_map<UINT64, CSession *>		m_mapSessions;

	// Session ����
	UINT64									m_iCurSessionIDValue = 0; // �������� �ο��� ID ��ȣ
	UINT64									m_iMaxSessionCount = 0;

	// DeleteQueue
	std::deque<CSession *>					m_deqDeleteQueue;

	// ���� ����
	SOCKET									m_listenSocket;
	fd_set									m_readSet;
	fd_set									m_writeSet;
};

