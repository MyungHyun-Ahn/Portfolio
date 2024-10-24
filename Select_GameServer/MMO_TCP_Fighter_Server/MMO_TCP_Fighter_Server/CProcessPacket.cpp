#include "pch.h"
#include "Config.h"
#include "CSession.h"
#include "DefinePacket.h"
#include "CServerCore.h"
#include "CProcessPacket.h"

CProcessPacket g_ProcessPacket;
CProcessPacketInterface *g_pProcessPacket = &g_ProcessPacket;

bool CProcessPacket::PacketProcCSMoveStart(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}

bool CProcessPacket::PacketProcCSMoveStop(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}

bool CProcessPacket::PacketProcCSAttack1(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}

bool CProcessPacket::PacketProcCSAttack2(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}

bool CProcessPacket::PacketProcCSAttack3(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}

bool CProcessPacket::PacketProcCSEcho(UINT64 sessionId, CSerializableBuffer *message)
{
	return false;
}
