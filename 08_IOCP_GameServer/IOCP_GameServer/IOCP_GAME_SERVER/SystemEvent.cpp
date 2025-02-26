#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "SystemEvent.h"
#include "CBaseContent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorTimerEvent::Execute() noexcept
{
	// g_monitor.Update(g_NetServer->GetSessionCount(), ((CChatServer *)g_NetServer)->GetPlayerCount());
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void KeyBoardTimerEvent::Execute() noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		NETWORK_SERVER::g_NetServer->Stop();

	// 프로파일러 저장
	if (GetAsyncKeyState(VK_F2))
		g_ProfileMgr->DataOutToFile();
}

ContentFrameEvent::~ContentFrameEvent()
{
	delete m_pBaseContent;
}

void ContentFrameEvent::SetEvent(CBaseContent *pBaseContent, int frame) noexcept
{
	m_pBaseContent = pBaseContent;
	// TimerEvent의 종료를 제어하기 위함
	m_pBaseContent->m_pTimerEvent = this;
	timeMs = 1000 / frame;
	execute = std::bind(&ContentFrameEvent::Execute, this, std::placeholders::_1);
	nextExecuteTime = timeGetTime();
}

// delayFrame 안씀
void ContentFrameEvent::Execute(int delayFrame) noexcept
{
	int leaveJobQSize = m_pBaseContent->m_LeaveJobQ.GetUseSize();
	for (int i = 0; i < leaveJobQSize; i++)
	{
		UINT64 sessionID;
		m_pBaseContent->m_LeaveJobQ.Dequeue(&sessionID);
		m_pBaseContent->OnLeave(sessionID);
	}

	int moveJobQSize = m_pBaseContent->m_MoveJobQ.GetUseSize();
	for (int i = 0; i < moveJobQSize; i++)
	{
		MOVE_JOB *moveJob;
		// 올린 상태로 들어올 것
		// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		m_pBaseContent->m_MoveJobQ.Dequeue(&moveJob);

		// 세션 찾기
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(moveJob->sessionID)];

		// 여기까진 유효한 세션일 것
		// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
		// 자기 자신 map 에도 넣어야 함
		// 세션에 상태 설정
		// pSession->flag = m_pBaseContent::State
		m_pBaseContent->OnEnter(moveJob->sessionID, moveJob->objectPtr);

		MOVE_JOB::s_MoveJobPool.Free(moveJob);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NETWORK_SERVER::g_NetServer->ReleaseSession(pSession);
		}
	}


}
