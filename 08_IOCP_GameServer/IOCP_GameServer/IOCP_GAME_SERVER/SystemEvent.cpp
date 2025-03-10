#include "pch.h"
#include "MyInclude.h"
#include "ServerSetting.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "SystemEvent.h"
#include "CBaseContent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&MonitorTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 1000; // 1��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void MonitorTimerEvent::Execute() noexcept
{
	g_monitor.Update();
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
	isTimerEvent = true;
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
	InterlockedIncrement(&m_pBaseContent->m_FPS);

	m_pBaseContent->ConsumeMoveJob();
	m_pBaseContent->ConsumeLeaveJob();
	m_pBaseContent->ConsumeRecvMsg();
}

void SendAllTimerEvent::SetEvent() noexcept
{
	execute = std::bind(&SendAllTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 50; // 1�ʿ� 10��
	nextExecuteTime = timeGetTime(); // ���� �ð�
}

void SendAllTimerEvent::Execute() noexcept
{
	NETWORK_SERVER::g_NetServer->SendAll();
}

void EnqueuePacketEvent::SetEvent(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer) noexcept
{
	execute = std::bind(&EnqueuePacketEvent::Execute, this, sessionId, sBuffer, std::placeholders::_1);
}

void EnqueuePacketEvent::Execute(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer, int delayFrame)
{
	NETWORK_SERVER::g_NetServer->EnqueuePacket(sessionId, sBuffer);
	if (sBuffer->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(sBuffer);
}
