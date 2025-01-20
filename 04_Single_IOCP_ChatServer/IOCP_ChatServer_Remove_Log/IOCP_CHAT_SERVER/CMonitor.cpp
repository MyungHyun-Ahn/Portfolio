#include "pch.h"
#include "CMonitor.h"
#include "CPlayer.h"
#include "CNetSession.h"

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess) noexcept
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_hProcess = GetCurrentProcess();
	}

	// �ܼ� ��¿� �´� ũ��� ����
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r);
	MoveWindow(console, r.left, r.top, 700, 900, TRUE);

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

	PdhOpenQuery(NULL, NULL, &m_PDHQuery);

	PdhAddCounter(m_PDHQuery, L"\\Process(IOCP_CHAT_SERVER)\\Pool Nonpaged Bytes", NULL, &m_ProcessNPMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_SystemNPMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Memory\\Available MBytes", NULL, &m_SystemAvailableMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Sent/sec", NULL, &m_NetworkSendKB1Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Sent/sec", NULL, &m_NetworkSendKB2Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Received/sec", NULL, &m_NetworkRecvKB1Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Received/sec", NULL, &m_NetworkRecvKB2Counter);
	PdhAddCounter(m_PDHQuery, L"\\TCPv4\\Segments Retransmitted/sec", NULL, &m_NetworkRetransmissionCounter);
	
	// ù ����
	PdhCollectQueryData(m_PDHQuery);

	UpdateCpuTime();
	UpdatePDH();
}

void CMonitor::Update(INT sessionCount, INT playerCount) noexcept
{
	UpdateCpuTime();
	UpdateMemory();
	UpdatePDH();

	// ����͸� �ܼ�
	MonitoringConsole(sessionCount, playerCount);

	UpdateServer();
}

void CMonitor::UpdateCpuTime() noexcept
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

void CMonitor::UpdateServer() noexcept
{
	InterlockedExchange(&m_lAcceptTPS, 0);
	InterlockedExchange(&m_lRecvTPS, 0);
	InterlockedExchange(&m_lSendTPS, 0);
	InterlockedExchange(&m_lUpdateTPS, 0);
	InterlockedExchange(&m_lServerFrame, 0);
}

void CMonitor::UpdateMemory() noexcept
{
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_stPmc, sizeof(m_stPmc));
}

void CMonitor::UpdatePDH() noexcept
{
	PdhCollectQueryData(m_PDHQuery);

	PdhGetFormattedCounterValue(m_ProcessNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_ProcessNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemAvailableMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemAvailableMemoryVal);
	PdhGetFormattedCounterValue(m_NetworkSendKB1Counter, PDH_FMT_LARGE, NULL, &m_NetworkSendKB1Val);
	PdhGetFormattedCounterValue(m_NetworkSendKB2Counter, PDH_FMT_LARGE, NULL, &m_NetworkSendKB2Val);
	PdhGetFormattedCounterValue(m_NetworkRecvKB1Counter, PDH_FMT_LARGE, NULL, &m_NetworkRecvKB1Val);
	PdhGetFormattedCounterValue(m_NetworkRecvKB2Counter, PDH_FMT_LARGE, NULL, &m_NetworkRecvKB2Val);
	PdhGetFormattedCounterValue(m_NetworkRetransmissionCounter, PDH_FMT_LARGE, NULL, &m_NetworkRetransmissionVal);
}

void CMonitor::MonitoringConsole(INT sessionCount, INT playerCount) noexcept
{
	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"              System info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process memory usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\ttotal \t: %u MB", m_stPmc.PrivateUsage / (1024 * 1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u KB\n", m_ProcessNPMemoryVal.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"System memory usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tavailable \t: %u MB", m_SystemAvailableMemoryVal.largeValue);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u MB\n", m_SystemNPMemoryVal.largeValue / (1024 * 1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Processor CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Network");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend1 \t: %u KB", m_NetworkSendKB1Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend2 \t: %u KB", m_NetworkSendKB2Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv1 \t: %u KB", m_NetworkRecvKB1Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv2 \t: %u KB", m_NetworkRecvKB2Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRetransmission \t: %u\n", m_NetworkRetransmissionVal.largeValue);
	

	// Server Monitor
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"        Server monitoring info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tServer FPS \t: %lld", m_lServerFrame);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_lAcceptTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer count \t: %d\n", playerCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Pool capacity");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession pool capacity \t: %d, usage \t: %d", CNetSession::GetPoolCapacity(), CNetSession::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tBuffer pool capacity \t: %d, usage \t: %d", CSerializableBuffer<FALSE>::GetPoolCapacity(), CSerializableBuffer<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tView pool capacity \t: %d, usage \t: %d", CSerializableBufferView<FALSE>::GetPoolCapacity(), CSerializableBufferView<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv pool capacity \t: %d, usage \t: %d", CRecvBuffer::GetPoolCapacity(), CRecvBuffer::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer pool capacity \t: %d, usage \t: %d\n", CPlayer::GetPoolCapacity(), CPlayer::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept\t : %d", m_lAcceptTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUpdate\t : %d", m_lUpdateTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}