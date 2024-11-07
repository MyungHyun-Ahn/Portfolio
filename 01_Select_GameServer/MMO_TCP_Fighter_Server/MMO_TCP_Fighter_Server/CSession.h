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
		// 나머지는 생성하는 측에서 세팅
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

	// 유효성 체크
	BOOL m_isVaild = TRUE;
	INT m_iPrevRecvTime = 0;

	// 네트워크 관련
	SOCKET m_ClientSocket;
	WCHAR m_szClientIP[16] = { 0 };

	// 링 버퍼
	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

#ifdef USE_OBJECT_POOL
	inline static CObjectPool<CSession> s_SessionPool = CObjectPool<CSession>(7500, false);
#endif
};

