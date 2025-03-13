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
	timeMs = 1000 * 60; // 1��
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

	// 0 ��
	if (mon != t.tm_mon + 1)
	{
		// ���̺� ����
		mon = t.tm_mon + 1;
		// �����ص� �������
		pConnector->Query(L"CREATE TABLE monitorlog_%04d%02d LIKE monitorlog_temp", t.tm_year + 1900, t.tm_mon + 1);
	}
	MonitoringInfo *pArrMonitorInfo = ((CLanMonitoringServer *)LAN_SERVER::g_LanServer)->m_arrMonitorInfo;

	for (int i = 0; i < dfMONITOR_DATA_TYPE_END; i++)
	{
		// timeStamp�� 0�̸� �ƹ��� ���� ���� �� - db ���� ����
		if(pArrMonitorInfo[i].timeStamp == 0)
			continue;

		pConnector->Query(L"INSERT INTO monitorlog_%04d%02d (serverno, type, value) VALUES (%d, %d, %d);", t.tm_year + 1900, t.tm_mon + 1, pArrMonitorInfo[i].serverNo, i, pArrMonitorInfo[i].dataValue);
	}
}
