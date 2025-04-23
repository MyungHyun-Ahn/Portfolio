#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "CBaseContents.h"
#include "CGameContents.h"
#include "CGameServer.h"
#include "CPlayer.h"
#include "CGenPacket.h"
#include "SystemEvent.h"
#include "CContentsThread.h"

void CAuthContents::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	CNonLoginPlayer *pNonLoginPlayer = CNonLoginPlayer::Alloc();
	m_umapSessions.insert(std::make_pair(sessionID, pNonLoginPlayer));

}

void CAuthContents::OnLeave(const UINT64 sessionID) noexcept
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

RECV_RET CAuthContents::OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message, int delayFrame) noexcept
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
		if (message->GetDataSize() != sizeof(INT64) + 64 * sizeof(char) + sizeof(int))
			return RECV_RET::RECV_FALSE;

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
		CEchoContents *pEchoContent = ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContents;
		CPlayer *pPlayer = CPlayer::Alloc();
		pPlayer->m_iAccountNo = accountNo;
		pEchoContent->MoveJobEnqueue(sessionID, pPlayer);

		CSerializableBuffer<FALSE> *pLoginRes = CGenPacket::makePacketResLogin(1, accountNo);
		NET_SERVER::g_NetServer->SendPacket(sessionID, pLoginRes);


		return RECV_RET::RECV_MOVE;
	}
	break;
	default:
		break;
	}

	return RECV_RET::RECV_FALSE;
}

void CAuthContents::OnLoopEnd() noexcept
{
}

void CEchoContents::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	m_umapSessions.insert(std::make_pair(sessionID, pObject));
}

void CEchoContents::OnLeave(const UINT64 sessionID) noexcept
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

RECV_RET CEchoContents::OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message, int delayFrame) noexcept
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

		if (message->GetDataSize() != sizeof(INT64) + sizeof(LONGLONG))
			return RECV_RET::RECV_FALSE;

		*message >> accountNo >> sendTick;

		if (accountNo != pPlayer->m_iAccountNo)
			return RECV_RET::RECV_FALSE;
		
		if (message->GetDataSize() != 0)
			return RECV_RET::RECV_FALSE;

		CSerializableBuffer<FALSE> *pEchoRes = CGenPacket::makePacketResEcho(accountNo, sendTick);
		NET_SERVER::g_NetServer->EnqueuePacket(sessionID, pEchoRes);
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

void CEchoContents::OnLoopEnd() noexcept
{
	// NET_SERVER::g_NetServer->SendAllPQCS();
}
