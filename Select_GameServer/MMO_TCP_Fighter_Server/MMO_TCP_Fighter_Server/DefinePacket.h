#pragma once

#pragma pack(push, 1)
struct PacketHeader
{
	BYTE		byCode;
	BYTE		bySize;
};
#pragma pack(pop)

enum class PACKET_CODE
{
	// S -> C Packet
	SCCreateMyCharacter = 0,
	SCCreateOtherCharacter = 1,
	SCDeleteCharacter = 2,
	SCMoveStart = 11,
	SCMoveStop = 13,
	SCAttack1 = 21,
	SCAttack2 = 23,
	SCAttack3 = 25,
	SCDamage = 30,
	SCSync = 251,
	SCEcho = 253,


	// C -> S Packet
	CSMoveStart = 10,
	CSMoveStop = 12,
	CSAttack1 = 20,
	CSAttack2 = 22,
	CSAttack3 = 24,
	CSEcho = 252,
};