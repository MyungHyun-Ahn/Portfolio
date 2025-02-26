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
		// �ø� ���·� ���� ��
		// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		m_pBaseContent->m_MoveJobQ.Dequeue(&moveJob);

		// ���� ã��
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(moveJob->sessionID)];

		// ������� ��ȿ�� ������ ��
		// objectPtr == nullptr�̸� OnEnter ���� �ʿ��� ����
		// �ڱ� �ڽ� map ���� �־�� ��
		// ���ǿ� ���� ����
		// pSession->flag = m_pBaseContent::State
		m_pBaseContent->OnEnter(moveJob->sessionID, moveJob->objectPtr);

		MOVE_JOB::s_MoveJobPool.Free(moveJob);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NETWORK_SERVER::g_NetServer->ReleaseSession(pSession);
		}
	}


}
