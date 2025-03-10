#include "pch.h"
#include "MyInclude.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "CBaseContent.h"
#include "SystemEvent.h"
#include "CGameContent.h"
#include "ContentEvent.h"
#include "CContentThread.h"

CGameServer::CGameServer()
{

}

bool CGameServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

void CGameServer::OnAccept(const UINT64 sessionID) noexcept
{
	// 해당 유저 Auth Content로 보냄
	m_pAuthContent->MoveJobEnqueue(sessionID, nullptr);
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

void CGameServer::RegisterContentTimerEvent() noexcept
{
	m_pAuthContent = new CAuthContent;
	ContentFrameEvent *pAuthEvent = new ContentFrameEvent;
	pAuthEvent->SetEvent(m_pAuthContent, 25);

	CContentThread::s_arrContentThreads[0]->EnqueueEventMy(pAuthEvent);
	// CContentThread::EnqueueEvent(pAuthEvent);

	m_pEchoContent = new CEchoContent;
	ContentFrameEvent *pEchoEvent = new ContentFrameEvent;
	pEchoEvent->SetEvent(m_pEchoContent, 25);
	CContentThread::s_arrContentThreads[1]->EnqueueEventMy(pEchoEvent);
	// CContentThread::EnqueueEvent(pEchoEvent);
}
