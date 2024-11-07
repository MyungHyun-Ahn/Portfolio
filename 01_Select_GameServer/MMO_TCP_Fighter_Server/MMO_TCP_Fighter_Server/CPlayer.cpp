#include "pch.h"
#include "Config.h"
#include "GameDefine.h"
#include "CSector.h"
#include "CPlayer.h"

bool CPlayer::Move(SHORT x, SHORT y)
{
	bool flagX = true;
	bool flagY = true;

	if (m_sX + x < RANGE_MOVE_LEFT || m_sX + x >= RANGE_MOVE_RIGHT)
	{
		flagX = false;
	}

	if (m_sY + y < RANGE_MOVE_TOP || m_sY + y >= RANGE_MOVE_BOTTOM)
	{
		flagY = false;
	}

	if (flagX && flagY)
	{
		m_sX += x;
		m_sY += y;

		// 변경된 섹터 구하기
		int nowSecY = CSector::CalSectorY(m_sY);
		int nowSecX = CSector::CalSectorX(m_sX);

		if (nowSecX != m_sSecX || nowSecY != m_sSecY)
		{
			// 섹터 이동하며 Delete Create 메시지 보내기
			CSector::MoveSector(m_iId, m_sSecY, m_sSecX, nowSecY, nowSecX);
		}

		return true;
	}

	return false;
}
