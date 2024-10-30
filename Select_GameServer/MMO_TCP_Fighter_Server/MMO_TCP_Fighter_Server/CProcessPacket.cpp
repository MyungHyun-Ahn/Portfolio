#include "pch.h"
#include "Config.h"
#include "CSession.h"
#include "DefinePacket.h"
#include "CServerCore.h"
#include "CSector.h"
#include "CPlayer.h"
#include "CGameServer.h"
#include "CGenPacket.h"
#include "CProcessPacket.h"

// ���ڷ� �Ѿ���� ����ȭ ����
// * CServerCore ���� �Ҵ�� ������
// * CServerCore�� �Ҵ�� ������ ��� ������
// * ��, CProcessPacket������ �����ϸ� ���� �ȵ�

bool CGameProcessPacket::PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;
	// PacketCode�� �̹� ������ ����� �Ѿ�� - CGameServer �� OnRecv �Լ�

	CHAR moveDir;
	USHORT x;
	USHORT y;

	*message >> moveDir >> x >> y;

	// ���� ID�� player ã��
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];

	// ���� ���� üũ
	if (abs(player->m_sY - y) > ERROR_RANGE ||
		abs(player->m_sX - x) > ERROR_RANGE)
	{
		// ��� �÷��̾� ��ǥ�� sync �޽���
		CSerializableBuffer *syncMessage = CGenPacket::makePacketSCSync(player->m_iId, player->m_sX, player->m_sY);
		pGameServer->SendPacket(sessionId, syncMessage);
		delete syncMessage;
		g_Logger->WriteLog(L"Sync", LOG_LEVEL::SYSTEM, L"[START] PlayerId : %d, Server X : %d, Y : %d | Client X : %d, Y : %d", sessionId, player->m_sX, player->m_sY, x, y);

		// ���� ��ǥ�� ������
		x = player->m_sX;
		y = player->m_sY;
	}

	// ������ ���� �ȳ� �� Ŭ���̾�Ʈ ��ǥ�� ����
	player->m_dwAction = (DWORD)moveDir;
	player->m_sX = x;
	player->m_sY = y;

	// ���� �������� ������ ���� ����
	// ���� ���̺��� ����� �������� ���� �� ����
	// - �����ַ� �ߴµ�...
	// - �п����� ������ Ŭ���̾�Ʈ�� ��Ŷ �ѹ��� �̹� ���ǵ� ��ȣ
	// - ����� Ȯ�� ��� if else�� ���������� ����
	switch ((MOVE_DIR)moveDir)
	{
	case MOVE_DIR::MOVE_DIR_RR:
	case MOVE_DIR::MOVE_DIR_RU:
	case MOVE_DIR::MOVE_DIR_RD:
		player->m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_RR;
		break;

	case MOVE_DIR::MOVE_DIR_LU:
	case MOVE_DIR::MOVE_DIR_LL:
	case MOVE_DIR::MOVE_DIR_LD:
		player->m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_LL;
		break;
	}

	int nowSecY = CSector::CalSectorY(player->m_sY);
	int nowSecX = CSector::CalSectorX(player->m_sX);

	if (nowSecY != player->m_sSecY || nowSecX != player->m_sSecX)
	{
		// ���Ͱ� �̵��� ���
		// g_Logger->WriteLog(L"MoveSector", LOG_LEVEL::SYSTEM, L"[PacketProcCSMoveStart] %d %d %d %d", player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
		CSector::MoveSector(sessionId, player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
	}

	// �ش��ϴ� ���Ϳ� MoveStart �޽��� ����
	CSerializableBuffer *moveStartPacket = CGenPacket::makePacketSCMoveStart(sessionId, (CHAR)player->m_dwAction, player->m_sX, player->m_sY);
	pGameServer->SendSector(sessionId, nowSecY, nowSecX, moveStartPacket);
	delete moveStartPacket;

	return true;
}

bool CGameProcessPacket::PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;

	CHAR viewDir;
	USHORT x;
	USHORT y;
	*message >> viewDir >> x >> y;

	// ���� ID�� player ã��
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];

	// ���� ���� üũ
	if (abs(player->m_sY - y) > ERROR_RANGE ||
		abs(player->m_sX - x) > ERROR_RANGE)
	{
		// ��� �÷��̾� ��ǥ�� sync �޽���
		CSerializableBuffer *syncMessage = CGenPacket::makePacketSCSync(player->m_iId, player->m_sX, player->m_sY);
		pGameServer->SendPacket(sessionId, syncMessage);
		delete syncMessage;
		g_Logger->WriteLog(L"Sync", LOG_LEVEL::SYSTEM, L"[STOP] PlayerId : %d, Server X : %d, Y : %d | Client X : %d, Y : %d", sessionId, player->m_sX, player->m_sY, x, y);

		// ���� ��ǥ�� ������
		x = player->m_sX;
		y = player->m_sY;
	}

	// ������ ���� �ȳ� �� Ŭ���̾�Ʈ ��ǥ�� ����
	player->m_dwAction = (DWORD)MOVE_DIR::MOVE_DIR_STOP;
	player->m_sX = x;
	player->m_sY = y;

	// ���� �������� ������ ���� ����
	// ���� ���̺��� ����� �������� ���� �� ����
	switch ((MOVE_DIR)viewDir)
	{
	case MOVE_DIR::MOVE_DIR_RR:
	case MOVE_DIR::MOVE_DIR_RU:
	case MOVE_DIR::MOVE_DIR_RD:
		player->m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_RR;
		break;

	case MOVE_DIR::MOVE_DIR_LU:
	case MOVE_DIR::MOVE_DIR_LL:
	case MOVE_DIR::MOVE_DIR_LD:
		player->m_bDirection = (BYTE)MOVE_DIR::MOVE_DIR_LL;
		break;
	}

	int nowSecY = CSector::CalSectorY(player->m_sY);
	int nowSecX = CSector::CalSectorX(player->m_sX);

	if (nowSecY != player->m_sSecY || nowSecX != player->m_sSecX)
	{
		// ���Ͱ� �̵��� ���
		// g_Logger->WriteLog(L"MoveSector", LOG_LEVEL::SYSTEM, L"[PacketProcCSMoveStop] %d %d %d %d", player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
		CSector::MoveSector(sessionId, player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
	}

	// �ش��ϴ� ���Ϳ� MoveStart �޽��� ����
	CSerializableBuffer *moveStopPacket = CGenPacket::makePacketSCMoveStop(sessionId, (CHAR)player->m_dwAction, player->m_sX, player->m_sY);
	pGameServer->SendSector(sessionId, nowSecY, nowSecX, moveStopPacket);
	delete moveStopPacket;

	return true;
}

bool CGameProcessPacket::PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;

	CHAR direction;
	USHORT playerX;
	USHORT playerY;

	*message >> direction >> playerX >> playerY;

	// ���� ���� ���
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer Ŭ������ FuncSector�� ����Ϸ� ����
	// * �׷��� ���� �߰� �Ű������� �ʿ��ؼ� ���� ����
	// * ������ �ش��ϴ� �÷��̾� ��ü �Ѱ��� �������� �Լ��� ������ ����
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];
	CPlayer *targetPlayer = pGameServer->GetCurrentTargetPlayerId(sessionId, dir, ATTACK1_RANGE_Y, ATTACK1_RANGE_X);
	if (targetPlayer != nullptr)
	{
		bool targetAlive = targetPlayer->GetDamage(ATTACK1_DAMAGE);
		if (!targetAlive)
		{
			pGameServer->Disconnect(sessionId);
		}

		CSerializableBuffer *damagePacket = CGenPacket::makePacketSCDamage(sessionId, targetPlayer->m_iId, targetPlayer->m_sHp);
		pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, damagePacket);
		delete damagePacket;
	}

	CSerializableBuffer *attackPacket = CGenPacket::makePacketSCAttack1(sessionId, direction, playerX, playerY);
	pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, attackPacket);
	delete attackPacket;

	return true;
}

bool CGameProcessPacket::PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;

	CHAR direction;
	USHORT playerX;
	USHORT playerY;

	*message >> direction >> playerX >> playerY;

	// ���� ���� ���
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer Ŭ������ FuncSector�� ����Ϸ� ����
	// * �׷��� ���� �߰� �Ű������� �ʿ��ؼ� ���� ����
	// * ������ �ش��ϴ� �÷��̾� ��ü �Ѱ��� �������� �Լ��� ������ ����
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];
	CPlayer *targetPlayer = pGameServer->GetCurrentTargetPlayerId(sessionId, dir, ATTACK2_RANGE_Y, ATTACK2_RANGE_X);
	if (targetPlayer != nullptr)
	{
		bool targetAlive = targetPlayer->GetDamage(ATTACK2_DAMAGE);
		if (!targetAlive)
		{
			pGameServer->Disconnect(sessionId);
		}

		CSerializableBuffer *damagePacket = CGenPacket::makePacketSCDamage(sessionId, targetPlayer->m_iId, targetPlayer->m_sHp);
		pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, damagePacket);
		delete damagePacket;
	}
	
	CSerializableBuffer *attackPacket = CGenPacket::makePacketSCAttack2(sessionId, direction, playerX, playerY);
	pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, attackPacket);
	delete attackPacket;


	return true;
}

bool CGameProcessPacket::PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;

	CHAR direction;
	USHORT playerX;
	USHORT playerY;

	*message >> direction >> playerX >> playerY;

	// ���� ���� ���
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer Ŭ������ FuncSector�� ����Ϸ� ����
	// * �׷��� ���� �߰� �Ű������� �ʿ��ؼ� ���� ����
	// * ������ �ش��ϴ� �÷��̾� ��ü �Ѱ��� �������� �Լ��� ������ ����
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];
	CPlayer *targetPlayer = pGameServer->GetCurrentTargetPlayerId(sessionId, dir, ATTACK3_RANGE_Y, ATTACK3_RANGE_X);
	if (targetPlayer != nullptr)
	{
		bool targetAlive = targetPlayer->GetDamage(ATTACK3_DAMAGE);
		if (!targetAlive)
		{
			pGameServer->Disconnect(sessionId);
		}

		CSerializableBuffer *damagePacket = CGenPacket::makePacketSCDamage(sessionId, targetPlayer->m_iId, targetPlayer->m_sHp);
		pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, damagePacket);
		delete damagePacket;
	}
	
	CSerializableBuffer *attackPacket = CGenPacket::makePacketSCAttack3(sessionId, direction, playerX, playerY);
	pGameServer->SendSector(NULL, player->m_sSecY, player->m_sSecX, attackPacket);
	delete attackPacket;

	return true;
}

bool CGameProcessPacket::PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;

	DWORD time;
	*message >> time;

	CSerializableBuffer *echoPacket = CGenPacket::makePacketSCEcho(time);
	pGameServer->SendPacket(sessionId, echoPacket);
	delete echoPacket;

	return true;
}
