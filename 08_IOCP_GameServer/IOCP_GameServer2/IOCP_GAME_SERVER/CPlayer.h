#pragma once

class CNonLoginPlayer
{
public:
	DWORD m_dwPrevRecvTime;

	inline static CNonLoginPlayer *Alloc() noexcept
	{
		CNonLoginPlayer *newPlayer = s_PlayerPool.Alloc();
		return newPlayer;
	}

	inline static void Free(CNonLoginPlayer *delPlayer) noexcept
	{
		s_PlayerPool.Free(delPlayer);
	}

private:
	inline static CTLSMemoryPoolManager<CNonLoginPlayer> s_PlayerPool = CTLSMemoryPoolManager<CNonLoginPlayer>();
};

class CPlayer
{
public:
	friend class CGameServer;
	friend class CGameProcessPacket;
	friend class CAuthContents;
	friend class CEchoContents;

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

	inline static CTLSMemoryPoolManager<CPlayer> s_PlayerPool = CTLSMemoryPoolManager<CPlayer>();
};

