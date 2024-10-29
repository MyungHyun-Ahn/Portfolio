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

void CSector::MoveSector(UINT64 playerId, int prevY, int prevX, int nowY, int nowX)
{
	int dX = nowX - prevX;
	int dY = nowY - prevY;

	int createPlayerX = nowX + SECTOR_VIEW_START * dX;
	int deletePlayerX = prevX + SECTOR_VIEW_START * dX * -1;

	int createPlayerY = nowY + SECTOR_VIEW_START * dY;
	int deletePlayerY = prevY + SECTOR_VIEW_START * dY * -1;

	// ���� �̵� - Y ���� ����
	if (prevY == nowY)
	{
		CPlayer *player = s_Sectors[prevY][prevX][playerId];
		s_Sectors[prevY][prevX].erase(playerId);
		s_Sectors[nowY][nowX].insert(std::make_pair(playerId, player));

		player->m_sSecY = nowY;
		player->m_sSecX = nowX;

		for (int y = 0; y < 3; y++)
		{
			// ���� �޽���
			do
			{
				if (deletePlayerX < 0 || deletePlayerX >= SECTOR_MAX_X)
					break;

				if (prevY + y - 1 < 0 || prevY + y - 1 >= SECTOR_MAX_Y)
					break;

				for (auto otherPlayer : s_Sectors[prevY + y - 1][deletePlayerX])
				{
					if (otherPlayer.first == player->m_iId)
						continue;

					// �ٸ� ĳ���� �������� �� ������!
					CSerializableBuffer *deletePacketMy = CGenPacket::makePacketSCDeleteCharacter(player->m_iId);
					s_pGameServer->SendPacket(otherPlayer.first, deletePacketMy);
					delete deletePacketMy;

					// �� �������� �ٸ� ĳ���͸� ������!
					CSerializableBuffer *deletePacketOther = CGenPacket::makePacketSCDeleteCharacter(otherPlayer.first);
					s_pGameServer->SendPacket(playerId, deletePacketOther);
					delete deletePacketOther;
				}
			} while (0);

			// ���� �޽���
			do
			{
				if (createPlayerX < 0 || createPlayerX >= SECTOR_MAX_X)
					break;

				if (nowY + y - 1 < 0 || nowY + y - 1 >= SECTOR_MAX_Y)
					break;

				for (auto otherPlayer : s_Sectors[nowY + y - 1][createPlayerX])
				{
					if (otherPlayer.first == player->m_iId)
						continue;

					// �� ĳ������ ������ �̹� ���Ϳ� �ִ� �÷��̾�� ������!
					CSerializableBuffer *createCharPacketMy = CGenPacket::makePacketSCCreateOtherCharacter(player->m_iId, player->m_bDirection, player->m_sX, player->m_sY, player->m_sHp);
					s_pGameServer->SendPacket(otherPlayer.first, createCharPacketMy);
					delete createCharPacketMy;

					if (player->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
					{
						CSerializableBuffer *moveStartPacketMy = CGenPacket::makePacketSCMoveStart(player->m_iId, (CHAR)player->m_dwAction, player->m_sX, player->m_sY);
						s_pGameServer->SendPacket(otherPlayer.first, moveStartPacketMy);
						delete moveStartPacketMy;
					}

					// �ٸ� �÷��̾��� ������ ������ ������!
					CSerializableBuffer *createCharPacketOther = CGenPacket::makePacketSCCreateOtherCharacter(otherPlayer.first, otherPlayer.second->m_bDirection, otherPlayer.second->m_sX, otherPlayer.second->m_sY, otherPlayer.second->m_sHp);
					s_pGameServer->SendPacket(player->m_iId, createCharPacketOther);
					delete createCharPacketOther;

					if (otherPlayer.second->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
					{
						CSerializableBuffer *moveStartPacketOther = CGenPacket::makePacketSCMoveStart(otherPlayer.first, (CHAR)otherPlayer.second->m_dwAction, otherPlayer.second->m_sX, otherPlayer.second->m_sY);
						s_pGameServer->SendPacket(otherPlayer.first, moveStartPacketOther);
						delete moveStartPacketOther;
					}
				}
			} while (0);
		}
	}
	// ���� �̵� - X ���� ����
	else if (prevX == nowX)
	{
		CPlayer *player = s_Sectors[prevY][prevX][playerId];
		s_Sectors[prevY][prevX].erase(playerId);
		s_Sectors[nowY][nowX].insert(std::make_pair(playerId, player));

		player->m_sSecY = nowY;
		player->m_sSecX = nowX;

		for (int x = 0; x < 3; x++)
		{
			// ���� �޽���
			do
			{
				if (deletePlayerY < 0 || deletePlayerY >= SECTOR_MAX_Y)
					break;

				if (prevX + x - 1 < 0 || prevX + x - 1 >= SECTOR_MAX_X)
					break;

				for (auto otherPlayer : s_Sectors[deletePlayerY][prevX + x - 1])
				{
					if (otherPlayer.first == player->m_iId)
						continue;

					// �ٸ� ĳ���� �������� �� ������!
					CSerializableBuffer *deletePacketMy = CGenPacket::makePacketSCDeleteCharacter(player->m_iId);
					s_pGameServer->SendPacket(otherPlayer.first, deletePacketMy);
					delete deletePacketMy;

					// �� �������� �ٸ� ĳ���͸� ������!
					CSerializableBuffer *deletePacketOther = CGenPacket::makePacketSCDeleteCharacter(otherPlayer.first);
					s_pGameServer->SendPacket(playerId, deletePacketOther);
					delete deletePacketOther;
				}
			} while (0);

			// ���� �޽���
			do
			{
				if (createPlayerY < 0 || createPlayerY >= SECTOR_MAX_Y)
					break;

				if (nowX + x - 1 < 0 || nowX + x - 1 >= SECTOR_MAX_X)
					break;

				for (auto otherPlayer : s_Sectors[createPlayerY][nowX + x - 1])
				{
					if (otherPlayer.first == player->m_iId)
						continue;

					// �� ĳ������ ������ �̹� ���Ϳ� �ִ� �÷��̾�� ������!
					CSerializableBuffer *createCharPacketMy = CGenPacket::makePacketSCCreateOtherCharacter(player->m_iId, player->m_bDirection, player->m_sX, player->m_sY, player->m_sHp);
					s_pGameServer->SendPacket(otherPlayer.first, createCharPacketMy);
					delete createCharPacketMy;

					if (player->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
					{
						CSerializableBuffer *moveStartPacketMy = CGenPacket::makePacketSCMoveStart(player->m_iId, (CHAR)player->m_dwAction, player->m_sX, player->m_sY);
						s_pGameServer->SendPacket(otherPlayer.first, moveStartPacketMy);
						delete moveStartPacketMy;
					}

					// �ٸ� �÷��̾��� ������ ������ ������!
					CSerializableBuffer *createCharPacketOther = CGenPacket::makePacketSCCreateOtherCharacter(otherPlayer.first, otherPlayer.second->m_bDirection, otherPlayer.second->m_sX, otherPlayer.second->m_sY, otherPlayer.second->m_sHp);
					s_pGameServer->SendPacket(player->m_iId, createCharPacketOther);
					delete createCharPacketOther;

					if (otherPlayer.second->m_dwAction != (CHAR)MOVE_DIR::MOVE_DIR_STOP)
					{
						CSerializableBuffer *moveStartPacketOther = CGenPacket::makePacketSCMoveStart(otherPlayer.first, (CHAR)otherPlayer.second->m_dwAction, otherPlayer.second->m_sX, otherPlayer.second->m_sY);
						s_pGameServer->SendPacket(otherPlayer.first, moveStartPacketOther);
						delete moveStartPacketOther;
					}
				}
			} while (0);
		}
	}
	else // �밢�� �̵�
	{
		// �ܼ��� �̵� ��Ű�� ������ �ذ�
		MoveSector(playerId, prevY, prevX, prevY, nowX);
		MoveSector(playerId, prevY, nowX, nowY, nowX);
	}
}

