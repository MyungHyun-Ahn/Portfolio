#pragma once

class CGameServer;
typedef void (CGameServer::*SectorFunc)(UINT64, CPlayer *);

class CGameServer : public CServerCore
{
public:
	friend class CGameProcessPacket;

	CGameServer();
	~CGameServer();

	// Sector 단위 진행
	BOOL			SendSector(const UINT64 sessionId, INT secY, INT secX, CSerializableBuffer *message);
	BOOL			FuncSector(const UINT64 sessionId, INT secY, INT secX, SectorFunc func);

	// CServerCore을(를) 통해 상속됨
	VOID			OnAccept(const UINT64 sessionId) override;
	VOID			OnClientLeave(const UINT64 sessionId) override;
	BOOL			OnRecv(const UINT64 sessionId, CSerializableBuffer *message) override;

	// SectorFunc으로 사용할 것을 정의
	VOID			NewPlayerGetOtherPlayerInfo(UINT64 newPlayerId, CPlayer *otherPlayer);

	// FuncSector를 사용하려 했으나 실패해서 만든 함수
	CPlayer			*GetCurrentTargetPlayerId(UINT64 playerId, INT dir, INT rangeY, INT rangeX);

	// GameUpdateFrame
	void			Update();

	INT				GetPlayerCount() { return m_mapPlayers.size(); }

private:
	std::unordered_map<UINT64, CPlayer *> m_mapPlayers;
};

