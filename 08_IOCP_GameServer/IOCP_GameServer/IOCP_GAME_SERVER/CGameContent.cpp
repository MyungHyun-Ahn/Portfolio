#include "pch.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "CBaseContent.h"
#include "CGameContent.h"
#include "CPlayer.h"
#include "CGenPacket.h"

void CAuthContent::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	// pObject는 nullptr 이 넘어왔을 것임

	CNonLoginPlayer *m_nonLoginPlayer = new CNonLoginPlayer;
	m_umapSessions.insert(std::make_pair(sessionID, m_nonLoginPlayer));

}

void CAuthContent::OnLeave(const UINT64 sessionID) noexcept
{
}

RECV_RET CAuthContent::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
	{
		CNonLoginPlayer *pPlayer = (CNonLoginPlayer *)m_umapSessions[sessionID];

		INT64 accountNo;
		char sessionKey[64];
		int version;

		*message >> accountNo;
		message->Dequeue(sessionKey, sizeof(char) * 64);
		*message >> version;

		// 버전 검사, 세션 키 인증

		// 인증 통과

		// Echo 모드 전환
		// g_NetServer -> EchoContent -> EnqueueMoveJob

		return RECV_RET::RECV_MOVE;
	}
	break;
	default:
		break;
	}

	return RECV_RET::RECV_FALSE;
}

void CEchoContent::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
}

void CEchoContent::OnLeave(const UINT64 sessionID) noexcept
{
}

RECV_RET CEchoContent::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_ECHO:
	{
		INT64 accountNo;
		LONGLONG sendTick;
		*message >> accountNo >> sendTick;

		CSerializableBuffer<FALSE> *pEchoRes = CGenPacket::makePacketResEcho(accountNo, sendTick);
		pEchoRes->IncreaseRef();

		NETWORK_SERVER::g_NetServer->SendPacket(sessionID, pEchoRes);

		if (pEchoRes->DecreaseRef() == 0)
			CSerializableBuffer<FALSE>::Free(pEchoRes);
	}
	return RECV_RET::RECV_TRUE;

	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
	{
		
	}
	return RECV_RET::RECV_TRUE;
	default:
		break;
	}

	return RECV_RET::RECV_FALSE;
}
