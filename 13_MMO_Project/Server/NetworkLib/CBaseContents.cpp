#include "ServerNetworkLibPch.h"

namespace ServerNetworkLib
{
void CBaseContents::MoveJobEnqueue(UINT64 sessionId, void *pObject) noexcept
{
	CSession *pSession = g_pServer->m_pSessions[CServer::getIndex(sessionId)];

	InterlockedIncrement(&pSession->m_ioCountAndRelease);
	if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
	{
		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ServerNetworkLib::g_pServer->ReleaseSession(pSession);
		}
		return;
	}

	if (sessionId != pSession->m_sessionId)
	{
		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ServerNetworkLib::g_pServer->ReleaseSession(pSession);
		}
		return;
	}

	SMoveJob *pMoveJob = SMoveJob::Alloc();
	pMoveJob->m_sessionId = sessionId;
	pMoveJob->m_pObject = pObject;
	m_moveJobQ.Enqueue(pMoveJob);
}

void CBaseContents::LeaveJobEnqueue(UINT64 sessionId)
{
	m_leaveJobQ.Enqueue(sessionId);
}

void CBaseContents::ProcessMoveJob() noexcept
{
	// 이동 처리
	int moveJobQSize = m_moveJobQ.GetUseSize();
	for (int i = 0; i < moveJobQSize; i++)
	{
		SMoveJob *pMoveJob;
		// 올린 상태로 들어올 것
		// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if (!m_moveJobQ.Dequeue(&pMoveJob))
			break;

		// 세션 찾기
		ServerNetworkLib::CSession *pSession = ServerNetworkLib::g_pServer->m_pSessions[ServerNetworkLib::CServer::getIndex(pMoveJob->m_sessionId)];

		// 여기까진 유효한 세션일 것
		// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
		// 자기 자신 map 에도 넣어야 함
		// 세션에 상태 설정
		// pSession->flag = m_pBaseContent::State
		OnEnter(pMoveJob->m_sessionId, pMoveJob->m_pObject);
		pSession->m_pCurrentContent = this;

		SMoveJob::sm_moveJobPool.Free(pMoveJob);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ServerNetworkLib::g_pServer->ReleaseSession(pSession);
		}
	}
}

void CBaseContents::ProcessLeaveJob() noexcept
{
	int leaveJobQSize = m_leaveJobQ.GetUseSize();
	for (int i = 0; i < leaveJobQSize; i++)
	{
		UINT64 sessionId;
		if (!m_leaveJobQ.Dequeue(&sessionId))
			break;
		OnLeave(sessionId);
	}
}

void CBaseContents::ProcessRecvMsg(int delayFrame) noexcept
{
	for (auto &it : m_umapSessions)
	{
		UINT64 sessionId = it.first;
		int sessionIdx = ServerNetworkLib::CServer::getIndex(sessionId);
		ServerNetworkLib::CSession *pSession = ServerNetworkLib::g_pServer->m_pSessions[sessionIdx];
		
		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ServerNetworkLib::g_pServer->ReleaseSession(pSession);
			}
			continue;
		}
		
		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ServerNetworkLib::g_pServer->ReleaseSession(pSession);
			}
			continue;
		}

		LONG recvMsgCount = pSession->m_recvMsgQueue.GetUseSize();
		for (int i = 0; i < recvMsgCount; i++)
		{
			CSerializableBufferView *pMsg;
			if (!pSession->m_recvMsgQueue.Dequeue(&pMsg))
				break;

			ERecvResult ret = OnRecv(sessionId, pMsg, delayFrame);
			CSerializableBufferView::Free(pMsg);
			if (ret == ERecvResult::Moved)
				break;
			if (ret == ERecvResult::Failed)
			{
				ServerNetworkLib::g_pServer->Disconnect(sessionId);
				break;
			}
		}

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ServerNetworkLib::g_pServer->ReleaseSession(pSession);
		}
	}
}
}
