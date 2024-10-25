#pragma once

class CGameServer;
typedef void (CGameServer::*SectorFunc)(UINT64, CPlayer *);

class CGameServer : public CServerCore
{
public:
	// Sector 단위 진행
	bool SendSector(const UINT64 sessionId, INT secY, INT secX, CSerializableBuffer *message);
	bool FuncSector(const UINT64 sessionId, INT secY, INT secX, SectorFunc func);

	// CServerCore을(를) 통해 상속됨
	void OnAccept(const UINT64 sessionId) override;
	void OnClientLeave(const UINT64 sessionId) override;
	bool OnRecv(const UINT64 sessionId, CSerializableBuffer *message) override;

	// SectorFunc으로 사용할 것을 정의
	void NewPlayerGetOtherPlayerInfo(UINT64 newPlayerId, CPlayer *otherPlayer);

private:
	std::unordered_map<UINT64, CPlayer *> m_mapPlayers;
};

