#include "ServerNetworkLibPch.h"

namespace ServerNetworkLib
{
void SMonitorTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&MonitorTimerEvent::Execute, this);
	m_isRunning = true;
	m_isTimerEvent = true;
	m_timeMilliseconds = 1000; // 1초
	m_nextExecuteTime = timeGetTime(); // 현재 시각
}

void SMonitorTimerEvent::Execute(int delayFrame) noexcept
{
	g_monitor.Update(g_pNetServer->GetSessionCount());
}

void SKeyboardTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	m_isRunning = true;
	m_isTimerEvent = true;
	m_timeMilliseconds = 1000; // 1초
	m_nextExecuteTime = timeGetTime(); // 현재 시각
}

void SKeyboardTimerEvent::Execute(int delayFrame) noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		ServerNetworkLib::g_pNetServer->Stop();

	// 프로파일러 저장
	if (GetAsyncKeyState(VK_F2))
		g_pProfileManager->DataOutToFile();
}

SContentsFrameEvent::~SContentsFrameEvent()
{
	delete m_pBaseContent;
}

void SContentsFrameEvent::SetEvent(CBaseContents *pBaseContent, int frame) noexcept
{
	m_pBaseContent = pBaseContent;
	// TimerEvent의 종료를 제어하기 위함
	m_pBaseContent->m_pTimerEvent = this;
	if (frame == 0)
		m_timeMilliseconds = 0;
	else
		m_timeMilliseconds = 1000 / frame;
	m_nextExecuteTime = timeGetTime();
}

// delayFrame 안씀

void SContentsFrameEvent::Execute(int delayFrame) noexcept
{
	InterlockedIncrement(&m_pBaseContent->m_fps);

	m_pBaseContent->ProcessMoveJob();
	m_pBaseContent->ProcessLeaveJob();
	m_pBaseContent->ProcessRecvMsg(delayFrame);
}

void SSendAllTimerEvent::SetEvent() noexcept
{
	m_timeMilliseconds = 50; // 1초에 20번
	m_nextExecuteTime = timeGetTime(); // 현재 시각
}

void SSendAllTimerEvent::Execute(int delayFrame) noexcept
{
	ServerNetworkLib::g_pNetServer->SendAll();
}

void SExampleTimerEvent::SetEvent(int param1, double param2, int frame) noexcept
{
	m_param1 = param1;
	m_dParam2 = param2;
	if (frame == 0)
		m_timeMilliseconds = 0;
	else
		m_timeMilliseconds = 1000 / frame;
	m_nextExecuteTime = timeGetTime();
}

void SExampleTimerEvent::Execute(int delayFrame) noexcept
{
	m_param1 += (1 + (delayFrame * 1));
	// ...
}
}
