#pragma once
class CPlayer;
class CGameServer;

class CSector
{
public:
	friend class CGameServer;

	// 섹터 처리 함수
	inline static int	CalSectorY(int y) { return (y - RANGE_MOVE_LEFT) / SECTOR_SIZE; }
	inline static int	CalSectorX(int x) { return (x - RANGE_MOVE_TOP) / SECTOR_SIZE; }
	static void			MoveSector(UINT64 playerId, int prevY, int prevX, int nowY, int nowX);

public:
	inline static std::unordered_map<UINT64, CPlayer *>		s_Sectors[SECTOR_MAX_Y][SECTOR_MAX_X];

private:
	inline static CGameServer								*s_pGameServer;
};

