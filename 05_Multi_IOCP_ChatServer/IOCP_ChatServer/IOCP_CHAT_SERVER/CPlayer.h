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

	inline void Clear() noexcept
	{
		m_usSectorY = 0xFF;
		m_usSectorX = 0xFF; 
		m_iAccountNo = 0;
	}

	inline static CPlayer *Alloc() noexcept
	{
		CPlayer *newPlayer = s_PlayerPool.Alloc();
		newPlayer->Clear();
		return newPlayer;
	}

	inline static void Free(CPlayer *delPlayer) noexcept
	{
		s_PlayerPool.Free(delPlayer);
	}

	inline static LONG GetPoolCapacity() noexcept { return s_PlayerPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return s_PlayerPool.GetUseCount(); }

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

