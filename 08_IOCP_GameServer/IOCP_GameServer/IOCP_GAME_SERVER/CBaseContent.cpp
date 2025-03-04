#include "pch.h"
#include "CNetServer.h"
#include "CBaseContent.h"

void CBaseContent::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
{
	NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	// ReleaseFlag가 이미 켜진 상황
	if ((pSession->m_iIOCountAndRelease & NETWORK_SERVER::CNetSession::RELEASE_FLAG) == NETWORK_SERVER::CNetSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NETWORK_SERVER::g_NetServer->ReleaseSession(pSession);
		}
		return;
	}

	if (sessionID != pSession->m_uiSessionID)
	{
		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NETWORK_SERVER::g_NetServer->ReleaseSession(pSession);
		}
		return;
	}

	// MOVE_JOB Enqueue 가능

	MOVE_JOB *pMoveJob = MOVE_JOB::Alloc();
	pMoveJob->sessionID = sessionID;
	pMoveJob->objectPtr = pObject;

	m_MoveJobQ.Enqueue(pMoveJob);
}

void CBaseContent::ConsumeMoveJob() noexcept
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
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(moveJob->sessionID)];

		// 여기까진 유효한 세션일 것
		// objectPtr == nullptr이면 OnEnter 받은 쪽에서 생성
		// 자기 자신 map 에도 넣어야 함
		// 세션에 상태 설정
		// pSession->flag = m_pBaseContent::State
		OnEnter(moveJob->sessionID, moveJob->objectPtr);

		MOVE_JOB::s_MoveJobPool.Free(moveJob);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			NETWORK_SERVER::g_NetServer->ReleaseSession(pSession);
		}
	}
}

void CBaseContent::ConsumeLeaveJob() noexcept
{
	int leaveJobQSize = m_LeaveJobQ.GetUseSize();
	for (int i = 0; i < leaveJobQSize; i++)
	{
		UINT64 sessionID;
		m_LeaveJobQ.Dequeue(&sessionID);
		OnLeave(sessionID);
	}
}

void CBaseContent::ConsumeRecvMsg() noexcept
{
	// 세션 메시지 처리
	for (auto &[key, value] : m_umapSessions)
	{
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(key)];
		LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
		for (int i = 0; i < recvMsgCount; i++)
		{
			CSerializableBufferView<FALSE> *pMsg;
			pSession->m_RecvMsgQueue.Dequeue(&pMsg);

			RECV_RET ret = OnRecv(key, pMsg);
			// 이동 메시지 처리
			if (ret == RECV_RET::RECV_MOVE)
			{
				if (pMsg->DecreaseRef() == 0)
					CSerializableBufferView<FALSE>::Free(pMsg);
				break;
			}

			// 잘못된 메시지 수신
			if (ret == RECV_RET::RECV_FALSE)
			{
				NETWORK_SERVER::g_NetServer->Disconnect(key);

				if (pMsg->DecreaseRef() == 0)
					CSerializableBufferView<FALSE>::Free(pMsg);
				break;
			}

			// TRUE

			if (pMsg->DecreaseRef() == 0)
				CSerializableBufferView<FALSE>::Free(pMsg);
		}
	}
}
