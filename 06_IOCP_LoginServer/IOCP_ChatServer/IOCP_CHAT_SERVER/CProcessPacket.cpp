#include "pch.h"
#include "CNetServer.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"
#include "DBSetting.h"
#include "CRedisConnector.h"

bool CChatProcessPacket::PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept
{
	std::unordered_map<UINT64, CNonLoginPlayer> &umapNonLoginPlayer = m_pChatServer->m_umapNonLoginPlayer;
	auto it = umapNonLoginPlayer.find(sessionId);
	if (it == umapNonLoginPlayer.end())
	{
		// APC 처리 유도
		SleepEx(0, TRUE);
		it = umapNonLoginPlayer.find(sessionId);
		if (it == umapNonLoginPlayer.end())
		{
			return false;
		}
	}

	INT64 accountNo;
	*message >> accountNo;

	CPlayer *loginPlayer = CPlayer::Alloc();
	loginPlayer->m_iAccountNo = accountNo;
	loginPlayer->m_dwPrevRecvTime = timeGetTime();

	message->Dequeue((char *)loginPlayer->m_szID, sizeof(WCHAR) * 20);
	message->Dequeue((char *)loginPlayer->m_szNickname, sizeof(WCHAR) * 20);

	char sessionKey[64];
	message->Dequeue(sessionKey, sizeof(char) * 64);

	if (message->GetDataSize() != 0)
		return false;

	// Redis 토큰 비교
	char getKey[64];
	CRedisConnector *redisConnector = CRedisConnector::GetRedisConnector();
	redisConnector->Get(accountNo, getKey, 64);
	redisConnector->Del(accountNo);

	for (int i = 0; i < 64; i++)
	{
		// 세션 키가 다르면 연결 끊기
		if (sessionKey[i] != getKey[i])
			return false;
	}

	m_pChatServer->m_umapNonLoginPlayer.erase(sessionId);
	m_pChatServer->m_umapLoginPlayer.insert(std::make_pair(sessionId, loginPlayer));

	CSerializableBuffer<SERVER_TYPE::WAN> *loginRes = CGenPacket::makePacketResLogin(TRUE, accountNo);
	m_pChatServer->SendPacket(sessionId, loginRes);
	return true;
}

bool CChatProcessPacket::PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		return false;
	}

	CPlayer *player = it->second;
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD sectorX;
	WORD sectorY;
	
	// size 체크
	if (message->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(WORD))
		return false;
	
	*message >> accountNo >> sectorX >> sectorY;

	if (accountNo != player->m_iAccountNo)
		return false;

	if (message->GetDataSize() != 0)
		return false;

	if (sectorX >= MAX_SECTOR_X || sectorY >= MAX_SECTOR_Y)
		return false;

	if (player->m_usSectorY != 0xFF && player->m_usSectorX != 0xFF)
	{
		m_pChatServer->m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_players.erase(sessionId);
		m_pChatServer->m_arrCSector[sectorY][sectorX].m_players.insert(std::make_pair(sessionId, player));
	}
	else
	{
		m_pChatServer->m_arrCSector[sectorY][sectorX].m_players.insert(std::make_pair(sessionId, player));
	}

	player->m_usSectorY = sectorY;
	player->m_usSectorX = sectorX;

	CSerializableBuffer<SERVER_TYPE::WAN> *sectorMoveRes = CGenPacket::makePacketResSectorMove(accountNo, sectorX, sectorY);
	sectorMoveRes->IncreaseRef();
	m_pChatServer->SendPacket(sessionId, sectorMoveRes);
	if (sectorMoveRes->DecreaseRef() == 0)
		CSerializableBuffer<SERVER_TYPE::WAN>::Free(sectorMoveRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		return false;
	}

	CPlayer *player = it->second;
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD messageLen;
	*message >> accountNo >> messageLen;

	if (message->GetDataSize() != messageLen)
		return false;

	if (accountNo != player->m_iAccountNo)
		return false;

	WCHAR chatMessage[256];
	message->Dequeue((char *)chatMessage, messageLen);

	if (message->GetDataSize() != 0)
		return false;

	CSerializableBuffer<SERVER_TYPE::WAN> *messageRes = CGenPacket::makePacketResMessage(accountNo, player->m_szID, player->m_szNickname, messageLen, chatMessage);
	{
		messageRes->IncreaseRef();
		messageRes->SetSessionId(sessionId);
		InterlockedIncrement(&g_monitor.m_chatMsgRes);
		m_pChatServer->SendPacket(sessionId, messageRes);
		m_pChatServer->SendSector(0, player->m_usSectorY, player->m_usSectorX, CSmartPtr<CSerializableBuffer<SERVER_TYPE::WAN>>(messageRes));
		if (messageRes->DecreaseRef() == 0)
			CSerializableBuffer<SERVER_TYPE::WAN>::Free(messageRes);
	}
	
	// m_pChatServer->m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_sendMsgQ.push_back(messageRes);

	// m_pChatServer->SendSector(0, player->m_usSectorY, player->m_usSectorX, messageRes);
	// if (messageRes->DecreaseRef() == 0)
	// 	CSerializableBuffer<FALSE>::Free(messageRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<SERVER_TYPE::WAN>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		return false;
	}


	INT64 accountNo;
	*message >> accountNo;

	if (accountNo != it->second->m_iAccountNo)
		return false;

	if (message->GetDataSize() != 0)
		return false;

	it->second->m_dwPrevRecvTime = timeGetTime();

	return true;
}
