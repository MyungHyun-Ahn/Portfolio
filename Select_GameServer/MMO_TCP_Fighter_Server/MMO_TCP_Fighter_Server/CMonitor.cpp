#include "pch.h"
#include "Config.h"
#include "CMonitor.h"

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess)
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_hProcess = GetCurrentProcess();
	}

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

	UpdateCpuTime();
}

void CMonitor::Update(INT sessionCount, INT playerCount)
{
	UpdateCpuTime();

	if (m_lFPS != FRAME_PER_SECOND)
		g_Logger->WriteLog(L"ServerFPS", LOG_LEVEL::SYSTEM, L"FPS %d", m_lFPS);

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

	ULONGLONG kernelDiff	= kernel.QuadPart	- m_ftProcessor_LastKernel.QuadPart;
	ULONGLONG userDiff		= user.QuadPart		- m_ftProcessor_LastUser.QuadPart;
	ULONGLONG idleDiff		= idle.QuadPart		- m_ftProcessor_LastIdle.QuadPart;

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
	InterlockedExchange(&m_lLoopCount, 0);
	InterlockedExchange(&m_lFPS, 0);
	InterlockedExchange(&m_lAcceptTPS, 0);
	InterlockedExchange(&m_lRecvTPS, 0);
	InterlockedExchange(&m_lSendTPS, 0);
}

void CMonitor::MonitoringConsole(INT sessionCount, INT playerCount)
{
	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"             Cpu Usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Processor usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process Usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f", m_fProcessKernel);

	// Server Monitor
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\n\n\n        Server monitoring info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer count  \t: %d\n", playerCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Frame");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tFPS \t : %d\n\tLoop\t : %d\n", m_lFPS, m_lLoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept\t : %d", m_lAcceptTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}