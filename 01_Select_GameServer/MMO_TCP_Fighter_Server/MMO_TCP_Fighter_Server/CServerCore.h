#pragma once

class CProcessPacketInterface;

/*
��Ʈ��ũ �ھ� ����
	* ���� ���� ���� ������ ������ �̸� ��ӹ޾� ������ ��
*/
class CServerCore
{
public:
	friend class CProcessPacket;

	BOOL					Start(CONST CHAR *openIp, CONST USHORT port, INT maxSessionCount);
	VOID					Stop();

	BOOL					Select();
	VOID					TimeoutCheck();
	BOOL					ReleaseSession();
	VOID					Disconnect(UINT64 sessionId);

	BOOL					SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message);

	INT						GetSessionCount() { return m_mapSessions.size(); }

private:
	BOOL					Accept();
	BOOL					Recv(CSession *pSession);
	BOOL					Send(CSession *pSession);
	BOOL					Process(CSession *pSession);

public:
	// ��ӹ޴� �ʿ��� ȣ���� �ݹ��� �����Ͽ� ����ؾ���
	virtual VOID			OnAccept(CONST UINT64 sessionId) = 0;
	virtual VOID			OnClientLeave(CONST UINT64 sessionId) = 0;
	virtual BOOL			OnRecv(CONST UINT64 sessionId, CSerializableBuffer *message) = 0;

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

protected:
	// ��Ŷ ó����
	CProcessPacketInterface					*m_pProcessPacket;
};

