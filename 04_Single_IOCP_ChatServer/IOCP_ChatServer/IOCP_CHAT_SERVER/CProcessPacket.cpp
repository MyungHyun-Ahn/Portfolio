#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"

bool CChatProcessPacket::PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
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
			// 이번에도 없으면 버그
			m_pChatServer->Disconnect(sessionId);
			return true;
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

	m_pChatServer->m_umapNonLoginPlayer.erase(sessionId);
	m_pChatServer->m_umapLoginPlayer.insert(std::make_pair(sessionId, loginPlayer));

	CSerializableBuffer<FALSE> *loginRes = CGenPacket::makePacketResLogin(TRUE, accountNo);
	m_pChatServer->SendPacket(sessionId, loginRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		m_pChatServer->Disconnect(sessionId);
		return true;
	}

	CPlayer *player = it->second;
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD sectorX;
	WORD sectorY;

	*message >> accountNo >> sectorX >> sectorY;

	if (player->m_usSectorY != 0xFF && player->m_usSectorX != 0xFF)
		m_pChatServer->m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_players.erase(sessionId);
	m_pChatServer->m_arrCSector[sectorY][sectorX].m_players.insert(std::make_pair(sessionId, player));
	player->m_usSectorY = sectorY;
	player->m_usSectorX = sectorX;

	CSerializableBuffer<FALSE> *sectorMoveRes = CGenPacket::makePacketResSectorMove(accountNo, sectorX, sectorY);
	m_pChatServer->SendPacket(sessionId, sectorMoveRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		m_pChatServer->Disconnect(sessionId);
		return true;
	}

	CPlayer *player = it->second;
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD messageLen;
	*message >> accountNo >> messageLen;
	WCHAR chatMessage[256];
	message->Dequeue((char *)chatMessage, messageLen);

	CSerializableBuffer<FALSE> *messageRes = CGenPacket::makePacketResMessage(accountNo, player->m_szID, player->m_szNickname, messageLen, chatMessage);

	messageRes->IncreaseRef();
	
	m_pChatServer->SendSector(NULL, player->m_usSectorY, player->m_usSectorX, messageRes);

	if (messageRes->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(messageRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		m_pChatServer->Disconnect(sessionId);
		return true;
	}

	it->second->m_dwPrevRecvTime = timeGetTime();

	return true;
}
