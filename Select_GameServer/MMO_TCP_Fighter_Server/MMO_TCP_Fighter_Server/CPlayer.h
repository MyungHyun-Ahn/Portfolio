#pragma once
class CPlayer
{
public:
	friend class CGameServer;

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

		m_sSecY = CPlayer::CalSectorY(m_sY);
		m_sSecX = CPlayer::CalSectorX(m_sX);

		m_dwAction = (DWORD)MOVE_DIR::MOVE_DIR_STOP;
		m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_RR;
	}

	bool				Move(SHORT x, SHORT y);
	inline bool			GetDamage(INT damage) { m_sHp - damage; }


	// ���� ó�� �Լ�
	inline static int	CalSectorY(int y) { return (y - RANGE_MOVE_LEFT) / SECTOR_SIZE; }
	inline static int	CalSectorX(int x) { return (x - RANGE_MOVE_TOP) / SECTOR_SIZE; }
	void				MoveSector(int prevY, int prevX, int nowY, int nowX);

private:
	// Player Id - Session ID�� ����
	UINT64		m_iId;

	// Player ���� ����
	USHORT		m_sY;
	USHORT		m_sX;
	SHORT		m_sHp = MAX_PLAYER_HP;

	// Player ���� ����
	BYTE		m_bDirection;
	DWORD		m_dwAction;

	// Player ���� Sector
	USHORT		m_sSecY;
	USHORT		m_sSecX;

	// Sector �迭 - �̰� �ٵ� ���⿡ �ִ°� ������? ����� �ʿ���
	inline static std::unordered_map<UINT64, CPlayer *> s_Sectors[SECTOR_MAX_Y][SECTOR_MAX_X];
};

