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

	// 유효성 체크
	BOOL m_isVaild = TRUE;
	INT m_iPrevRecvTime = 0;

	// 네트워크 관련
	SOCKET m_ClientSocket;
	WCHAR m_szClientIP[16] = { 0 };

	// 링 버퍼
	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

	inline static CObjectPool<CSession> m_SessionPool = CObjectPool<CSession>(7500, true);
};

