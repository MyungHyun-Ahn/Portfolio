#include "pch.h"
#include "CMonitor.h"

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess)
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_hProcess = GetCurrentProcess();
	}

	// �ܼ� ��¿� �´� ũ��� ����
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r);
	MoveWindow(console, r.left, r.top, 500, 600, TRUE);

	// ���μ��� ���� Ȯ��
	//  * ���μ��� ����� ���� cpu ������ ������ ���� ������ ����
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

	m_ftProcessor_LastKernel.QuadPart = 0;
	m_ftProcessor_LastUser.QuadPart = 0;
	m_ftProcessor_LastIdle.QuadPart = 0;

	m_ftProcess_LastKernel.QuadPart = 0;
	m_ftProcess_LastUser.QuadPart = 0;
	m_ftProcess_LastTime.QuadPart = 0;

	PdhOpenQuery(NULL, NULL, &m_ProcessNPMemoryQuery);
	PdhOpenQuery(NULL, NULL, &m_SystemNPMemoryQuery);
	PdhOpenQuery(NULL, NULL, &m_SystemAvailableMemoryQuery);

	PdhAddCounter(m_ProcessNPMemoryQuery, L"\\Process(IOCP_ECHO_SERVER)\\Pool Nonpaged Bytes", NULL, &m_ProcessNPMemoryCounter);
	PdhAddCounter(m_SystemNPMemoryQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_SystemNPMemoryCounter);
	PdhAddCounter(m_SystemAvailableMemoryQuery, L"\\Memory\\Available MBytes", NULL, &m_SystemAvailableMemoryCounter);
	
	// ù ����
	PdhCollectQueryData(m_ProcessNPMemoryQuery);
	PdhCollectQueryData(m_SystemNPMemoryQuery);
	PdhCollectQueryData(m_SystemAvailableMemoryQuery);

	UpdateCpuTime();
	UpdatePDH();
}

void CMonitor::Update(INT sessionCount, INT playerCount)
{
	UpdateCpuTime();
	// UpdateMemory();
	// UpdatePDH();

	// ����͸� �ܼ�
	MonitoringConsole(sessionCount, playerCount);

	UpdateServer();
}

void CMonitor::UpdateCpuTime()
{
	// ���μ��� ������ ����
	// FILETIME ����ü�� ��������� ULARGE_INTEGER�� ������ �����Ƿ� �̸� ���
	ULARGE_INTEGER idle;
	ULARGE_INTEGER kernel;
	ULARGE_INTEGER user;

	// �ý��� ��� �ð��� ����
	// ���̵� Ÿ�� / Ŀ�� ��� Ÿ��(���̵� ����) / ���� ��� Ÿ��
	if (GetSystemTimes((PFILETIME)&idle, (PFILETIME)&kernel, (PFILETIME)&user) == false)
		return;

	ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessor_LastKernel.QuadPart;
	ULONGLONG userDiff = user.QuadPart - m_ftProcessor_LastUser.QuadPart;
	ULONGLONG idleDiff = idle.QuadPart - m_ftProcessor_LastIdle.QuadPart;

	ULONGLONG total = kernelDiff + userDiff;
	ULONGLONG timeDiff;

	if (total == 0)
	{
		m_fProcessorUser = 0.0f;
		m_fProcessorKernel = 0.0f;
		m_fProcessorTotal = 0.0f;
	}
	else
	{
		m_fProcessorTotal = (float)((double)(total - idleDiff) / total * 100.0f);
		m_fProcessorUser = (float)((double)(userDiff) / total * 100.0f);
		m_fProcessorKernel = (float)((double)(kernelDiff - idleDiff) / total * 100.0f);
	}

	m_ftProcessor_LastKernel = kernel;
	m_ftProcessor_LastUser = user;
	m_ftProcessor_LastIdle = idle;

	// ������ ���μ��� ������ ����
	ULARGE_INTEGER none;
	ULARGE_INTEGER nowTime;

	// 100 ���뼼���� ���� �ð��� ����. UTC
	// ���μ��� ���� �Ǵ� ����
	// a = ���� ������ �ý��� �ð��� ���� (���� �帥 �ð�)
	// b = ���μ����� CPU ��� �ð��� ����
	// a : 100 = b : ���� ������ ���� ������ ���


	// 100 ���뼼���� ������ �ð� ����
	GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

	// �ش� ���μ����� ����� �ð��� ����
	// �ι�°, ����°�� ����, ���� �ð����� �̻��
	GetProcessTimes(m_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);

	// ������ ����� ���μ��� �ð����� ���� ���� ������ ���� �ð��� �������� Ȯ��
	// ���� ������ �ð����� ������ ����
	timeDiff = nowTime.QuadPart - m_ftProcess_LastTime.QuadPart;
	userDiff = user.QuadPart - m_ftProcess_LastUser.QuadPart;
	kernelDiff = kernel.QuadPart - m_ftProcess_LastKernel.QuadPart;

	total = kernelDiff + userDiff;

	m_fProcessTotal = (float)(total / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);
	m_fProcessKernel = (float)(kernelDiff / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);
	m_fProcessUser = (float)(userDiff / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);

	m_ftProcess_LastTime = nowTime;
	m_ftProcess_LastKernel = kernel;
	m_ftProcess_LastUser = user;
}

void CMonitor::UpdateServer()
{
	InterlockedExchange(&m_lConnectTPS, 0);
	InterlockedExchange(&m_lRecvTPS, 0);
	InterlockedExchange(&m_lSendTPS, 0);
}

void CMonitor::UpdateMemory()
{
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_stPmc, sizeof(m_stPmc));
}

void CMonitor::UpdatePDH()
{
	PdhCollectQueryData(m_ProcessNPMemoryQuery);
	PdhCollectQueryData(m_SystemNPMemoryQuery);
	PdhCollectQueryData(m_SystemAvailableMemoryQuery);

	PdhGetFormattedCounterValue(m_ProcessNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_ProcessNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemAvailableMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemAvailableMemoryVal);
}

void CMonitor::MonitoringConsole(INT sessionCount, INT playerCount)
{
	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"              System info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);

	// Server Monitor
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"        Server monitoring info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\Connect total \t: %lld", m_lConnectTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Error");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\Disconnect from server \t: %d", m_lDisconncetFromServer);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\LoginPacket duplication \t: %d", m_lLoginPacketDuplication);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\LoginPacket not recv (3s) \t: %d", m_lLoginPacketNotRecv);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\Echo not recv (500ms) \t: %d", m_lEchoNotRecv);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\Packet error \t: %d", m_lPacketError);


	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Pool capacity");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tBuffer pool capacity \t: %d, usage \t: %d", CSerializableBuffer::GetPoolCapacity(), CSerializableBuffer::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tConnect\t : %d", m_lConnectTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}

void CMonitor::MonitoringFile(INT sessionCount, INT playerCount)
{
	// Cpu Usage
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"              System info");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Process Usage");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tKernel \t: %f", m_fProcessKernel);

	// Server Monitor 
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\n\n\n        Server monitoring info");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_lConnectTotal);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Pool capacity");
	// g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tSession pool capacity \t: %d", CSession::GetPoolCapacity());
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tBuffer pool capacity \t: %d", CSerializableBuffer::GetPoolCapacity());
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tAccept\t : %d", m_lConnectTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"----------------------------------------");
}