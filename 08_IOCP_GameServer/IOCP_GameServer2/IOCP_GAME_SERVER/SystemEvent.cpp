#include "pch.h"
#include "ServerSetting.h"
#include "BaseEvent.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "SystemEvent.h"
#include "CBaseContent.h"

void MonitorTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&MonitorTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void MonitorTimerEvent::execute(int delayFrame) noexcept
{
	g_monitor.Update();
}

void KeyBoardTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&KeyBoardTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 1000; // 1초
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void KeyBoardTimerEvent::execute(int delayFrame) noexcept
{
	// 서버 종료
	if (GetAsyncKeyState(VK_F1))
		NET_SERVER::g_NetServer->Stop();

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
	isTimerEvent = true;
	m_pBaseContent = pBaseContent;
	// TimerEvent의 종료를 제어하기 위함
	m_pBaseContent->m_pTimerEvent = this;
	if (frame == 0)
		timeMs = 0;
	else
		timeMs = 1000 / frame;
	// execute = std::bind(&ContentFrameEvent::Execute, this, std::placeholders::_1);
	nextExecuteTime = timeGetTime();
}

// delayFrame 안씀
void ContentFrameEvent::execute(int delayFrame) noexcept
{
	InterlockedIncrement(&m_pBaseContent->m_FPS);

	m_pBaseContent->ConsumeMoveJob();
	m_pBaseContent->ConsumeLeaveJob();
	m_pBaseContent->ConsumeRecvMsg();
}

void SendAllTimerEvent::SetEvent() noexcept
{
	// execute = std::bind(&SendAllTimerEvent::Execute, this);
	isRunning = true;
	isTimerEvent = true;
	timeMs = 50; // 1초에 20번
	nextExecuteTime = timeGetTime(); // 현재 시각
}

void SendAllTimerEvent::execute(int delayFrame) noexcept
{
	NET_SERVER::g_NetServer->SendAll();
}

void EnqueuePacketEvent::SetEvent(UINT64 sessionId, CSerializableBuffer<FALSE> *sBuffer) noexcept
{
	SessionId = sessionId;
	SBuffer = sBuffer;
	// execute = std::bind(&EnqueuePacketEvent::Execute, this, sessionId, sBuffer, std::placeholders::_1);
}

void EnqueuePacketEvent::execute(int delayFrame) noexcept
{
	NET_SERVER::g_NetServer->EnqueuePacket(SessionId, SBuffer);
	if (SBuffer->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(SBuffer);
}
