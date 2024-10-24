#pragma once

class CSession
{
public:
	friend class CServerCore;
	friend class CProcessPacket;

	CSession() = default;
	CSession(int id, SOCKET sock) : m_iId(id), m_ClientSocket(sock) {}

private:
	BOOL m_isVaild = TRUE;
	UINT64 m_iId;

	SOCKET m_ClientSocket;
	WCHAR m_szClientIP[16] = { 0 };

	INT m_iPrevRecvTime = 0;

	CRingBuffer m_RecvBuffer;
	CRingBuffer m_SendBuffer;

	inline static CObjectPool<CSession> m_SessionPool = CObjectPool<CSession>(7500, true);
};

