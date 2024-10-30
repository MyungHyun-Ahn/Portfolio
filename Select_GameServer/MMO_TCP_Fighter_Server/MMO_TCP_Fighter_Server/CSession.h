#pragma once

class CSession
{
public:
	friend class CServerCore;

	CSession() = default;
	CSession(int id, SOCKET sock) : m_iId(id), m_ClientSocket(sock) {}

	inline static CSession *Alloc()
	{
		CSession *pSession = s_SessionPool.Alloc();
		// �������� �����ϴ� ������ ����
		pSession->m_isVaild = TRUE;
		pSession->m_RecvBuffer.Clear();
		pSession->m_SendBuffer.Clear();
		return pSession;
	}

	inline static void Free(CSession *delSession)
	{
		s_SessionPool.Free(delSession);
	}

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

#ifdef USE_OBJECT_POOL
	inline static CObjectPool<CSession> s_SessionPool = CObjectPool<CSession>(7500, false);
#endif
};

