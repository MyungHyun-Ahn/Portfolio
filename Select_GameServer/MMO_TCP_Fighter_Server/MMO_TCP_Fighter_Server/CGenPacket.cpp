#include "pch.h"
#include "DefinePacket.h"
#include "CGenPacket.h"

CSerializableBuffer *CGenPacket::makePacketSCCreateMyCharacter(INT id, CHAR direction, USHORT x, USHORT y, BYTE hp)
{
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
	BYTE type = (BYTE)PACKET_CODE::SCCreateMyCharacter;
	*pSBuffer << type << id << direction << x << y << hp;
	return pSBuffer;
}

CSerializableBuffer *CGenPacket::makePacketSCCreateOtherCharacter(INT id, CHAR direction, USHORT x, USHORT y, BYTE hp)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCDeleteCharacter(INT id)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCMoveStart(INT id, CHAR direction, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCMoveStop(INT id, CHAR direction, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCAttack1(INT id, CHAR direction, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCAttack2(INT id, CHAR direction, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCAttack3(INT id, CHAR direction, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCDamage(INT attackId, INT damageId, CHAR damageHp)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCSync(INT id, USHORT x, USHORT y)
{
	return nullptr;
}

CSerializableBuffer *CGenPacket::makePacketSCEcho(DWORD time)
{
	return nullptr;
}
