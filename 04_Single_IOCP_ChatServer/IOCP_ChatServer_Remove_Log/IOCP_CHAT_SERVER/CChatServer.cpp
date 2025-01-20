#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CPlayer.h"
#include "CSector.h"
#include "ChatSetting.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"

CChatServer::CChatServer() noexcept
{
	m_pProcessPacket = new CChatProcessPacket;
	m_pProcessPacket->SetChatServer(this);
}

void CChatServer::Update() noexcept
{
	// �̶� Ȯ���� JobCount ��ŭ ������ ����
	LONG jobCount = m_RecvJobQ.GetUseSize();
	for (int i = 0; i < jobCount; i++)
	{
		CSerializableBufferView<FALSE> *recvJob;
		if (!m_RecvJobQ.Dequeue(&recvJob))
			__debugbreak();

		g_monitor.m_lUpdateTPS++;

		WORD type;
		*recvJob >> type;
		if (!m_pProcessPacket->ConsumPacket((en_PACKET_TYPE)type, recvJob->GetSessionID(), CSmartPtr<CSerializableBufferView<FALSE>>(recvJob)))
		{
			Disconnect(recvJob->GetSessionID());
		}

		if (recvJob->DecreaseRef() == 0)
			CSerializableBufferView<FALSE>::Free(recvJob);
	}

	// ���⿡�� Sector�� 1�� ��ȸ�ϸ� �����۾� ����
	DelaySendSector();
}

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
	int startY = sectorY - SECTOR_VIEW_START;
	int startX = sectorX - SECTOR_VIEW_START;
	for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
	{
		for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
		{
			if (startY + y < 0 || startY + y >= MAX_SECTOR_Y || startX + x < 0 || startX + x >= MAX_SECTOR_X)
				continue;

			CSector &sector = m_arrCSector[y + startY][x + startX];
			for (auto &it : sector.m_players)
			{
				if (it.first == sessionId)
					continue;

				SendPacket(it.first, message);
			}
		}
	}
}

bool CChatServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

// Apc ���� �����
void CChatServer::OnAccept(const UINT64 sessionID) noexcept
{
	g_monitor.m_lUpdateTPS++;
	CNonLoginPlayer nonLogin = { timeGetTime() };
	m_umapNonLoginPlayer.insert(std::make_pair(sessionID, nonLogin));
}

// Apc ���� �����
void CChatServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	g_monitor.m_lUpdateTPS++;

	auto it1 = m_umapNonLoginPlayer.find(sessionID);
	if (it1 != m_umapNonLoginPlayer.end())
	{
		m_umapNonLoginPlayer.erase(sessionID);
		return;
	}

	auto it2 = m_umapLoginPlayer.find(sessionID);
	if (it2 != m_umapLoginPlayer.end())
	{
		CPlayer *player = it2->second;

		// ���Ϳ��� ����
		if (player->m_usSectorY != 0xFF && player->m_usSectorX != 0xFF)
			m_arrCSector[player->m_usSectorY][player->m_usSectorX].m_players.erase(sessionID);

		m_umapLoginPlayer.erase(sessionID);
		CPlayer::Free(player);
		return;
	}
}

void CChatServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	// JobQ Enqueue
	message->IncreaseRef();
	m_RecvJobQ.Enqueue(message);
}

// �� ���� Lock Free Queue ������ ����
void CChatServer::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
}

void CChatServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

DWORD CChatServer::OnUpdate() noexcept
{
	static DWORD prevTick = timeGetTime();

	int dTime = timeGetTime() - prevTick;
	// ���� ����� �ϴ� �ð�
	if (dTime < FRAME_PER_TICK)
		return FRAME_PER_TICK - dTime;

	// ��¥ Update
	Update();
	InterlockedIncrement(&g_monitor.m_lServerFrame);

	prevTick += FRAME_PER_TICK;
	return 0;
}

void CChatServer::OnHeartBeat() noexcept
{
	// non-login Player ��ü�� HeartBeat �˻�
	static DWORD nonLoginPrevTick = timeGetTime();
	int dTime = timeGetTime() - nonLoginPrevTick;
	if (dTime - nonLoginPrevTick > NON_LOGIN_TIME_OUT_CHECK)
	{
		NonLoginHeartBeat();
		nonLoginPrevTick += NON_LOGIN_TIME_OUT_CHECK;
	}

	// login Player ��ü�� HeartBeat �˻�
	static DWORD loginPrevTick = timeGetTime();
	dTime = timeGetTime() - loginPrevTick;
	if (dTime - loginPrevTick > LOGIN_TIME_OUT_CHECK)
	{
		LoginHeartBeat();
		loginPrevTick += LOGIN_TIME_OUT_CHECK;
	}

}

void CChatServer::DelaySendSector() noexcept
{
	for (int sectorY = 0; sectorY < MAX_SECTOR_Y; sectorY++)
	{
		for (int sectorX = 0; sectorX < MAX_SECTOR_X; sectorX++)
		{
			CDeque<CSerializableBuffer<FALSE> *> &msgQ = m_arrCSector[sectorY][sectorX].m_sendMsgQ;
			if (msgQ.empty())
				continue;

			// ���� ��ȸ
			int startY = sectorY - SECTOR_VIEW_START;
			int startX = sectorX - SECTOR_VIEW_START;
			for (int y = 0; y < SECTOR_VIEW_COUNT; y++)
			{
				for (int x = 0; x < SECTOR_VIEW_COUNT; x++)
				{
					if (startY + y < 0 || startY + y >= MAX_SECTOR_Y || startX + x < 0 || startX + x >= MAX_SECTOR_X)
						continue;

					CSector &sector = m_arrCSector[y + startY][x + startX];
					for (auto msgIt = msgQ.begin(); msgIt != msgQ.end();)
					{
						UINT64 sessionId = (*msgIt)->GetSessionId();
						SendPacket(sessionId, *msgIt);


						for (auto &it : sector.m_players)
						{
							if (it.first == sessionId)
								continue;

							SendPacket(it.first, *msgIt);
						}

						if ((*msgIt)->DecreaseRef() == 0)
							CSerializableBuffer<FALSE>::Free(*msgIt);

						msgIt = msgQ.erase(msgIt);
					}
				}
			}
		}
	}
}
