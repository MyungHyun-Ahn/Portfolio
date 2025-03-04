#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CGameServer.h"
#include "CBaseContent.h"
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
}

void CGameServer::OnClientLeave(const UINT64 sessionID) noexcept
{
}

void CGameServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
}

void CGameServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

void CGameServer::RegisterContentTimerEvent() noexcept
{
	TestEvent *pTest1 = new TestEvent;
	pTest1->SetEvent(1000 / 25, 1);
	CContentThread::EnqueueEvent(pTest1);

	TestEvent *pTest2 = new TestEvent;
	pTest2->SetEvent(1000 / 25, 2);
	CContentThread::EnqueueEvent(pTest2);

	TestEvent *pTest3 = new TestEvent;
	pTest3->SetEvent(1000 / 25, 3);
	CContentThread::EnqueueEvent(pTest3);

	TestEvent *pTest4 = new TestEvent;
	pTest4->SetEvent(1000 / 25, 4);
	CContentThread::EnqueueEvent(pTest4);

	TestEvent *pTest5 = new TestEvent;
	pTest5->SetEvent(1000 / 25, 5);
	CContentThread::EnqueueEvent(pTest5);
	
	TestEvent *pTest6 = new TestEvent;
	pTest6->SetEvent(1000 / 25, 6);
	CContentThread::EnqueueEvent(pTest6);
	
	TestEvent *pTest7 = new TestEvent;
	pTest7->SetEvent(1000 / 25, 7);
	CContentThread::EnqueueEvent(pTest7);
	
	TestEvent *pTest8 = new TestEvent;
	pTest8->SetEvent(1000 / 25, 8);
	CContentThread::EnqueueEvent(pTest8);
	// 
	// TestEvent *pTest9 = new TestEvent;
	// pTest9->SetEvent(1000 / 5, 9);
	// CContentThread::EnqueueEvent(pTest9);
	// 
	// TestEvent *pTest10 = new TestEvent;
	// pTest10->SetEvent(1000 / 5, 10);
	// CContentThread::EnqueueEvent(pTest10);
}
