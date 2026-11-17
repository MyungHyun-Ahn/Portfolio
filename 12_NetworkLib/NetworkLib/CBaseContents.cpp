#include "pch.h"
#include "CNetServer.h"
#include "CBaseContents.h"

void CBaseContents::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
{
	NET_SERVER::CNetSession *pSession 
		= NET_SERVER::g_NetServer->m_arrPSessions[NET_SERVER::CNetServer::GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	if ((pSession->m_iIOCountAndRelease & NET_SERVER::CNetSession::RELEASE_FLAG) 
		== NET_SERVER::CNetSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NET_SERVER::g_NetServer->ReleaseSession(pSession);
		}
		return;
	}

	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NET_SERVER::g_NetServer->ReleaseSession(pSession);
		}
		return;
	}

	MOVE_JOB *pMoveJob = MOVE_JOB::Alloc();
	pMoveJob->sessionID = sessionID;
	pMoveJob->objectPtr = pObject;
	m_MoveJobQ.Enqueue(pMoveJob);
}

void CBaseContents::LeaveJobEnqueue(UINT64 sessionID)
{
	m_LeaveJobQ.Enqueue(sessionID);
}

void CBaseContents::ProcessMoveJob() noexcept
{
	// 이동 처리
	int moveJobQSize = m_MoveJobQ.GetUseSize();
	for (int i = 0; i < moveJobQSize; i++)
	{
		MOVE_JOB *moveJob;
		// 올린 상태로 들어올 것
		// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		m_MoveJobQ.Dequeue(&moveJob);

		// 세션 찾기
		NET_SERVER::CNetSession *pSession = NET_SERVER::g_NetServer->m_arrPSessions[NET_SERVER::CNetServer::GetIndex(moveJob->sessionID)];

		// 여기까진 유효한 세션일 것
		// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
		// 자기 자신 map 에도 넣어야 함
		// 세션에 상태 설정
		// pSession->flag = m_pBaseContent::State
		OnEnter(moveJob->sessionID, moveJob->objectPtr);
		pSession->m_pCurrentContent = this;

		MOVE_JOB::s_MoveJobPool.Free(moveJob);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NET_SERVER::g_NetServer->ReleaseSession(pSession);
		}
	}
}

void CBaseContents::ProcessLeaveJob() noexcept
{
	int leaveJobQSize = m_LeaveJobQ.GetUseSize();
	for (int i = 0; i < leaveJobQSize; i++)
	{
		UINT64 sessionID;
		m_LeaveJobQ.Dequeue(&sessionID);
		OnLeave(sessionID);
	}
}

void CBaseContents::ProcessRecvMsg(int delayFrame) noexcept
{
	for (auto &it : m_umapSessions)
	{
		UINT64 sessionId = it.first;
		int sessionIdx = NET_SERVER::CNetServer::GetIndex(sessionId);
		NET_SERVER::CNetSession *pSession = NET_SERVER::g_NetServer->m_arrPSessions[sessionIdx];
		
		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & NET_SERVER::CNetSession::RELEASE_FLAG) 
			== NET_SERVER::CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NET_SERVER::g_NetServer->ReleaseSession(pSession);
			}
			continue;
		}
		
		if (sessionId != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				NET_SERVER::g_NetServer->ReleaseSession(pSession);
			}
			continue;
		}

		LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
		for (int i = 0; i < recvMsgCount; i++)
		{
			CSerializableBuffer<FALSE> *pMsg;
			pSession->m_RecvMsgQueue.Dequeue(&pMsg);

			RECV_RET ret = OnRecv(sessionId, pMsg, delayFrame);
			if (ret == RECV_RET::RECV_MOVE)
			{
				if (pMsg->DecreaseRef() == 0)
					CSerializableBuffer<FALSE>::Free(pMsg);
				break;
			}
			if (ret == RECV_RET::RECV_FALSE)
			{
				NET_SERVER::g_NetServer->Disconnect(sessionId);

				if (pMsg->DecreaseRef() == 0)
					CSerializableBuffer<FALSE>::Free(pMsg);
				break;
			}

			if (pMsg->DecreaseRef() == 0)
				CSerializableBuffer<FALSE>::Free(pMsg);
		}

		if (recvMsgCount != 0)
			NET_SERVER::g_NetServer->SendPQCS(pSession);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NET_SERVER::g_NetServer->ReleaseSession(pSession);
		}
	}
}
