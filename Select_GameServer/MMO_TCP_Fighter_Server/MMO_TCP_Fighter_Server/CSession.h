#pragma once

class CSession
{
public:
	friend class CServerCore;

	CSession() = default;
	CSession(int id, SOCKET sock) : m_iId(id), m_ClientSocket(sock) {}

private:
	// Session ID
	UINT64 m_iId;

	// ��ȿ�� üũ
	BOOL m_isVaild = TRUE;
	INT m_iPrevRecvTime = 0;

	// ��Ʈ��ũ ����
	SOCKET m_ClientSocket;
	WCHAR m_szClientIP[16] = { 0 };

	// �� ����
	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

	inline static CObjectPool<CSession> m_SessionPool = CObjectPool<CSession>(7500, true);
};

