#pragma once
class CMonitor
{
public:
	CMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void Update(INT sessionCount, INT playerCount);
	void UpdateCpuTime();
	void UpdateServer();
	void UpdateMemory();
	void UpdatePDH();

	void MonitoringConsole(INT sessionCount, INT playerCount);
	void MonitoringFile(INT sessionCount, INT playerCount);

public:
	// CPU 모니터링 정보
	HANDLE m_hProcess;
	INT m_iNumberOfProcessors;
	FLOAT m_fProcessorTotal = 0;
	FLOAT m_fProcessorUser = 0;
	FLOAT m_fProcessorKernel = 0;

	FLOAT m_fProcessTotal = 0;
	FLOAT m_fProcessUser = 0;
	FLOAT m_fProcessKernel = 0;

	ULARGE_INTEGER m_ftProcessor_LastKernel;
	ULARGE_INTEGER m_ftProcessor_LastUser;
	ULARGE_INTEGER m_ftProcessor_LastIdle;

	ULARGE_INTEGER m_ftProcess_LastKernel;
	ULARGE_INTEGER m_ftProcess_LastUser;
	ULARGE_INTEGER m_ftProcess_LastTime;

	// PDH
	PDH_HQUERY  m_ProcessNPMemoryQuery;
	PDH_HQUERY  m_SystemNPMemoryQuery;
	PDH_HQUERY  m_SystemAvailableMemoryQuery;

	PDH_HCOUNTER m_ProcessNPMemoryCounter;
	PDH_HCOUNTER m_SystemNPMemoryCounter;
	PDH_HCOUNTER m_SystemAvailableMemoryCounter;

	PDH_FMT_COUNTERVALUE m_ProcessNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_SystemNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_SystemAvailableMemoryVal;

	// 메모리 사용량 모니터링 정보
	PROCESS_MEMORY_COUNTERS_EX m_stPmc;

	// Client 모니터링 정보
	LONG m_lWaitEchoCount = 0;
	DWORD m_dwTime = 0;
	INT64 m_lConnectTotal = 0;
	LONG m_lConnectTPS = 0;
	LONG m_lDisconncetFromServer = 0;
	LONG m_lRecvTPS = 0;
	LONG m_lSendTPS = 0;

	// Error
	LONG m_lLoginPacketDuplication = 0;
	LONG m_lLoginPacketNotRecv = 0;
	LONG m_lEchoNotRecv = 0;
	LONG m_lPacketError = 0;
};

extern CMonitor g_monitor;

