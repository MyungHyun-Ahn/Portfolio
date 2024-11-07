#include "pch.h"
#include "DefinePacket.h"
#include "CGenPacket.h"
CSerializableBuffer *CGenPacket::makePacketSCCreateMyCharacter( INT id, CHAR direction, USHORT x, USHORT y, BYTE hp)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCCreateMyCharacter;
	*pSBuffer << type << id <<  direction <<  x <<  y <<  hp;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCCreateOtherCharacter( INT id, CHAR direction, USHORT x, USHORT y, BYTE hp)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCCreateOtherCharacter;
	*pSBuffer << type << id <<  direction <<  x <<  y <<  hp;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCDeleteCharacter( INT id)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCDeleteCharacter;
	*pSBuffer << type << id;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCMoveStart( INT id, CHAR direction, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCMoveStart;
	*pSBuffer << type << id <<  direction <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCMoveStop( INT id, CHAR direction, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCMoveStop;
	*pSBuffer << type << id <<  direction <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCAttack1( INT id, CHAR direction, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCAttack1;
	*pSBuffer << type << id <<  direction <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCAttack2( INT id, CHAR direction, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCAttack2;
	*pSBuffer << type << id <<  direction <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCAttack3( INT id, CHAR direction, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCAttack3;
	*pSBuffer << type << id <<  direction <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCDamage( INT attackId, INT damageId, CHAR damageHp)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCDamage;
	*pSBuffer << type << attackId <<  damageId <<  damageHp;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCSync( INT id, USHORT x, USHORT y)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCSync;
	*pSBuffer << type << id <<  x <<  y;
	return pSBuffer;
} 

CSerializableBuffer *CGenPacket::makePacketSCEcho( DWORD time)
{
#ifdef USE_OBJECT_POOL
	CSerializableBuffer *pSBuffer = CSerializableBuffer::Alloc();
#else
	CSerializableBuffer *pSBuffer = new CSerializableBuffer;
#endif
	BYTE type = (BYTE)PACKET_CODE::SCEcho;
	*pSBuffer << type << time;
	return pSBuffer;
} 