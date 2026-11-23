#include "ServerNetworkLibPch.h"
#include <pdhmsg.h>
#include <cmath>
#include <string_view>
#include "SNetworkCounters.h"

namespace NetworkLib
{
namespace
{
	constexpr DWORD kPdhFormat = PDH_FMT_DOUBLE | PDH_FMT_NOSCALE | PDH_FMT_NOCAP100;

	bool IsValidPdhValue(const PDH_FMT_COUNTERVALUE &value) noexcept
	{
		return (value.CStatus == PDH_CSTATUS_VALID_DATA || value.CStatus == PDH_CSTATUS_NEW_DATA)
			&& std::isfinite(value.doubleValue) && value.doubleValue >= 0;
	}

	PDH_HCOUNTER AddPdhCounter(PDH_HQUERY query, const WCHAR *pPath) noexcept
	{
		PDH_HCOUNTER counter = nullptr;
		if (PdhAddEnglishCounterW(query, pPath, 0, &counter) != ERROR_SUCCESS)
			return nullptr;
		return counter;
	}

	std::optional<double> ReadPdhValue(PDH_HCOUNTER counter) noexcept
	{
		PDH_FMT_COUNTERVALUE value{};
		if (counter == nullptr || PdhGetFormattedCounterValue(counter, kPdhFormat, nullptr, &value) != ERROR_SUCCESS
			|| !IsValidPdhValue(value))
			return std::nullopt;
		return value.doubleValue;
	}

	std::string ToUtf8(std::wstring_view text)
	{
		if (text.empty())
			return {};
		const int length = static_cast<int>(text.size());
		const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, nullptr, 0, nullptr, nullptr);
		if (bytes <= 0)
			return {};
		std::string result(bytes, '\0');
		if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length, result.data(), bytes, nullptr, nullptr) != bytes)
			return {};
		return result;
	}

	std::string ReadProcessName(HANDLE process)
	{
		constexpr DWORD kMaxPathCharacters = 32768;
		std::wstring path(MAX_PATH, L'\0');
		DWORD length = static_cast<DWORD>(path.size());
		if (!QueryFullProcessImageNameW(process, 0, path.data(), &length))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				return {};
			path.resize(kMaxPathCharacters);
			length = static_cast<DWORD>(path.size());
			if (!QueryFullProcessImageNameW(process, 0, path.data(), &length))
				return {};
		}
		const std::wstring_view fullPath(path.data(), length);
		const auto separator = fullPath.find_last_of(L"\\/");
		return ToUtf8(separator == std::wstring_view::npos ? fullPath : fullPath.substr(separator + 1));
	}
}

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess) noexcept
	: m_process(hProcess == INVALID_HANDLE_VALUE ? GetCurrentProcess() : hProcess)
	, m_processName(ReadProcessName(m_process))
{
	// 프로세서 개수 확인
	//  * 프로세스 실행률 계산시 cpu 개수로 나누어 실제 사용률을 구함
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_numberOfProcessors = sysInfo.dwNumberOfProcessors;
	if (PdhOpenQuery(NULL, NULL, &m_pdhQuery) == ERROR_SUCCESS)
	{
		m_nonpagedPoolCounter = AddPdhCounter(m_pdhQuery, LR"(\Memory\Pool Nonpaged Bytes)");
		m_availableMemoryCounter = AddPdhCounter(m_pdhQuery, LR"(\Memory\Available MBytes)");
		m_tcpV4RetransmittedSegmentsCounter = AddPdhCounter(m_pdhQuery, LR"(\TCPv4\Segments Retransmitted/sec)");
		m_nicCounter = AddPdhCounter(m_pdhQuery, LR"(\Network Interface(*)\Bytes Total/sec)");
		// 속도 카운터의 첫 원본 샘플이다.
		PdhCollectQueryData(m_pdhQuery);
	}
	SMonitorSnapshot baseline;
	updateCpuTime(baseline);
}

CMonitor::~CMonitor()
{
	if (m_pdhQuery != nullptr)
		PdhCloseQuery(m_pdhQuery);
}

void CMonitor::Update(LONG sessions) noexcept
{
	auto spNext = std::make_shared<SMonitorSnapshot>();
	auto &snapshot = *spNext;
	snapshot.m_sessions = sessions;
	snapshot.m_processName = m_processName;
	updateServer(snapshot);
	updateCpuTime(snapshot);
	auto &memory = snapshot.m_memory;
	memory.cb = sizeof(memory);
	snapshot.m_hasMemory = GetProcessMemoryInfo(m_process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), memory.cb) != FALSE;
	if (snapshot.m_hasMemory)
	{
		m_pageFaults += static_cast<DWORD>(memory.PageFaultCount - m_lastPageFaults);
		m_lastPageFaults = memory.PageFaultCount;
		snapshot.m_pageFaults = m_pageFaults;
	}
	updatePdh(snapshot);

	// 완성된 표본만 교체하고 이전 표본은 락 밖에서 해제한다.
	std::shared_ptr<const SMonitorSnapshot> spPublished = std::move(spNext);
	AcquireSRWLockExclusive(&m_snapshotRWLock);
	m_spSnapshot.swap(spPublished);
	ReleaseSRWLockExclusive(&m_snapshotRWLock);
}

std::shared_ptr<const SMonitorSnapshot> CMonitor::GetSnapshot() noexcept
{
	AcquireSRWLockShared(&m_snapshotRWLock);
	auto spSnapshot = m_spSnapshot;
	ReleaseSRWLockShared(&m_snapshotRWLock);
	return spSnapshot;
}

void CMonitor::updateCpuTime(SMonitorSnapshot &outSnapshot) noexcept
{
	// 프로세서 사용률을 갱신
	// FILETIME 구조체를 사용하지만 ULARGE_INTEGER와 구조가 같으므로 이를 사용
	ULARGE_INTEGER idle;
	ULARGE_INTEGER kernel;
	ULARGE_INTEGER user;

	// 시스템 사용 시간을 구함
	// 아이들 타임 / 커널 사용 타임(아이들 포함) / 유저 사용 타임
	if (GetSystemTimes((PFILETIME)&idle, (PFILETIME)&kernel, (PFILETIME)&user) == false)
		return;

	ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessorLastKernel.QuadPart;
	ULONGLONG userDiff = user.QuadPart - m_ftProcessorLastUser.QuadPart;
	ULONGLONG idleDiff = idle.QuadPart - m_ftProcessorLastIdle.QuadPart;

	ULONGLONG total = kernelDiff + userDiff;
	ULONGLONG timeDiff;

	if (m_ftProcessorLastKernel.QuadPart != 0 && total != 0 && kernelDiff >= idleDiff)
	{
		const auto denominator = static_cast<double>(total);
		outSnapshot.m_hostCpu[0] = static_cast<double>(total - idleDiff) / denominator;
		outSnapshot.m_hostCpu[1] = static_cast<double>(userDiff) / denominator;
		outSnapshot.m_hostCpu[2] = static_cast<double>(kernelDiff - idleDiff) / denominator;
		outSnapshot.m_hasHostCpu = true;
	}

	m_ftProcessorLastKernel = kernel;
	m_ftProcessorLastUser = user;
	m_ftProcessorLastIdle = idle;

	// 지정된 프로세스 사용률을 갱신
	FILETIME created{}, exited{};
	ULARGE_INTEGER nowTime;

	// 단조 증가 시계를 CPU 사용 시간과 같은 100ns 단위로 변환한다.
	// 프로세스 사용률 판단 공식
	// a = 샘플 간격의 시스템 시간을 구함 (실제 흐른 시간)
	// b = 프로세스의 CPU 사용 시간을 구함
	// a : 100 = b : 사용률 공식을 통해 사용률을 계산


	// 100 나노세컨드 단위의 시간 구함
	nowTime.QuadPart = GetTickCount64() * 10000;

	// 해당 프로세스가 사용한 시간을 구함
	// 두번째, 세번째는 실행, 종료 시간으로 미사용
	if (!GetProcessTimes(m_process, &created, &exited, (LPFILETIME)&kernel, (LPFILETIME)&user))
		return;

	// 이전에 저장된 프로세스 시간과의 차를 구해 실제로 얼마의 시간이 지났는지 확인
	// 실제 지나온 시간으로 나누면 사용률
	timeDiff = nowTime.QuadPart - m_ftProcessLastTime.QuadPart;
	userDiff = user.QuadPart - m_ftProcessLastUser.QuadPart;
	kernelDiff = kernel.QuadPart - m_ftProcessLastKernel.QuadPart;

	total = kernelDiff + userDiff;

	if (m_ftProcessLastTime.QuadPart != 0 && timeDiff != 0 && m_numberOfProcessors > 0)
	{
		const auto capacity = static_cast<double>(timeDiff) * m_numberOfProcessors;
		outSnapshot.m_processCpu[0] = static_cast<double>(total) / capacity;
		outSnapshot.m_processCpu[1] = static_cast<double>(userDiff) / capacity;
		outSnapshot.m_processCpu[2] = static_cast<double>(kernelDiff) / capacity;
		outSnapshot.m_hasProcessCpu = true;
	}

	m_ftProcessLastTime = nowTime;
	m_ftProcessLastKernel = kernel;
	m_ftProcessLastUser = user;
}

void CMonitor::updateServer(SMonitorSnapshot &outSnapshot) noexcept
{
	const auto nowMilliseconds = GetTickCount64();
	const auto elapsedMilliseconds = nowMilliseconds - m_lastServerMilliseconds;
	outSnapshot.m_hasIntervalRates = m_hasServerSample && elapsedMilliseconds > 0;
	// 교환 연산의 반환값을 사용해 샘플링과 리셋 사이에 증가분을 잃지 않는다.
	LONG *counters[] = {&m_acceptTps, &g_networkCounters.m_recvTps, &g_networkCounters.m_sendTps,
		&m_updateTps, &m_loginReq, &m_sectorMoveReq, &m_chatMsgReq, &m_chatMsgRes,
		&m_delWork, &m_stealWork, &g_networkCounters.m_maxSendCount};
	static_assert(_countof(counters) == SMonitorSnapshot::kIntervalCount);
	for (size_t index = 0; index < _countof(counters); ++index)
	{
		// 첫 표본도 누적값에는 반영하되, 처리율은 두 표본 사이의 구간에서만 계산한다.
		if (!m_hasServerSample || outSnapshot.m_hasIntervalRates)
		{
			const auto count = static_cast<ULONG>(InterlockedExchange(counters[index], 0));
			m_totals[index] += count;
			if (outSnapshot.m_hasIntervalRates)
				outSnapshot.m_rates[index] = 1000.0 * count / static_cast<double>(elapsedMilliseconds);
		}
		outSnapshot.m_totals[index] = m_totals[index];
	}
	m_lastServerMilliseconds = nowMilliseconds;
	m_hasServerSample = true;
	outSnapshot.m_sampledAtMilliseconds = nowMilliseconds;
	outSnapshot.m_accepted = static_cast<UINT64>(InterlockedCompareExchange64(&m_acceptTotal, 0, 0));
	outSnapshot.m_sendCalls = static_cast<UINT64>(InterlockedCompareExchange64(&m_sendCallCount, 0, 0));
	outSnapshot.m_sendPending = static_cast<UINT64>(InterlockedCompareExchange64(&m_sendPendingCount, 0, 0));
}

void CMonitor::updatePdh(SMonitorSnapshot &outSnapshot) noexcept
{
	if (m_pdhQuery == nullptr || PdhCollectQueryData(m_pdhQuery) != ERROR_SUCCESS)
		return;
	outSnapshot.m_nonpagedPoolBytes = ReadPdhValue(m_nonpagedPoolCounter);
	if (const auto availableMemoryMegabytes = ReadPdhValue(m_availableMemoryCounter))
		outSnapshot.m_availableMemoryBytes = *availableMemoryMegabytes * (1024.0 * 1024.0);
	outSnapshot.m_tcpV4RetransmittedSegmentsPerSecond = ReadPdhValue(m_tcpV4RetransmittedSegmentsCounter);
	if (outSnapshot.m_hasIntervalRates)
		updateNicRates(outSnapshot);
}

void CMonitor::updateNicRates(SMonitorSnapshot &outSnapshot) noexcept
{
	if (m_nicCounter == nullptr)
		return;
	DWORD bytes = 0;
	DWORD count = 0;
	if (PdhGetFormattedCounterArrayW(m_nicCounter, kPdhFormat, &bytes, &count, nullptr) != PDH_MORE_DATA || bytes == 0)
		return;
	// PDH 항목과 이름이 들어갈 정렬된 버퍼를 재사용한다.
	m_nicBuffer.resize((static_cast<size_t>(bytes) + sizeof(m_nicBuffer[0]) - 1) / sizeof(m_nicBuffer[0]));
	if (PdhGetFormattedCounterArrayW(m_nicCounter, kPdhFormat, &bytes, &count, m_nicBuffer.data()) != ERROR_SUCCESS)
		return;
	outSnapshot.m_nicRates.reserve(count);

	for (DWORD index = 0; index < count; ++index)
	{
		const auto &item = m_nicBuffer[index];
		if (item.szName == nullptr || !IsValidPdhValue(item.FmtValue))
			continue;
		auto name = ToUtf8(item.szName);
		if (!name.empty())
			outSnapshot.m_nicRates.push_back({std::move(name), item.FmtValue.doubleValue});
	}
}
}
