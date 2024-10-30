#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CSector.h"
#include "CPlayer.h"
#include "CServerCore.h"
#include "CGameServer.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"

CGameServer::CGameServer()
{
	m_pProcessPacket = new CGameProcessPacket;
	m_pProcessPacket->SetServer(this);
	CSector::s_pGameServer = this;
}

CGameServer::~CGameServer()
{
	delete m_pProcessPacket;
}

/*
	������ ���� ������ ��� �÷��̾�� ����
		* sessionId�� �÷��̾�� ����
		* sessionId�� 0�Ͻ� �ڽŵ� ����
			* sessionId�� 1���� �ο��ϱ� ������ 0�� ���ø��� ����
*/
BOOL CGameServer::SendSector(const UINT64 sessionId, INT secY, INT secX, CSerializableBuffer *message)
{
	int startY = secY - SECTOR_VIEW_START;
	int startX = secX - SECTOR_VIEW_START;

	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= SECTOR_MAX_Y || startX + x < 0 || startX + x >= SECTOR_MAX_X)
				continue;

			for (auto &player : CSector::s_Sectors[startY + y][startX + x])
			{
				CPlayer *otherPlayer = player.second;
				if (otherPlayer->m_iId == sessionId)
					continue;

				SendPacket(otherPlayer->m_iId, message);
			}
		}
	}

	return true;
}

/*
	���� ���� ���� �÷��̾ ������� ������ �ൿ ����
*/
BOOL CGameServer::FuncSector(const UINT64 sessionId, INT secY, INT secX, SectorFunc func)
{
	int startY = secY - SECTOR_VIEW_START;
	int startX = secX - SECTOR_VIEW_START;

	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= SECTOR_MAX_Y || startX + x < 0 || startX + x >= SECTOR_MAX_X)
				continue;

			for (auto &player : CSector::s_Sectors[startY + y][startX + x])
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
VOID CGameServer::OnAccept(const UINT64 sessionId)
{
	// ���� ��ǥ ����
	USHORT ranY = (rand() % (RANGE_MOVE_BOTTOM - RANGE_MOVE_TOP - 1)) + RANGE_MOVE_TOP + 1;
	USHORT ranX = (rand() % (RANGE_MOVE_RIGHT - RANGE_MOVE_LEFT - 1)) + RANGE_MOVE_LEFT + 1;

	// ranY = 1000;
	// ranX = 1000;

	// Player ��ü ����
	CPlayer *newPlayer = new CPlayer;
	newPlayer->Init(sessionId, ranY, ranX);

	CSerializableBuffer *createMyCharPacket = CGenPacket::makePacketSCCreateMyCharacter(sessionId, newPlayer->m_bDirection, newPlayer->m_sX, newPlayer->m_sY, (BYTE)newPlayer->m_sHp);
	if (!SendPacket(sessionId, createMyCharPacket))
		g_Logger->WriteLog(L"GameContent", LOG_LEVEL::ERR, L"OnAccept SendPacket Err SessionID : %d", sessionId);
	delete createMyCharPacket;

	// �ٸ� �÷��̾�� �ڽ��� ĳ���� ���� ������ ����
	// ���� ���
	int startY = newPlayer->m_sSecY - SECTOR_VIEW_START;
	int startX = newPlayer->m_sSecX - SECTOR_VIEW_START;

	// ���� ���� ��� �ٸ� ĳ������ ������ ����
	SectorFunc func = &CGameServer::NewPlayerGetOtherPlayerInfo;
	FuncSector(sessionId, newPlayer->m_sSecY, newPlayer->m_sSecX, func);

	// ���� ���� ��� �ٸ� ĳ���Ϳ��� �� ���� ������ ������
	CSerializableBuffer *createOtherCharPacketMy = CGenPacket::makePacketSCCreateOtherCharacter(sessionId, newPlayer->m_bDirection, newPlayer->m_sX, newPlayer->m_sY, (BYTE)newPlayer->m_sHp);
	SendSector(sessionId, newPlayer->m_sSecY, newPlayer->m_sSecX, createOtherCharPacketMy);
	delete createOtherCharPacketMy;

	CSector::s_Sectors[newPlayer->m_sSecY][newPlayer->m_sSecX].insert(std::make_pair(sessionId, newPlayer));
	m_mapPlayers.insert(std::make_pair(sessionId, newPlayer));
}

VOID CGameServer::OnClientLeave(const UINT64 sessionId)
{
	CPlayer *delPlayer = m_mapPlayers[sessionId];

	// ���Ϳ��� �����ֱ�
	CSector::s_Sectors[delPlayer->m_sSecY][delPlayer->m_sSecX].erase(sessionId);
	m_mapPlayers.erase(sessionId);

	CSerializableBuffer *deleteCharPacket = CGenPacket::makePacketSCDeleteCharacter(sessionId);
	SendSector(sessionId, delPlayer->m_sSecY, delPlayer->m_sSecX, deleteCharPacket);
	delete deleteCharPacket;

	delete delPlayer;
}

BOOL CGameServer::OnRecv(const UINT64 sessionId, CSerializableBuffer *message)
{
	BYTE packetCode;
	*message >> packetCode;
	if (!m_pProcessPacket->ConsumePacket((PACKET_CODE)packetCode, sessionId, message))
		return false;

	return true;
}

// CGameServer Ŭ���� ����� ���� ����
//		* CPlayer ��ü�� ����� ������ �ʿ�
//		* friend Ŭ������ CGameServer ��ü ���� �Լ���� ������ ���
VOID CGameServer::NewPlayerGetOtherPlayerInfo(UINT64 newPlayerId, CPlayer *otherPlayer)
{
	CSerializableBuffer *createOtherCharPacket = CGenPacket::makePacketSCCreateOtherCharacter(otherPlayer->m_iId, otherPlayer->m_bDirection, otherPlayer->m_sX, otherPlayer->m_sY, (CHAR)otherPlayer->m_sHp);
	SendPacket(newPlayerId, createOtherCharPacket);
	delete createOtherCharPacket;

	if (otherPlayer->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
	{
		CSerializableBuffer *moveStartPacket = CGenPacket::makePacketSCMoveStart(otherPlayer->m_iId, otherPlayer->m_dwAction, otherPlayer->m_sX, otherPlayer->m_sY);
		SendPacket(newPlayerId, moveStartPacket);
		delete moveStartPacket;
	}
}

CPlayer *CGameServer::GetCurrentTargetPlayerId(UINT64 playerId, INT dir, INT rangeY, INT rangeX)
{
	CPlayer *player = m_mapPlayers[playerId];
	int startY = player->m_sSecY - SECTOR_VIEW_START;
	int startX = player->m_sSecX - SECTOR_VIEW_START;

	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= SECTOR_MAX_Y || startX + x < 0 || startX + x >= SECTOR_MAX_X)
				continue;

			for (auto &targetPlayer : CSector::s_Sectors[startY + y][startX + x])
			{
				// �ڱ� �ڽ� pass
				if (targetPlayer.first == playerId)
					continue;

				CPlayer *pTargetPlayer = targetPlayer.second;
				int dY = abs(pTargetPlayer->m_sY - player->m_sY);
				int dX = (pTargetPlayer->m_sX - player->m_sX) * dir;

				if (dX < 0)
					continue;

				if (!(dX < rangeX && dY < rangeY))
					continue;

				// ������� ����ϸ� Ÿ���� ������ ��
				return pTargetPlayer;
			}
		}
	}

	return nullptr;
}

void CGameServer::Update()
{
	for (auto &player : m_mapPlayers)
	{
		switch ((MOVE_DIR)player.second->m_dwAction)
		{
			// ����
		case MOVE_DIR::MOVE_DIR_LL:
			player.second->Move(-SPEED_PLAYER_X, 0);
			break;

			// �밢�� ���� �Ʒ�
		case MOVE_DIR::MOVE_DIR_LU:
			player.second->Move(-SPEED_PLAYER_X, -SPEED_PLAYER_Y);
			break;

			// ��
		case MOVE_DIR::MOVE_DIR_UU:
			player.second->Move(0, -SPEED_PLAYER_Y);
			break;

		case MOVE_DIR::MOVE_DIR_RU:
			player.second->Move(SPEED_PLAYER_X, -SPEED_PLAYER_Y);
			break;

		case MOVE_DIR::MOVE_DIR_RR:
			player.second->Move(SPEED_PLAYER_X, 0);
			break;

		case MOVE_DIR::MOVE_DIR_RD:
			player.second->Move(SPEED_PLAYER_X, SPEED_PLAYER_Y);
			break;

		case MOVE_DIR::MOVE_DIR_DD:
			player.second->Move(0, SPEED_PLAYER_Y);
			break;

		case MOVE_DIR::MOVE_DIR_LD:
			player.second->Move(-SPEED_PLAYER_X, SPEED_PLAYER_Y);
			break;

		default:
			break;
		}
	}
}
