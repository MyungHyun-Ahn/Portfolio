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

// 인자로 넘어오는 직렬화 버퍼
// * CServerCore 에서 할당된 버퍼임
// * CServerCore가 할당과 생성을 모두 관리함
// * 즉, CProcessPacket에서는 삭제하면 절대 안됨

bool CGameProcessPacket::PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message)
{
	CGameServer *pGameServer = (CGameServer *)m_pServerCore;
	// PacketCode는 이미 빼내고 여기로 넘어옴 - CGameServer 측 OnRecv 함수

	CHAR moveDir;
	USHORT x;
	USHORT y;

	*message >> moveDir >> x >> y;

	// 세션 ID로 player 찾기
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];

	// 오차 범위 체크
	if (abs(player->m_sY - y) > ERROR_RANGE ||
		abs(player->m_sX - x) > ERROR_RANGE)
	{
		// 대상 플레이어 좌표로 sync 메시지
		CSerializableBuffer *syncMessage = CGenPacket::makePacketSCSync(player->m_iId, player->m_sX, player->m_sY);
		pGameServer->SendPacket(sessionId, syncMessage);
		delete syncMessage;
		g_Logger->WriteLog(L"Sync", LOG_LEVEL::SYSTEM, L"[START] PlayerId : %d, Server X : %d, Y : %d | Client X : %d, Y : %d", sessionId, player->m_sX, player->m_sY, x, y);

		// 서버 좌표로 맞춰줌
		x = player->m_sX;
		y = player->m_sY;
	}

	// 오차가 별로 안날 땐 클라이언트 좌표를 믿음
	player->m_dwAction = (DWORD)moveDir;
	player->m_sX = x;
	player->m_sY = y;

	// 지금 열거형의 순서가 맞지 않음
	// 점프 테이블이 제대로 형성되지 않을 수 있음
	// - 맞춰주려 했는데...
	// - 학원에서 제공된 클라이언트의 패킷 넘버에 이미 정의된 번호
	// - 어셈블리 확인 결과 if else로 동작하지는 않음
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
		// 섹터가 이동된 경우
		// g_Logger->WriteLog(L"MoveSector", LOG_LEVEL::SYSTEM, L"[PacketProcCSMoveStart] %d %d %d %d", player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
		CSector::MoveSector(sessionId, player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
	}

	// 해당하는 섹터에 MoveStart 메시지 전달
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

	// 세션 ID로 player 찾기
	CPlayer *player = pGameServer->m_mapPlayers[sessionId];

	// 오차 범위 체크
	if (abs(player->m_sY - y) > ERROR_RANGE ||
		abs(player->m_sX - x) > ERROR_RANGE)
	{
		// 대상 플레이어 좌표로 sync 메시지
		CSerializableBuffer *syncMessage = CGenPacket::makePacketSCSync(player->m_iId, player->m_sX, player->m_sY);
		pGameServer->SendPacket(sessionId, syncMessage);
		delete syncMessage;
		g_Logger->WriteLog(L"Sync", LOG_LEVEL::SYSTEM, L"[STOP] PlayerId : %d, Server X : %d, Y : %d | Client X : %d, Y : %d", sessionId, player->m_sX, player->m_sY, x, y);

		// 서버 좌표로 맞춰줌
		x = player->m_sX;
		y = player->m_sY;
	}

	// 오차가 별로 안날 땐 클라이언트 좌표를 믿음
	player->m_dwAction = (DWORD)MOVE_DIR::MOVE_DIR_STOP;
	player->m_sX = x;
	player->m_sY = y;

	// 지금 열거형의 순서가 맞지 않음
	// 점프 테이블이 제대로 형성되지 않을 수 있음
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
		// 섹터가 이동된 경우
		// g_Logger->WriteLog(L"MoveSector", LOG_LEVEL::SYSTEM, L"[PacketProcCSMoveStop] %d %d %d %d", player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
		CSector::MoveSector(sessionId, player->m_sSecY, player->m_sSecX, nowSecY, nowSecX);
	}

	// 해당하는 섹터에 MoveStart 메시지 전달
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

	// 공격 방향 계산
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer 클래스의 FuncSector를 사용하려 했음
	// * 그러나 각종 추가 매개변수가 필요해서 쉽지 않음
	// * 범위에 해당하는 플레이어 객체 한개만 가져오는 함수를 만들기로 결정
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

	// 공격 방향 계산
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer 클래스의 FuncSector를 사용하려 했음
	// * 그러나 각종 추가 매개변수가 필요해서 쉽지 않음
	// * 범위에 해당하는 플레이어 객체 한개만 가져오는 함수를 만들기로 결정
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

	// 공격 방향 계산
	MOVE_DIR eDir = (MOVE_DIR)direction;
	int dir = (eDir == MOVE_DIR::MOVE_DIR_RR ? 1 : -1);

	// CGameServer 클래스의 FuncSector를 사용하려 했음
	// * 그러나 각종 추가 매개변수가 필요해서 쉽지 않음
	// * 범위에 해당하는 플레이어 객체 한개만 가져오는 함수를 만들기로 결정
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
