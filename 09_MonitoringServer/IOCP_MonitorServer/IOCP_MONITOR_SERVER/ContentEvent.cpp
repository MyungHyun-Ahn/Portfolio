#include "pch.h"
#include "CNetServer.h"
#include "ContentEvent.h"
#include "DBSetting.h"
#include "CDBConnector.h"
#include "CommonProtocol.h"
#include "CLanServer.h"
#include "CLanMonitoringServer.h"

void DBTimerEvent::SetEvent() noexcept
{
	isPQCS = TRUE;
	timeMs = 1000 * 60; // 1분
	nextExecuteTime = timeGetTime();
	execute = std::bind(&DBTimerEvent::Excute, this);
}

void DBTimerEvent::Excute() noexcept
{
	// DB 
	CDBConnector *pConnector = CDBConnector::GetDBConnector();
	tm t;
	time_t timer = time(NULL);
	localtime_s(&t, &timer);

	// 0 값
	if (mon != t.tm_mon + 1)
	{
		// 테이블 생성
		mon = t.tm_mon + 1;
		// 실패해도 상관없음

		pConnector->Query(
			L"CREATE TABLE monitorlog_%04d%02d LIKE monitorlog_temp"
			, t.tm_year + 1900, t.tm_mon + 1);
	}
	MonitoringInfo *pArrMonitorInfo = ((CLanMonitoringServer *)LAN_SERVER::g_LanServer)->m_arrMonitorInfo;

	for (int i = 0; i < dfMONITOR_DATA_TYPE_END; i++)
	{
		// timeStamp가 0이면 아무도 쓰지 않은 것 - db 저장 안함
		if(pArrMonitorInfo[i].timeStamp == 0)
			continue;

		INT avg;
		INT min;
		INT max;

		AcquireSRWLockExclusive(&pArrMonitorInfo[i].srwLock);

		avg = pArrMonitorInfo[i].dataSum / 60;
		min = pArrMonitorInfo[i].dataMin;
		max = pArrMonitorInfo[i].dataMax;

		pArrMonitorInfo[i].dataSum = 0;
		pArrMonitorInfo[i].dataMin = INT_MAX;
		pArrMonitorInfo[i].dataMax = INT_MIN;

		ReleaseSRWLockExclusive(&pArrMonitorInfo[i].srwLock);

		pConnector->Query(L"INSERT INTO monitorlog_%04d%02d (serverno, type, avg, min, max) VALUES (%d, %d, %d, %d, %d);", t.tm_year + 1900, t.tm_mon + 1, pArrMonitorInfo[i].serverNo, i, avg, min, max);
	}
}
