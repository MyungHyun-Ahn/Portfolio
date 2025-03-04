#include "pch.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "SystemEvent.h"
#include "CBaseContent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void MonitorTimerEvent::Execute() noexcept
{
	// g_monitor.Update(g_NetServer->GetSessionCount(), ((CChatServer *)g_NetServer)->GetPlayerCount());
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void KeyBoardTimerEvent::Execute() noexcept
{
	// ���� ����
	if (GetAsyncKeyState(VK_F1))
		NETWORK_SERVER::g_NetServer->Stop();

	// �������Ϸ� ����
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
	// TimerEvent�� ���Ḧ �����ϱ� ����
	m_pBaseContent->m_pTimerEvent = this;
	timeMs = 1000 / frame;
	execute = std::bind(&ContentFrameEvent::Execute, this, std::placeholders::_1);
	nextExecuteTime = timeGetTime();
}

// delayFrame �Ⱦ�
void ContentFrameEvent::Execute(int delayFrame) noexcept
{
	m_pBaseContent->ConsumeMoveJob();
	m_pBaseContent->ConsumeLeaveJob();
	m_pBaseContent->ConsumeRecvMsg();
}
