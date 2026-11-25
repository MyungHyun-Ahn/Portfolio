#include "ServerNetworkLibPch.h"

namespace ServerNetworkLib
{
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
