#pragma once
class CPlayer
{
public:
	friend class CGameServer;
	friend class CGameProcessPacket;
	friend class CSector;

	CPlayer() = default;
	CPlayer(UINT id, USHORT y, USHORT x) 
		: m_iId(id)
		, m_sY(y)
		, m_sX(x)
		, m_bDirection((char)MOVE_DIR::MOVE_DIR_LL)
		, m_dwAction((DWORD)MOVE_DIR::MOVE_DIR_STOP) {}

	inline void Init(UINT id, USHORT y, USHORT x) 
	{
		m_iId = id;
		m_sY = y;
		m_sX = x;

		m_sSecY = CSector::CalSectorY(m_sY);
		m_sSecX = CSector::CalSectorX(m_sX);

		m_dwAction = (DWORD)MOVE_DIR::MOVE_DIR_STOP;
		m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_RR;
	}

	bool				Move(SHORT x, SHORT y);
	inline bool			GetDamage(INT damage) 
	{ 
		m_sHp -= damage; 
		if (m_sHp <= 0) // 캐릭터가 죽은 상황
			return false;
		return true; 
	}

private:
	// Player Id - Session ID와 같음
	UINT64		m_iId;

	// Player 게임 정보
	USHORT		m_sY;
	USHORT		m_sX;
	SHORT		m_sHp = MAX_PLAYER_HP;

	// Player 게임 상태
	BYTE		m_bDirection;
	DWORD		m_dwAction;

	// Player 속한 Sector
	USHORT		m_sSecY;
	USHORT		m_sSecX;
};

