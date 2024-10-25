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
	������ ���� ������ ��� �÷��̾�� ����
		* sessionId�� �÷��̾�� ����
		* sessionId�� 0�Ͻ� �ڽŵ� ����
			* sessionId�� 1���� �ο��ϱ� ������ 0�� ���ø��� ����
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
	���� ���� ���� �÷��̾ ������� ������ �ൿ ����
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

// ���⼭ Player ���� �۾��� ����
void CGameServer::OnAccept(const UINT64 sessionId)
{
	// ���� ��ǥ ����
	USHORT ranY = (rand() % (RANGE_MOVE_BOTTOM - RANGE_MOVE_TOP - 1)) + RANGE_MOVE_TOP + 1;
	USHORT ranX = (rand() % (RANGE_MOVE_RIGHT - RANGE_MOVE_LEFT - 1)) + RANGE_MOVE_LEFT + 1;

	// Player ��ü ����
	CPlayer *newPlayer = new CPlayer;
	newPlayer->Init(sessionId, ranY, ranX);

	CSerializableBuffer *createMyCharPacket = CGenPacket::makePacketSCCreateMyCharacter(sessionId, newPlayer->m_bDirection, newPlayer->m_sX, newPlayer->m_sY, (BYTE)newPlayer->m_sHp);
	if (!SendPacket(sessionId, createMyCharPacket))
		g_Logger->WriteLog(L"GameContent", LOG_LEVEL::ERR, L"OnAccept SendPacket Err SessionID : %d", sessionId);

	// �ٸ� �÷��̾�� �ڽ��� ĳ���� ���� ������ ����
	// ���� ���
	int startY = newPlayer->m_sSecY - SECTOR_VIEW_START;
	int startX = newPlayer->m_sSecX - SECTOR_VIEW_START;

	// ���� ���� ��� �ٸ� ĳ���Ϳ��� �� ���� ������ ������
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
	// ���Ϳ��� �����ֱ�
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

// CGameServer Ŭ���� ����� ���� ����
//		* CPlayer ��ü�� ����� ������ �ʿ�
//		* friend Ŭ������ CGameServer ��ü ���� �Լ���� ������ ���
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
