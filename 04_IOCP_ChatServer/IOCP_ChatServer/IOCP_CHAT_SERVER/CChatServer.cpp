#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CChatServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"

unsigned int FPS = 25;
#define FRAME_PER_TICK 1000 / FPS

unsigned int NON_LOGIN_TIME_OUT = 2 * 1000;
unsigned int LOGIN_TIME_OUT = 40 * 1000;

unsigned int NON_LOGIN_TIME_OUT_CHECK = 2 * 1000;
unsigned int LOGIN_TIME_OUT_CHECK = 40 * 1000;

#define SECTOR_VIEW_START 1
#define SECTOR_VIEW_COUNT 3

CChatServer::CChatServer() noexcept
{
	m_pProcessPacket = new CChatProcessPacket;
}

void CChatServer::Update() noexcept
{
	// 이때 확인한 JobCount 만큼 진행할 것임
	LONG jobCount = m_RecvJobQ.GetUseSize();
	for (int i = 0; i < jobCount; i++)
	{
		CSerializableBufferView<FALSE> *recvJob;
		if (m_RecvJobQ.Dequeue(&recvJob))
			__debugbreak();

		WORD type;
		*recvJob >> type;
		m_pProcessPacket->ConsumPacket((en_PACKET_TYPE)type, recvJob->GetSessionID(), CSmartPtr<CSerializableBufferView<FALSE>>(recvJob));

		if (recvJob->DecreaseRef() == 0)
			CSerializableBufferView<FALSE>::Free(recvJob);
	}

	// 여기에서 Sector를 1번 순회하며 전송작업 진행
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

			CSector &sector = m_arrCSector[y][x];
			for (auto &it : sector.m_players)
			{
				if (it.first == sessionId)
					continue;

				SendPacket(sessionId, message);
			}
		}
	}
}

bool CChatServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return false;
}

// Apc 에서 수행됨
void CChatServer::OnAccept(const UINT64 sessionID) noexcept
{
	CNonLoginPlayer nonLogin = { timeGetTime() };
	m_umapNonLoginPlayer.insert(std::make_pair(sessionID, nonLogin));
}

// Apc 에서 수행됨
void CChatServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	auto it1 = m_umapNonLoginPlayer.find(sessionID);
	if (it1 != m_umapNonLoginPlayer.end())
	{
		m_umapNonLoginPlayer.erase(sessionID);
		return;
	}

	auto it2 = m_umapLoginPlayer.find(sessionID);
	CPlayer *player = it2->second;
	if (it2 != m_umapLoginPlayer.end())
	{
		m_umapNonLoginPlayer.erase(sessionID);
		delete player;
		return;
	}
}

void CChatServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	// JobQ Enqueue
	message->IncreaseRef();
	m_RecvJobQ.Enqueue(message);
}

// 이 버전 Lock Free Queue 때문에 못씀
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
	// 다음 깨어나야 하는 시간
	if (dTime - prevTick < FRAME_PER_TICK)
		return FRAME_PER_TICK - dTime;

	// 진짜 Update
	Update();

	prevTick += FRAME_PER_TICK;
	return FRAME_PER_TICK - (timeGetTime() - prevTick);
}

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
