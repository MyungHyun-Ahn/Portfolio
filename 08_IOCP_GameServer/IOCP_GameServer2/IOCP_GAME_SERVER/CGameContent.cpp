#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "CBaseContent.h"
#include "CGameContent.h"
#include "CGameServer.h"
#include "CPlayer.h"
#include "CGenPacket.h"
#include "SystemEvent.h"
#include "CContentThread.h"

void CAuthContent::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	// pObject는 nullptr 이 넘어왔을 것임
	// 이 컨텐츠에서는 non login player를 직접 만들어야 함
	CNonLoginPlayer *pNonLoginPlayer = CNonLoginPlayer::Alloc();
	m_umapSessions.insert(std::make_pair(sessionID, pNonLoginPlayer));

}

void CAuthContent::OnLeave(const UINT64 sessionID) noexcept
{
	auto it = m_umapSessions.find(sessionID);
	if (it == m_umapSessions.end())
	{
		// __debugbreak();
		return;
	}

	CNonLoginPlayer *pNonLoginPlayer = (CNonLoginPlayer *)it->second;
	CNonLoginPlayer::Free(pNonLoginPlayer);
	m_umapSessions.erase(sessionID);
}

RECV_RET CAuthContent::OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message) noexcept
{
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
	{
		CNonLoginPlayer *pNonLoginPlayer = (CNonLoginPlayer *)m_umapSessions[sessionID];

		INT64 accountNo;
		char sessionKey[64];
		int version;

		*message >> accountNo;
		message->Dequeue(sessionKey, sizeof(char) * 64);
		*message >> version;

		if (message->GetDataSize() != 0)
			return RECV_RET::RECV_FALSE;

		// 버전 검사, 세션 키 인증

		// 인증 통과

		// Echo 컨텐츠
		// Auth Content에서 삭제
		LeaveJobEnqueue(sessionID);

		// EchoContent에 MoveJob Enqueue
		CEchoContent *pEchoContent = ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent;
		CPlayer *pPlayer = CPlayer::Alloc();
		pPlayer->m_iAccountNo = accountNo;
		pEchoContent->MoveJobEnqueue(sessionID, pPlayer);

		CSerializableBuffer<FALSE> *pLoginRes = CGenPacket::makePacketResLogin(1, accountNo);
		pLoginRes->IncreaseRef();
		NET_SERVER::g_NetServer->SendPacket(sessionID, pLoginRes);
		if (pLoginRes->DecreaseRef() == 0)
			CSerializableBuffer<FALSE>::Free(pLoginRes);


		return RECV_RET::RECV_MOVE;
	}
	break;
	default:
		break;
	}

	return RECV_RET::RECV_FALSE;
}

void CAuthContent::OnLoopEnd() noexcept
{
}

void CEchoContent::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	m_umapSessions.insert(std::make_pair(sessionID, pObject));
}

void CEchoContent::OnLeave(const UINT64 sessionID) noexcept
{
	auto it = m_umapSessions.find(sessionID);
	if (it == m_umapSessions.end())
	{
		__debugbreak();
		return;
	}

	CPlayer *pPlayer = (CPlayer *)it->second;
	CPlayer::Free(pPlayer);
	m_umapSessions.erase(sessionID);
}

RECV_RET CEchoContent::OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message) noexcept
{
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_ECHO:
	{
		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			// Player 찾기 실패
			return RECV_RET::RECV_FALSE;
		}

		CPlayer *pPlayer = (CPlayer *)it->second;

		INT64 accountNo;
		LONGLONG sendTick;
		*message >> accountNo >> sendTick;

		if (accountNo != pPlayer->m_iAccountNo)
			return RECV_RET::RECV_FALSE;
		
		if (message->GetDataSize() != 0)
			return RECV_RET::RECV_FALSE;

		CSerializableBuffer<FALSE> *pEchoRes = CGenPacket::makePacketResEcho(accountNo, sendTick);
		pEchoRes->IncreaseRef();
		// EnqueuePacketEvent *pEnqueueEvent = new EnqueuePacketEvent;
		// pEnqueueEvent->SetEvent(sessionID, pEchoRes);
		// CContentThread::EnqueueEvent(pEnqueueEvent);

		NET_SERVER::g_NetServer->EnqueuePacket(sessionID, pEchoRes);
		if (pEchoRes->DecreaseRef() == 0)
			CSerializableBuffer<FALSE>::Free(pEchoRes);
	}
	return RECV_RET::RECV_TRUE;

	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
	{
		// 하트비트 type만 있음
		if (message->GetDataSize() != 0)
			return RECV_RET::RECV_FALSE;

		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			// Player 찾기 실패
			return RECV_RET::RECV_FALSE;
		}

		CPlayer *pPlayer = (CPlayer *)it->second;
		pPlayer->m_dwPrevRecvTime = timeGetTime();
	}
	return RECV_RET::RECV_TRUE;
	default:
		break;
	}

	return RECV_RET::RECV_FALSE;
}

void CEchoContent::OnLoopEnd() noexcept
{
	NET_SERVER::g_NetServer->SendAllPQCS();
}
