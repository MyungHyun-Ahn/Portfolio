#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "CBaseContents.h"
#include "SystemEvent.h"
#include "CGameContents.h"
#include "ContentEvent.h"
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
	// 해당 유저 Auth Content로 보냄
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

void CGameServer::RegisterContentTimerEvent() noexcept
{
	m_pAuthContents = new CAuthContents;
	ContentsFrameEvent *pAuthEvent = new ContentsFrameEvent;
	pAuthEvent->SetEvent(m_pAuthContents, GAME_SETTING::AUTH_FPS);
	CContentsThread::EnqueueEvent(pAuthEvent);

	m_pEchoContents = new CEchoContents;
	ContentsFrameEvent *pEchoEvent = new ContentsFrameEvent;
	pEchoEvent->SetEvent(m_pEchoContents, GAME_SETTING::ECHO_FPS);
	CContentsThread::EnqueueEvent(pEchoEvent);
}
