#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "ContentEvent.h"
#include "CGameServer.h"
#include "CBaseContents.h"
#include "SystemEvent.h"
#include "CGameContents.h"
#include "CContentsThread.h"
#include "GameSetting.h"

CGameServer::CGameServer()
{

}

bool CGameServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

void CGameServer::OnAccept(const UINT64 sessionID) noexcept
{
	m_pAuthContents->MoveJobEnqueue(sessionID, nullptr);
}

// 딱히 할일이 없음
void CGameServer::OnClientLeave(const UINT64 sessionID) noexcept
{

}

// 미사용
void CGameServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
}

void CGameServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

// lpParam으로 넘겨준 TimerEvent를 수행
unsigned int ContentsThreadFunc(LPVOID lpParam)
{
	TimerEvent *pTimerEvent = (TimerEvent *)lpParam;
	while (true)
	{
		LONG dTime = pTimerEvent->nextExecuteTime - timeGetTime();
		if (dTime > 0)
		{
			Sleep(dTime);
			continue;
		}

		INT delayFrame;
		if (pTimerEvent->timeMs == 0)
			delayFrame = 0;
		else
			delayFrame = ((dTime * -1) / pTimerEvent->timeMs) + 1;

		pTimerEvent->execute(delayFrame);
		pTimerEvent->nextExecuteTime += pTimerEvent->timeMs * delayFrame;
	}

	return 0;
}

void CGameServer::RegisterContentTimerEvent() noexcept
{
	// m_pAuthContents = new CAuthContents;
	// ContentsFrameEvent *pAuthEvent = new ContentsFrameEvent;
	// pAuthEvent->SetEvent(m_pAuthContents, GAME_SETTING::AUTH_FPS);
	// CContentsThread::EnqueueEvent(pAuthEvent);
	// 
	// m_pEchoContents = new CEchoContents;
	// ContentsFrameEvent *pEchoEvent = new ContentsFrameEvent;
	// pEchoEvent->SetEvent(m_pEchoContents, GAME_SETTING::ECHO_FPS);
	// CContentsThread::EnqueueEvent(pEchoEvent);

	for (int i = 0; i < GAME_SETTING::WORK_COUNT; i++)
	{
		ContentsThreadTest *pContentsEvent = new ContentsThreadTest;
		pContentsEvent->SetEvent(25);

		m_ArrTestEvent[m_iEventCount++] = pContentsEvent;

		if (GAME_SETTING::USE_CONTENTS_THREAD)
			CContentsThread::EnqueueEvent(pContentsEvent);
		else
			_beginthreadex(nullptr, 0, ContentsThreadFunc, (LPVOID)pContentsEvent, 0, nullptr);
	}
}