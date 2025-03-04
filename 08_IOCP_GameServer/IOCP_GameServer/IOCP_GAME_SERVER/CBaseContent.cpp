#include "pch.h"
#include "CNetServer.h"
#include "CBaseContent.h"

void CBaseContent::MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept
{
	NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(sessionID)];

	InterlockedIncrement(&pSession->m_iIOCountAndRelease);
	// ReleaseFlag�� �̹� ���� ��Ȳ
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

	// MOVE_JOB Enqueue ����

	MOVE_JOB *pMoveJob = MOVE_JOB::Alloc();
	pMoveJob->sessionID = sessionID;
	pMoveJob->objectPtr = pObject;

	m_MoveJobQ.Enqueue(pMoveJob);
}

void CBaseContent::ConsumeMoveJob() noexcept
{
	// �̵� ó��
	int moveJobQSize = m_MoveJobQ.GetUseSize();
	for (int i = 0; i < moveJobQSize; i++)
	{
		MOVE_JOB *moveJob;
		// �ø� ���·� ���� ��
		// InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		m_MoveJobQ.Dequeue(&moveJob);

		// ���� ã��
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(moveJob->sessionID)];

		// ������� ��ȿ�� ������ ��
		// objectPtr == nullptr�̸� OnEnter ���� �ʿ��� ����
		// �ڱ� �ڽ� map ���� �־�� ��
		// ���ǿ� ���� ����
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
	// ���� �޽��� ó��
	for (auto &[key, value] : m_umapSessions)
	{
		NETWORK_SERVER::CNetSession *pSession = NETWORK_SERVER::g_NetServer->m_arrPSessions[NETWORK_SERVER::CNetServer::GetIndex(key)];
		LONG recvMsgCount = pSession->m_RecvMsgQueue.GetUseSize();
		for (int i = 0; i < recvMsgCount; i++)
		{
			CSerializableBufferView<FALSE> *pMsg;
			pSession->m_RecvMsgQueue.Dequeue(&pMsg);

			RECV_RET ret = OnRecv(key, pMsg);
			// �̵� �޽��� ó��
			if (ret == RECV_RET::RECV_MOVE)
			{
				if (pMsg->DecreaseRef() == 0)
					CSerializableBufferView<FALSE>::Free(pMsg);
				break;
			}

			// �߸��� �޽��� ����
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
