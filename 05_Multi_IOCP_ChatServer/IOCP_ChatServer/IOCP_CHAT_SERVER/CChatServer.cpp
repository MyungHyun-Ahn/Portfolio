#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "ChatSetting.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"

CChatServer::CChatServer() noexcept
{
	m_pProcessPacket = new CChatProcessPacket;
	m_pProcessPacket->SetChatServer(this);
	InitializeSRWLock(&m_playerMapLock);

	// 싱글에서 수행됨
	m_umapNonLoginPlayer.reserve(20000);
	m_umapLoginPlayer.reserve(20000);
}


// 하트비트 일단 보류
void CChatServer::NonLoginHeartBeat() noexcept
{
	DWORD nowTime = timeGetTime();
	for (auto it = m_umapNonLoginPlayer.begin(); it != m_umapNonLoginPlayer.end(); ++it)
	{
		DWORD dTime = nowTime - it->second.m_dwPrevRecvTime;
		if (dTime > NON_LOGIN_TIME_OUT)
			Disconnect(it->first);
	}
}

void CChatServer::LoginHeartBeat() noexcept
{
	DWORD nowTime = timeGetTime();
	for (auto it = m_umapLoginPlayer.begin(); it != m_umapLoginPlayer.end(); ++it)
	{
		CPlayer *player = it->second;
		DWORD dTime = nowTime - player->m_dwPrevRecvTime;
		if (dTime > LOGIN_TIME_OUT)
			Disconnect(it->first);
	}
}

void CChatServer::SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX, CSerializableBuffer<FALSE> *message) noexcept
{
	// AcquireSRWLockExclusive(&m_playerMapLock);

	// 만약 2 * 2면 4
	int sectorCount = 0;
	// 최대 3 x 3
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

			SendPacketPQCS(it->first, message);
		}
	}

	for (int i = sectorCount - 1; i >= 0; i--)
	{
		ReleaseSRWLockShared(&sectors[i]->m_srwLock);
	}

	// ReleaseSRWLockExclusive(&m_playerMapLock);
}

bool CChatServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

// Apc 에서 수행됨
void CChatServer::OnAccept(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);
	CNonLoginPlayer nonLogin = { timeGetTime() };
	AcquireSRWLockExclusive(&m_playerMapLock);
	m_umapNonLoginPlayer.insert(std::make_pair(sessionID, nonLogin));
	ReleaseSRWLockExclusive(&m_playerMapLock);
}

// Apc 에서 수행됨
void CChatServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);

	AcquireSRWLockExclusive(&m_playerMapLock);
	auto it1 = m_umapNonLoginPlayer.find(sessionID);
	if (it1 != m_umapNonLoginPlayer.end())
	{
		m_umapNonLoginPlayer.erase(sessionID);
		ReleaseSRWLockExclusive(&m_playerMapLock);
		return;
	}

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

	// 사실 위 둘에서 안걸리고 여기서 해제하면 버그임
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

// 일단 보류
void CChatServer::OnHeartBeat() noexcept
{
	// non-login Player 객체의 HeartBeat 검사
	static DWORD nonLoginPrevTick = timeGetTime();
	int dTime = timeGetTime() - nonLoginPrevTick;
	if (dTime - nonLoginPrevTick > NON_LOGIN_TIME_OUT_CHECK)
	{
		NonLoginHeartBeat();
		nonLoginPrevTick += NON_LOGIN_TIME_OUT_CHECK;
	}

	// login Player 객체의 HeartBeat 검사
	static DWORD loginPrevTick = timeGetTime();
	dTime = timeGetTime() - loginPrevTick;
	if (dTime - loginPrevTick > LOGIN_TIME_OUT_CHECK)
	{
		LoginHeartBeat();
		loginPrevTick += LOGIN_TIME_OUT_CHECK;
	}

}

void CChatServer::OnSectorBroadcast() noexcept
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

			// 만약 2 * 2면 4
			int sectorCount = 0;
			// 최대 3 x 3
			CSector *sectors[3 * 3];

			int userCount = 0;

			// 섹터 순회
			int startY = sectorY - SECTOR_VIEW_START;
			int startX = sectorX - SECTOR_VIEW_START;
			for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
			{
				for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
				{
					// 0 1 2
					// 3 4 5
					// 6 7 8
					if (startY + y < 0 || startY + y >= MAX_SECTOR_Y || startX + x < 0 || startX + x >= MAX_SECTOR_X)
						continue;

					// 락을 먼저 싹 잡고
					sectors[sectorCount++] = &m_arrCSector[y + startY][x + startX];

					AcquireSRWLockShared(&m_arrCSector[y + startY][x + startX].m_srwLock);
					userCount += m_arrCSector[y + startY][x + startX].m_players.size();

				}
			}

			// 메시지 전송
			for (int msgI = 0; msgI < useSize; msgI++)
			{
				CSerializableBuffer<FALSE> *msg;
				if (!msgQ.Dequeue(&msg))
					__debugbreak();


				// SendPacketPQCS(msg->GetSessionId(), msg);

				int checkUserCount = 0;

				for (int i = 0; i < sectorCount; i++)
				{
					auto playerMap = sectors[i]->m_players;
					checkUserCount += playerMap.size();
					for (auto it = playerMap.begin(); it != playerMap.end(); ++it)
					{
						if (msg->GetSessionId() == it->first)
							continue;

						SendPacketPQCS(it->first, msg);
					}
				}

				if (userCount != checkUserCount)
					__debugbreak();

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
