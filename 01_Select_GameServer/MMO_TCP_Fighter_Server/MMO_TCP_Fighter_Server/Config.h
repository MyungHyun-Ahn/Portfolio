#pragma once
/*
	Config.h
		* ���� ���� ���� �ɼǰ� ���� ���� �ɼ� �ۼ�
		* ���� ��� �ɼ��� config ���� �ļ��� ���� �� �ű� ��
*/

// ���� ����
// ���� IP, Port
constexpr const char *SERVER_IP = "0.0.0.0";
constexpr short SERVER_PORT = 10611;

// ���� ������ ����
constexpr int FRAME_PER_SECOND = 25;
constexpr int TICK_PER_FRAME = (1000 / FRAME_PER_SECOND);

// ��Ŷ �ĺ��� - �̰��� ���� ������ �߸��� ��Ŷ���� ����
constexpr BYTE PACKET_IDENTIFIER = 0x89;

// 30�� �̻��� �ǵ��� �ƹ��� �޽��� ���ŵ� ���°�� ���� ����
constexpr int NETWORK_PACKET_RECV_TIMEOUT = 30000;


// ���� ����
// ȭ�� �̵� ����
constexpr int RANGE_MOVE_TOP = 0;
constexpr int RANGE_MOVE_LEFT = 0;
constexpr int RANGE_MOVE_RIGHT = 6400;
constexpr int RANGE_MOVE_BOTTOM = 6400;

// ���� ũ��
constexpr int SECTOR_SIZE = 160;

// �ֺ� ��ĭ���� ���̰��� ������
constexpr int SECTOR_VIEW_START = 1; // �밢�� 1ĭ ������ ����
constexpr int SECTOR_VIEW_COUNT = 3; // 3 x 3

constexpr int SECTOR_MAX_X = (RANGE_MOVE_RIGHT - RANGE_MOVE_LEFT) / SECTOR_SIZE;
constexpr int SECTOR_MAX_Y = (RANGE_MOVE_BOTTOM - RANGE_MOVE_TOP) / SECTOR_SIZE;

// �ִ� ü��
constexpr int MAX_PLAYER_HP = 100;

// ���ݹ���
constexpr int ATTACK1_RANGE_X = 80;
constexpr int ATTACK2_RANGE_X = 90;
constexpr int ATTACK3_RANGE_X = 100;
constexpr int ATTACK1_RANGE_Y = 10;
constexpr int ATTACK2_RANGE_Y = 10;
constexpr int ATTACK3_RANGE_Y = 20;


// ���� ������
constexpr int ATTACK1_DAMAGE = 1;
constexpr int ATTACK2_DAMAGE = 2;
constexpr int ATTACK3_DAMAGE = 3;


// ĳ���� �̵� �ӵ�   // 25fps ���� �̵��ӵ�
constexpr int SPEED_PLAYER_X = 3 * (50 / FRAME_PER_SECOND);	// 3   50fps;
constexpr int SPEED_PLAYER_Y = 2 * (50 / FRAME_PER_SECOND);	// 2   50fps;


// �̵� ����üũ ����
constexpr int ERROR_RANGE = 50;