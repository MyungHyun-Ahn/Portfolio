#include "pch.h"
#include "Config.h"
#include "CSession.h"
#include "DefinePacket.h"
#include "CServerCore.h"
#include "CPlayer.h"
#include "CGameServer.h"
#include "CProcessPacket.h"

bool CGameProcessPacket::PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}

bool CGameProcessPacket::PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}

bool CGameProcessPacket::PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}

bool CGameProcessPacket::PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}

bool CGameProcessPacket::PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}

bool CGameProcessPacket::PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message)
{
	return true;
}
