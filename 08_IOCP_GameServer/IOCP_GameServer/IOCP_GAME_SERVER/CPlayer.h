#pragma once

class CNonLoginPlayer
{
public:
	DWORD m_dwPrevRecvTime;
};

class CPlayer
{
public:
	friend class CGameServer;
	friend class CGameProcessPacket;
	friend class CEchoContent;

	inline static CPlayer *Alloc() noexcept
	{
		CPlayer *newPlayer = s_PlayerPool.Alloc();
		return newPlayer;
	}

	inline static void Free(CPlayer *delPlayer) noexcept
	{
		s_PlayerPool.Free(delPlayer);
	}

private:
	INT64 m_iAccountNo;
	DWORD m_dwPrevRecvTime;
	
	WCHAR m_szID[20];
	WCHAR m_szNickname[20];

	inline static CTLSMemoryPoolManager<CPlayer> s_PlayerPool = CTLSMemoryPoolManager<CPlayer>();
};

