#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "ContentEvent.h"

CChatServer::CChatServer() noexcept
{
	m_pProcessPacket = new CChatProcessPacket;
	m_pProcessPacket->SetChatServer(this);
	InitializeSRWLock(&m_playerMapLock);
	InitializeSRWLock(&m_nonLoginplayerMapLock);

	// 싱글에서 수행됨
	m_umapNonLoginPlayer.reserve(20000);
	m_umapLoginPlayer.reserve(20000);
}

void CChatServer::NonLoginHeartBeat() noexcept
{
	AcquireSRWLockShared(&m_nonLoginplayerMapLock);
	DWORD nowTime = timeGetTime();
	for (auto it = m_umapNonLoginPlayer.begin(); it != m_umapNonLoginPlayer.end(); ++it)
	{
		DWORD dTime = nowTime - it->second.m_dwPrevRecvTime;
		if (dTime > NON_LOGIN_TIME_OUT)
			Disconnect(it->first, TRUE); // ReleaseSession을 PQCS로 우회
	}
	ReleaseSRWLockShared(&m_nonLoginplayerMapLock);
}

void CChatServer::LoginHeartBeat() noexcept
{
	AcquireSRWLockShared(&m_playerMapLock);
	DWORD nowTime = timeGetTime();
	for (auto it = m_umapLoginPlayer.begin(); it != m_umapLoginPlayer.end(); ++it)
	{
		CPlayer *player = it->second;
		DWORD dTime = nowTime - player->m_dwPrevRecvTime;
		if (dTime > LOGIN_TIME_OUT)
			Disconnect(it->first, TRUE);
	}
	ReleaseSRWLockShared(&m_playerMapLock);
}

void CChatServer::SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, CSerializableBuffer<FALSE> *message) noexcept
{
	int sectorCount = 0;
	CSector *sectors[3 * 3];

	int startY = sectorY - SECTOR_VIEW_START;
	int startX = sectorX - SECTOR_VIEW_START;
	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= MAX_SECTOR_Y || startX + x < 0 || startX + x >= MAX_SECTOR_X)
				continue;

			sectors[sectorCount++] = &m_arrCSector[y + startY][x + startX];

			AcquireSRWLockShared(&m_arrCSector[y + startY][x + startX].m_srwLock);
		}
	}

	for (int i = 0; i < sectorCount; i++)
	{
		auto &playerMap = sectors[i]->m_players;
		for (auto it = playerMap.begin(); it != playerMap.end(); ++it)
		{
			if (message->GetSessionId() == it->first)
				continue;

			EnqueuePacket(it->first, message);
		}
	}

	for (int i = sectorCount - 1; i >= 0; i--)
	{
		ReleaseSRWLockShared(&sectors[i]->m_srwLock);
	}
}

bool CChatServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}


void CChatServer::OnAccept(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);
	CNonLoginPlayer nonLogin = { timeGetTime() };
	AcquireSRWLockExclusive(&m_nonLoginplayerMapLock);
	m_umapNonLoginPlayer.insert(std::make_pair(sessionID, nonLogin));
	ReleaseSRWLockExclusive(&m_nonLoginplayerMapLock);
}

void CChatServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);

	AcquireSRWLockExclusive(&m_nonLoginplayerMapLock);
	auto it1 = m_umapNonLoginPlayer.find(sessionID);
	if (it1 != m_umapNonLoginPlayer.end())
	{
		m_umapNonLoginPlayer.erase(sessionID);
		ReleaseSRWLockExclusive(&m_nonLoginplayerMapLock);
		return;
	}
	ReleaseSRWLockExclusive(&m_nonLoginplayerMapLock);


	AcquireSRWLockExclusive(&m_playerMapLock);
	auto it2 = m_umapLoginPlayer.find(sessionID);
	if (it2 != m_umapLoginPlayer.end())
	{
		CPlayer *player = it2->second;

		// 섹터에서 제거
		if (player->m_usSectorY != 0xFF && player->m_usSectorX != 0xFF)
		{
			AcquireSRWLockExclusive(&m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_srwLock);
			m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_players.erase(sessionID);
			ReleaseSRWLockExclusive(&m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_srwLock);
		}

		m_umapLoginPlayer.erase(sessionID);
		ReleaseSRWLockExclusive(&m_playerMapLock);
		CPlayer::Free(player);
		return;
	}

	__debugbreak();
	ReleaseSRWLockExclusive(&m_playerMapLock);
}

void CChatServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);
	message->IncreaseRef();

	WORD type;
	*message >> type;
	if (!m_pProcessPacket->ConsumPacket((en_PACKET_TYPE)type, message->GetSessionID(), message))
	{
		Disconnect(message->GetSessionID());
	}

	if (message->DecreaseRef() == 0)
		CSerializableBufferView<FALSE>::Free(message);
}

void CChatServer::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	
}

void CChatServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

void CChatServer::RegisterContentEvent() noexcept
{
	SectorBroadcastEvent *sectorBroadcastEvent = new SectorBroadcastEvent;
	sectorBroadcastEvent->SetEvent();
	RegisterTimerEvent((BaseEvent *)sectorBroadcastEvent);

	// NonLoginHeartBeatEvent *nonLoginHeartBeatEvent = new NonLoginHeartBeatEvent;
	// nonLoginHeartBeatEvent->SetEvent();
	// RegisterTimerEvent((BaseEvent *)nonLoginHeartBeatEvent);

	// LoginHeartBeatEvent *loginHeartBeatEvent = new LoginHeartBeatEvent;
	// loginHeartBeatEvent->SetEvent();
	// RegisterTimerEvent((BaseEvent *)loginHeartBeatEvent);
}

void CChatServer::SectorBroadcast() noexcept
{
	for (int sectorY = 0; sectorY < MAX_SECTOR_Y; sectorY++)
	{
		for (int sectorX = 0; sectorX < MAX_SECTOR_X; sectorX++)
		{
			// 메시지 큐 확인
			CLFQueue<CSerializableBuffer<FALSE> *> &msgQ = m_arrCSector[sectorY][sectorX].m_sendMsgLFQ;
			LONG useSize = msgQ.GetUseSize();
			if (useSize == 0)
				continue;

			// 보낼게 있으면
			// 락 획득하고

			int sectorCount = 0;
			CSector *sectors[3 * 3];

			int startY = sectorY - SECTOR_VIEW_START;
			int startX = sectorX - SECTOR_VIEW_START;
			for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
			{
				for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
				{
					if (startY + y < 0 || startY + y >= MAX_SECTOR_Y || startX + x < 0 || startX + x >= MAX_SECTOR_X)
						continue;

					sectors[sectorCount++] = &m_arrCSector[y + startY][x + startX];

					AcquireSRWLockShared(&m_arrCSector[y + startY][x + startX].m_srwLock);

				}
			}
			
			// 메시지 전송
			for (int msgI = 0; msgI < useSize; msgI++)
			{
				CSerializableBuffer<FALSE> *msg;
				if (!msgQ.Dequeue(&msg))
					__debugbreak();

				for (int i = 0; i < sectorCount; i++)
				{
					auto playerMap = sectors[i]->m_players;
					for (auto it = playerMap.begin(); it != playerMap.end(); ++it)
					{
						if (msg->GetSessionId() == it->first)
							continue;

						InterlockedIncrement(&g_monitor.m_chatMsgRes);
						EnqueuePacket(it->first, msg);
					}
				}

				if (msg->DecreaseRef() == 0)
					CSerializableBuffer<FALSE>::Free(msg);
			}

			// 락 해제
			for (int i = sectorCount - 1; i >= 0; i--)
			{
				ReleaseSRWLockShared(&sectors[i]->m_srwLock);
			}
		}
	}
}
