#pragma once
/*
	GameDefine.h
		* ���ӿ��� ����� ����ü ���� ����
*/

// �̵� ����
enum class MOVE_DIR
{
	MOVE_DIR_LL = 0,
	MOVE_DIR_LU = 1,
	MOVE_DIR_UU = 2,
	MOVE_DIR_RU = 3,
	MOVE_DIR_RR = 4,
	MOVE_DIR_RD = 5,
	MOVE_DIR_DD = 6,
	MOVE_DIR_LD = 7,
	MOVE_DIR_STOP = 8
};

// Pos ����ü
struct stPos
{
	int m_iX;
	int m_iY;
};