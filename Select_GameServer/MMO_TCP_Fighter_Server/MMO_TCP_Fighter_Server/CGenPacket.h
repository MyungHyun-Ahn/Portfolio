#pragma once
// ��Ŷ�� ������ִ� ���丮 Ŭ����
// * �ڵ�ȭ
class CGenPacket
{
public:
	static CSerializableBuffer *makePacketSCCreateMyCharacter(INT id, CHAR direction, USHORT x, USHORT y, BYTE hp);

	static CSerializableBuffer *makePacketSCCreateOtherCharacter(INT id, CHAR direction, USHORT x, USHORT y, BYTE hp);

	static CSerializableBuffer *makePacketSCDeleteCharacter(INT id);

	static CSerializableBuffer *makePacketSCMoveStart(INT id, CHAR direction, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCMoveStop(INT id, CHAR direction, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCAttack1(INT id, CHAR direction, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCAttack2(INT id, CHAR direction, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCAttack3(INT id, CHAR direction, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCDamage(INT attackId, INT damageId, CHAR damageHp);

	static CSerializableBuffer *makePacketSCSync(INT id, USHORT x, USHORT y);

	static CSerializableBuffer *makePacketSCEcho(DWORD time);
};

