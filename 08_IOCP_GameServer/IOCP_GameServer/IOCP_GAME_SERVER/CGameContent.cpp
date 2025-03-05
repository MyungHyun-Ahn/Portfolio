#include "pch.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "CBaseContent.h"
#include "CGameContent.h"
#include "CPlayer.h"
#include "CGenPacket.h"

void CAuthContent::OnEnter(const UINT64 sessionID, void *pObject) noexcept
{
	// pObject�� nullptr �� �Ѿ���� ����

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

		// ���� �˻�, ���� Ű ����

		// ���� ���

		// Player ����
		 
		// Echo ��� ��ȯ
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
	m_umapSessions.insert(std::make_pair(sessionID, pObject));
}

void CEchoContent::OnLeave(const UINT64 sessionID) noexcept
{
	m_umapSessions.erase(sessionID);
}

RECV_RET CEchoContent::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
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
			// Player ã�� ����
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

		NETWORK_SERVER::g_NetServer->SendPacket(sessionID, pEchoRes);

		if (pEchoRes->DecreaseRef() == 0)
			CSerializableBuffer<FALSE>::Free(pEchoRes);
	}
	return RECV_RET::RECV_TRUE;

	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
	{
		// ��Ʈ��Ʈ type�� ����
		if (message->GetDataSize() != 0)
			return RECV_RET::RECV_FALSE;

		auto it = m_umapSessions.find(sessionID);
		if (it == m_umapSessions.end())
		{
			// Player ã�� ����
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
