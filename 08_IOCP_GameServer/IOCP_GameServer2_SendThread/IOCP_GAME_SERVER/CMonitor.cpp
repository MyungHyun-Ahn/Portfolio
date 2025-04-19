#include "pch.h"
#include "CMonitor.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "CLanClient.h"
#include "CMonitorClient.h"
#include "CGenPacket.h"
#include "CommonProtocol.h"
#include "CGameServer.h"
#include "CBaseContent.h"
#include "CGameContent.h"


CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess) noexcept
{
	time_t startTime = time(nullptr);
	localtime_s(&m_startTime, &startTime);

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

	PdhAddCounter(m_PDHQuery, L"\\Process(IOCP_GAME_SERVER)\\Pool Nonpaged Bytes", NULL, &m_ProcessNPMemoryCounter);
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

void CMonitor::Update() noexcept
{
	UpdateCpuTime();
	UpdateMemory();
	UpdatePDH();

	// ����͸� �ܼ�
	MonitoringConsole();
	SendMonitoringServer();

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
	m_LoopCount++;
	m_AcceptTPSTotal += m_lAcceptTPS;
	InterlockedExchange(&m_lAcceptTPS, 0);
	m_RecvTPSTotal += m_lRecvTPS;
	InterlockedExchange(&m_lRecvTPS, 0);
	m_SendTPSTotal += m_lSendTPS;
	InterlockedExchange(&m_lSendTPS, 0);
	m_UpdateTPSTotal += m_lUpdateTPS;
	InterlockedExchange(&m_lUpdateTPS, 0);

	InterlockedExchange(&m_lMaxSendCount, 0);
	InterlockedExchange(&m_iSendPendingCount, 0);
	InterlockedExchange(&m_iSendCallCount, 0);

	InterlockedExchange(&((CGameServer *)NET_SERVER::g_NetServer)->m_pAuthContent->m_FPS, 0);
	InterlockedExchange(&((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent->m_FPS, 0);
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

void CMonitor::MonitoringConsole() noexcept
{
	time_t currentTime = time(nullptr);
	localtime_s(&m_currentTime, &currentTime);

	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	// �� �� �� �� �� ��
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tstart \t: %04d.%02d.%02d.%02d.%02d.%02d",
		m_startTime.tm_year + 1900, m_startTime.tm_mon + 1, m_startTime.tm_mday,
		m_startTime.tm_hour, m_startTime.tm_min, m_startTime.tm_sec);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnow \t: %04d.%02d.%02d.%02d.%02d.%02d",
		m_currentTime.tm_year + 1900, m_currentTime.tm_mon + 1, m_currentTime.tm_mday,
		m_currentTime.tm_hour, m_currentTime.tm_min, m_currentTime.tm_sec);
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
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_lAcceptTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", NET_SERVER::g_NetServer->GetSessionCount());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tNonLoginPlayer count \t: %d", ((CGameServer *)NET_SERVER::g_NetServer)->m_pAuthContent->m_umapSessions.size());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tLoginPlayer count \t: %d\n", ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent->m_umapSessions.size());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Frame");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAuthContent \t: %d", ((CGameServer *)NET_SERVER::g_NetServer)->m_pAuthContent->m_FPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tEchoContent \t: %d\n", ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent->m_FPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Pool capacity");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession pool capacity \t: %d, usage \t: %d", NET_SERVER::CNetSession::GetPoolCapacity(), NET_SERVER::CNetSession::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tBuffer pool capacity \t: %d, usage \t: %d", CSerializableBuffer<FALSE>::GetPoolCapacity(), CSerializableBuffer<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tView pool capacity \t: %d, usage \t: %d", CSerializableBufferView<FALSE>::GetPoolCapacity(), CSerializableBufferView<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv pool capacity \t: %d, usage \t: %d", CRecvBuffer::GetPoolCapacity(), CRecvBuffer::GetPoolUsage());

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept\t : %d \t Avr : %lf", m_lAcceptTPS, (DOUBLE)m_AcceptTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d \t Avr : %lf", m_lRecvTPS, (DOUBLE)m_RecvTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d \t Avr : %lf", m_lSendTPS, (DOUBLE)m_SendTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUpdate\t : %d \t Avr : %lf", m_lUpdateTPS, (DOUBLE)m_UpdateTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tMaxSend\t : %d", m_lMaxSendCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSendPending\t : %d", m_iSendPendingCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSendCall\t : %d", m_iSendCallCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}

void CMonitor::SendMonitoringServer() noexcept
{
	int currentTime = time(NULL);
	CSerializableBuffer<TRUE> *pGameServerRunBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_SERVER_RUN
			, TRUE, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerRunBuffer);

	CSerializableBuffer<TRUE> *pCpuTotalBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL
			, m_fProcessorTotal, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pCpuTotalBuffer);

	CSerializableBuffer<TRUE> *pNonpagedMemBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY
			, m_SystemNPMemoryVal.largeValue / (1024 * 1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNonpagedMemBuffer);

	CSerializableBuffer<TRUE> *pNetworkRecvBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV
			, m_NetworkRecvKB1Val.largeValue / (1024) + m_NetworkRecvKB2Val.largeValue / (1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNetworkRecvBuffer);

	CSerializableBuffer<TRUE> *pNetworkSendBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND
			, m_NetworkSendKB1Val.largeValue / (1024) + m_NetworkSendKB2Val.largeValue / (1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNetworkSendBuffer);

	CSerializableBuffer<TRUE> *pAvailableMemBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY
			, m_SystemAvailableMemoryVal.largeValue, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pAvailableMemBuffer);

	CSerializableBuffer<TRUE> *pGameServerCpuBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_SERVER_CPU
			, m_fProcessTotal, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerCpuBuffer);

	CSerializableBuffer<TRUE> *pGameServerMemBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_SERVER_MEM
			, m_stPmc.PrivateUsage / (1024 * 1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerMemBuffer);

	CSerializableBuffer<TRUE> *pGameServerSession
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_SESSION
			, NET_SERVER::g_NetServer->GetSessionCount(), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerSession);

	CSerializableBuffer<TRUE> *pGameServerAuthPlayer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER
			, ((CGameServer *)NET_SERVER::g_NetServer)->m_pAuthContent->m_umapSessions.size(), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerAuthPlayer);

	CSerializableBuffer<TRUE> *pGameServerGamePlayer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER
			, ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent->m_umapSessions.size(), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameServerGamePlayer);

	CSerializableBuffer<TRUE> *pGameAcceptTPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS
			, m_lAcceptTPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGameAcceptTPS);

	CSerializableBuffer<TRUE> *pGamePacketRecvTPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS
			, m_lRecvTPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGamePacketRecvTPS);

	CSerializableBuffer<TRUE> *pGamePacketSendTPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS
			, m_lSendTPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGamePacketSendTPS);

	CSerializableBuffer<TRUE> *pGamePacketAuthFPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS
			, ((CGameServer *)NET_SERVER::g_NetServer)->m_pAuthContent->m_FPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGamePacketAuthFPS);

	CSerializableBuffer<TRUE> *pGamePacketGameFPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS
			, ((CGameServer *)NET_SERVER::g_NetServer)->m_pEchoContent->m_FPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pGamePacketGameFPS);

	CSerializableBuffer<TRUE> *pGamePacketPacketPool
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_GAME_PACKET_POOL
			, CSerializableBuffer<FALSE>::GetPoolCapacity(), currentTime);
	g_MonitorClient->SendPacket(m_MonitorClientSessionId, pGamePacketPacketPool);
}
