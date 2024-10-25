#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CServerCore.h"
#include "CPlayer.h"
#include "CGameServer.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"
/*
	적절한 섹터 범위의 모든 플레이어에게 전송
		* sessionId의 플레이어는 제외
		* sessionId가 0일시 자신도 포함
			* sessionId는 1부터 부여하기 때문에 0이 들어올리가 없음
*/
bool CGameServer::SendSector(const UINT64 sessionId, INT secY, INT secX, CSerializableBuffer *message)
{
	int startY = secY - SECTOR_VIEW_START;
	int startX = secX - SECTOR_VIEW_START;

	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= SECTOR_MAX_Y || startX + x < 0 || startX + x >= SECTOR_MAX_X)
				continue;

			for (auto player : CPlayer::s_Sectors[startY + y][startX + x])
			{
				CPlayer *otherPlayer = player.second;
				if (otherPlayer->m_iId == sessionId)
					continue;

				SendPacket(sessionId, message);
			}
		}
	}

	return true;
}

/*
	섹터 범위 내의 플레이어를 대상으로 수행할 행동 지정
*/
bool CGameServer::FuncSector(const UINT64 sessionId, INT secY, INT secX, SectorFunc func)
{
	int startY = secY - SECTOR_VIEW_START;
	int startX = secX - SECTOR_VIEW_START;

	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= SECTOR_MAX_Y || startX + x < 0 || startX + x >= SECTOR_MAX_X)
				continue;

			for (auto player : CPlayer::s_Sectors[startY + y][startX + x])
			{
				CPlayer *otherPlayer = player.second;
				if (otherPlayer->m_iId == sessionId)
					continue;

				(this->*func)(sessionId, otherPlayer);
			}
		}
	}

	return true;
}

// 여기서 Player 생성 작업을 진행
void CGameServer::OnAccept(const UINT64 sessionId)
{
	// 랜덤 좌표 생성
	USHORT ranY = (rand() % (RANGE_MOVE_BOTTOM - RANGE_MOVE_TOP - 1)) + RANGE_MOVE_TOP + 1;
	USHORT ranX = (rand() % (RANGE_MOVE_RIGHT - RANGE_MOVE_LEFT - 1)) + RANGE_MOVE_LEFT + 1;

	// Player 객체 생성
	CPlayer *newPlayer = new CPlayer;
	newPlayer->Init(sessionId, ranY, ranX);

	CSerializableBuffer *createMyCharPacket = CGenPacket::makePacketSCCreateMyCharacter(sessionId, newPlayer->m_bDirection, newPlayer->m_sX, newPlayer->m_sY, (BYTE)newPlayer->m_sHp);
	if (!SendPacket(sessionId, createMyCharPacket))
		g_Logger->WriteLog(L"GameContent", LOG_LEVEL::ERR, L"OnAccept SendPacket Err SessionID : %d", sessionId);

	// 다른 플레이어에게 자신의 캐릭터 생성 정보를 전달
	// 섹터 계산
	int startY = newPlayer->m_sSecY - SECTOR_VIEW_START;
	int startX = newPlayer->m_sSecX - SECTOR_VIEW_START;

	// 섹터 내의 모든 다른 캐릭터에게 내 생성 정보를 보내라
	SectorFunc func = &CGameServer::NewPlayerGetOtherPlayerInfo;
	FuncSector(sessionId, newPlayer->m_sSecY, newPlayer->m_sSecX, func);

	CSerializableBuffer *createOtherCharPacketMy = CGenPacket::makePacketSCCreateOtherCharacter(sessionId, newPlayer->m_bDirection, newPlayer->m_sX, newPlayer->m_sY, (BYTE)newPlayer->m_sHp);
	SendSector(sessionId, newPlayer->m_sSecY, newPlayer->m_sSecX, createOtherCharPacketMy);

	CPlayer::s_Sectors[newPlayer->m_sSecY][newPlayer->m_sSecX].insert(std::make_pair(sessionId, newPlayer));
	m_mapPlayers.insert(std::make_pair(sessionId, newPlayer));
}

void CGameServer::OnClientLeave(const UINT64 sessionId)
{
	CPlayer *delPlayer = m_mapPlayers[sessionId];
	// 섹터에서 지워주기
	CPlayer::s_Sectors[delPlayer->m_sSecY][delPlayer->m_sSecX].erase(sessionId);
	m_mapPlayers.erase(sessionId);

	CSerializableBuffer *deleteCharPacket = CGenPacket::makePacketSCDeleteCharacter(sessionId);
	SendPacket(sessionId, deleteCharPacket);

	delete delPlayer;
}

bool CGameServer::OnRecv(const UINT64 sessionId, CSerializableBuffer *message)
{
	BYTE packetCode;
	*message >> packetCode;
	if (!g_pProcessPacket->ConsumePacket((PACKET_CODE)packetCode, sessionId, message))
		return false;

	return true;
}

// CGameServer 클래스 멤버로 만든 이유
//		* CPlayer 객체의 멤버의 접근이 필요
//		* friend 클래스로 CGameServer 객체 내의 함수라면 접근을 허용
void CGameServer::NewPlayerGetOtherPlayerInfo(UINT64 newPlayerId, CPlayer *otherPlayer)
{
	CSerializableBuffer *createOtherCharPacket = CGenPacket::makePacketSCCreateOtherCharacter(otherPlayer->m_iId, otherPlayer->m_bDirection, otherPlayer->m_sX, otherPlayer->m_sY, (CHAR)otherPlayer->m_sHp);
	SendPacket(newPlayerId, createOtherCharPacket);

	if (otherPlayer->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
	{
		CSerializableBuffer *moveStartPacket = CGenPacket::makePacketSCMoveStart(otherPlayer->m_iId, otherPlayer->m_dwAction, otherPlayer->m_sX, otherPlayer->m_sY);
		SendPacket(newPlayerId, moveStartPacket);
	}

}
