#pragma once
class CNonLoginPlayer
{
public:
	// 최근 수신 시간
	DWORD m_dwPrevRecvTime;
};

class CPlayer
{
public:
	friend class CChatServer;
	friend class CChatProcessPacket;

private:
	INT64	m_iAccountNo;
	DWORD	m_dwPrevRecvTime;
	WORD	m_usSectorY;
	WORD	m_usSectorX;

	WCHAR	m_szID[20];
	WCHAR	m_szNickname[20];

	inline static CTLSMemoryPoolManager<CPlayer> s_PlayerPool = CTLSMemoryPoolManager<CPlayer>();
	// char	m_sessionKey[64];
};

