#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"

bool CChatProcessPacket::PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CNonLoginPlayer> &umapNonLoginPlayer = m_pChatServer->m_umapNonLoginPlayer;
	AcquireSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	auto it = umapNonLoginPlayer.find(sessionId);
	if (it == umapNonLoginPlayer.end())
	{
		ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
		return false;
	}

	// 이 플레이어에 대한 접근은 어차피 자기 자신 세션의 OnRecv에서만
	// -> OnRecv 과정에서 RecvPost는 없으니깐 동기화 신경 안써도 됨
	// -> Map에만 등록하고 설정은 나중에 해도 됨
	CPlayer *loginPlayer = CPlayer::Alloc();

	m_pChatServer->m_umapNonLoginPlayer.erase(sessionId);
	m_pChatServer->m_umapLoginPlayer.insert(std::make_pair(sessionId, loginPlayer));
	ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);

	INT64 accountNo;
	*message >> accountNo;

	loginPlayer->m_iAccountNo = accountNo;
	loginPlayer->m_dwPrevRecvTime = timeGetTime();

	message->Dequeue((char *)loginPlayer->m_szID, sizeof(WCHAR) * 20);
	message->Dequeue((char *)loginPlayer->m_szNickname, sizeof(WCHAR) * 20);

	char sessionKey[64];
	message->Dequeue(sessionKey, sizeof(char) * 64);

	if (message->GetDataSize() != 0)
	{
		return false;
	}

	CSerializableBuffer<FALSE> *loginRes = CGenPacket::makePacketResLogin(TRUE, accountNo);
	loginRes->IncreaseRef();
	m_pChatServer->SendPacket(sessionId, loginRes);
	if (loginRes->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(loginRes);

	return true;
}

// 동기화 필요함
bool CChatProcessPacket::PacketProcReqSectorMove(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	AcquireSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
		return false;
	}

	CPlayer *player = it->second;
	// it 접근 끝
	ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD sectorX;
	WORD sectorY;
	
	// size 체크
	if (message->GetDataSize() < sizeof(INT64) + sizeof(WORD) + sizeof(WORD))
	{
		return false;
	}
	
	*message >> accountNo >> sectorX >> sectorY;

	if (accountNo != player->m_iAccountNo)
	{
		return false;
	}

	if (message->GetDataSize() != 0)
	{
		return false;
	}

	if (sectorX >= MAX_SECTOR_X || sectorY >= MAX_SECTOR_Y)
	{
		return false;
	}

	if (player->m_usSectorY != 0xFF && player->m_usSectorX != 0xFF)
	{
		// 같은 경우엔 아무것도 안함
		if (!(player->m_usSectorY == sectorY && player->m_usSectorX == sectorX))
		{
			// 락 순서 직렬화
			USHORT lock1Y, lock1X, lock2Y, lock2X;
			if (player->m_usSectorY == sectorY)
			{
				if (player->m_usSectorX < sectorX)
				{
					lock1X = player->m_usSectorX;
					lock2X = sectorX;
				}
				else
				{
					lock1X = sectorX;
					lock2X = player->m_usSectorX;
				}

				lock1Y = sectorY;
				lock2Y = sectorY;
			}
			else
			{
				if (player->m_usSectorY < sectorY)
				{
					lock1Y = player->m_usSectorY;
					lock1X = player->m_usSectorX;

					lock2Y = sectorY;
					lock2X = sectorX;
				}
				else
				{
					lock1Y = sectorY;
					lock1X = sectorX;

					lock2Y = player->m_usSectorY;
					lock2X = player->m_usSectorX;
				}
			}

			AcquireSRWLockExclusive(&m_pChatServer->m_arrCSector[lock1Y][lock1X].m_srwLock);
			AcquireSRWLockExclusive(&m_pChatServer->m_arrCSector[lock2Y][lock2X].m_srwLock);

			m_pChatServer->m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_players.erase(sessionId);
			m_pChatServer->m_arrCSector[sectorY][sectorX].m_players.insert(std::make_pair(sessionId, player));

			ReleaseSRWLockExclusive(&m_pChatServer->m_arrCSector[lock2Y][lock2X].m_srwLock);
			ReleaseSRWLockExclusive(&m_pChatServer->m_arrCSector[lock1Y][lock1X].m_srwLock);
		}
	}
	else
	{
		AcquireSRWLockExclusive(&m_pChatServer->m_arrCSector[sectorY][sectorX].m_srwLock);
		m_pChatServer->m_arrCSector[sectorY][sectorX].m_players.insert(std::make_pair(sessionId, player));
		ReleaseSRWLockExclusive(&m_pChatServer->m_arrCSector[sectorY][sectorX].m_srwLock);
	}

	player->m_usSectorY = sectorY;
	player->m_usSectorX = sectorX;

	CSerializableBuffer<FALSE> *sectorMoveRes = CGenPacket::makePacketResSectorMove(accountNo, sectorX, sectorY);
	sectorMoveRes->IncreaseRef();
	m_pChatServer->SendPacket(sessionId, sectorMoveRes);
	if (sectorMoveRes->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(sectorMoveRes);

	return true;
}

bool CChatProcessPacket::PacketProcReqMessage(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	AcquireSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
		return false;
	}

	CPlayer *player = it->second;
	ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	player->m_dwPrevRecvTime = timeGetTime();

	INT64 accountNo;
	WORD messageLen;
	*message >> accountNo >> messageLen;

	if (message->GetDataSize() != messageLen)
	{
		return false;
	}

	if (accountNo != player->m_iAccountNo)
	{
		return false;
	}
	


	WCHAR chatMessage[256];
	message->Dequeue((char *)chatMessage, messageLen);

	if (message->GetDataSize() != 0)
	{
		return false;
	}


	CSerializableBuffer<FALSE> *messageRes = CGenPacket::makePacketResMessage(accountNo, player->m_szID, player->m_szNickname, messageLen, chatMessage);
	messageRes->IncreaseRef();
	messageRes->SetSessionId(sessionId);
	// 응답 패킷 자기 자신한텐 그 즉시
	InterlockedIncrement(&g_monitor.m_chatMsgRes);
	m_pChatServer->SendPacket(sessionId, messageRes);
	// m_pChatServer->SendSector(sessionId, player->m_usSectorY, player->m_usSectorX, messageRes);
	messageRes->IncreaseRef();
	m_pChatServer->m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_sendMsgLFQ.Enqueue(messageRes);

	if (messageRes->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(messageRes);

	// if (messageRes->DecreaseRef() == 0)
	// 	CSerializableBuffer<FALSE>::Free(messageRes);
	return true;
}

bool CChatProcessPacket::PacketProcReqHeartBeat(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CPlayer *> &umapPlayer = m_pChatServer->m_umapLoginPlayer;
	AcquireSRWLockExclusive(&m_pChatServer->m_playerMapLock);
	auto it = umapPlayer.find(sessionId);
	if (it == umapPlayer.end())
	{
		ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);
		return false;
	}

	CPlayer *player = it->second;
	ReleaseSRWLockExclusive(&m_pChatServer->m_playerMapLock);

	INT64 accountNo;
	*message >> accountNo;

	if (accountNo != player->m_iAccountNo)
	{
		return false;
	}

	if (message->GetDataSize() != 0)
	{
		return false;
	}

	player->m_dwPrevRecvTime = timeGetTime();
	return true;
}
