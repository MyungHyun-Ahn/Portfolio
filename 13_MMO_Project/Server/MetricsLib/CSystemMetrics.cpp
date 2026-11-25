#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "MetricsSnapshot.h"
#include "CSystemMetrics.h"

#pragma comment(lib, "pdh")
#pragma comment(lib, "psapi")

namespace MetricsLib
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

	CSystemMetrics::CSystemMetrics(HANDLE hProcess) noexcept
		: m_process(hProcess == INVALID_HANDLE_VALUE ? GetCurrentProcess() : hProcess)
		, m_processName(ReadProcessName(m_process))
	{
		// 프로세스 CPU 사용률은 전체 논리 프로세서 용량을 기준으로 계산한다.
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		m_numberOfProcessors = sysInfo.dwNumberOfProcessors;
		if (PdhOpenQuery(nullptr, 0, &m_pdhQuery) == ERROR_SUCCESS)
		{
			m_nonpagedPoolCounter = AddPdhCounter(m_pdhQuery, LR"(\Memory\Pool Nonpaged Bytes)");
			m_availableMemoryCounter = AddPdhCounter(m_pdhQuery, LR"(\Memory\Available MBytes)");
			m_tcpV4RetransmittedSegmentsCounter = AddPdhCounter(m_pdhQuery, LR"(\TCPv4\Segments Retransmitted/sec)");
			m_nicCounter = AddPdhCounter(m_pdhQuery, LR"(\Network Interface(*)\Bytes Total/sec)");
			// 속도 카운터의 첫 원본 표본이다.
			PdhCollectQueryData(m_pdhQuery);
		}
		SMetricsSnapshot baseline;
		collectCpuTime(baseline);
	}

	CSystemMetrics::~CSystemMetrics()
	{
		if (m_pdhQuery != nullptr)
			PdhCloseQuery(m_pdhQuery);
	}

	void CSystemMetrics::Collect(SMetricsSnapshot &outSnapshot)
	{
		const auto nowMilliseconds = GetTickCount64();
		if (!m_processName.empty())
			outSnapshot.m_gauges.push_back({"process_info", 1.0, {{"process", m_processName}}});

		collectCpuTime(outSnapshot);
		collectProcessMemory(outSnapshot);
		collectPdh(m_hasCollected && nowMilliseconds > m_lastSampleMilliseconds, outSnapshot);
		m_lastSampleMilliseconds = nowMilliseconds;
		m_hasCollected = true;
	}

	// 이전 표본과의 차이로 호스트·프로세스 CPU 사용률을 계산한다.
	void CSystemMetrics::collectCpuTime(SMetricsSnapshot &outSnapshot)
	{
		ULARGE_INTEGER idle;
		ULARGE_INTEGER kernel;
		ULARGE_INTEGER user;

		if (GetSystemTimes(reinterpret_cast<FILETIME *>(&idle), reinterpret_cast<FILETIME *>(&kernel),
			reinterpret_cast<FILETIME *>(&user)) == FALSE)
			return;

		ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessorLastKernel.QuadPart;
		ULONGLONG userDiff = user.QuadPart - m_ftProcessorLastUser.QuadPart;
		const ULONGLONG idleDiff = idle.QuadPart - m_ftProcessorLastIdle.QuadPart;
		ULONGLONG total = kernelDiff + userDiff;

		if (m_ftProcessorLastKernel.QuadPart != 0 && total != 0 && kernelDiff >= idleDiff)
		{
			const auto denominator = static_cast<double>(total);
			auto &gauges = outSnapshot.m_gauges;
			gauges.push_back({"host_cpu_usage_ratio", static_cast<double>(total - idleDiff) / denominator, {{"mode", "total"}}});
			gauges.push_back({"host_cpu_usage_ratio", static_cast<double>(userDiff) / denominator, {{"mode", "user"}}});
			gauges.push_back({"host_cpu_usage_ratio", static_cast<double>(kernelDiff - idleDiff) / denominator, {{"mode", "kernel"}}});
		}

		m_ftProcessorLastKernel = kernel;
		m_ftProcessorLastUser = user;
		m_ftProcessorLastIdle = idle;

		FILETIME created{}, exited{};
		ULARGE_INTEGER nowTime;
		// 단조 증가 시계를 프로세스 CPU 시간과 같은 100ns 단위로 맞춘다.
		nowTime.QuadPart = GetTickCount64() * 10000;
		if (!GetProcessTimes(m_process, &created, &exited, reinterpret_cast<FILETIME *>(&kernel), reinterpret_cast<FILETIME *>(&user)))
			return;

		const ULONGLONG timeDiff = nowTime.QuadPart - m_ftProcessLastTime.QuadPart;
		userDiff = user.QuadPart - m_ftProcessLastUser.QuadPart;
		kernelDiff = kernel.QuadPart - m_ftProcessLastKernel.QuadPart;
		total = kernelDiff + userDiff;

		if (m_ftProcessLastTime.QuadPart != 0 && timeDiff != 0 && m_numberOfProcessors > 0)
		{
			const auto capacity = static_cast<double>(timeDiff) * m_numberOfProcessors;
			auto &gauges = outSnapshot.m_gauges;
			gauges.push_back({"process_cpu_usage_ratio", static_cast<double>(total) / capacity, {{"mode", "total"}}});
			gauges.push_back({"process_cpu_usage_ratio", static_cast<double>(userDiff) / capacity, {{"mode", "user"}}});
			gauges.push_back({"process_cpu_usage_ratio", static_cast<double>(kernelDiff) / capacity, {{"mode", "kernel"}}});
		}

		m_ftProcessLastTime = nowTime;
		m_ftProcessLastKernel = kernel;
		m_ftProcessLastUser = user;
	}

	// 프로세스 메모리를 바이트 단위로 추가하고 페이지 폴트 누계를 유지한다.
	void CSystemMetrics::collectProcessMemory(SMetricsSnapshot &outSnapshot)
	{
		PROCESS_MEMORY_COUNTERS_EX memory{};
		memory.cb = sizeof(memory);
		if (!GetProcessMemoryInfo(m_process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), memory.cb))
			return;

		m_pageFaults += static_cast<DWORD>(memory.PageFaultCount - m_lastPageFaults);
		m_lastPageFaults = memory.PageFaultCount;
		outSnapshot.m_counters.push_back({"process_page_faults_total", m_pageFaults, {}});
		auto &gauges = outSnapshot.m_gauges;
		gauges.push_back({"process_working_set_bytes", static_cast<double>(memory.WorkingSetSize), {}});
		gauges.push_back({"process_peak_working_set_bytes", static_cast<double>(memory.PeakWorkingSetSize), {}});
		gauges.push_back({"process_paged_pool_bytes", static_cast<double>(memory.QuotaPagedPoolUsage), {}});
		gauges.push_back({"process_peak_paged_pool_bytes", static_cast<double>(memory.QuotaPeakPagedPoolUsage), {}});
		gauges.push_back({"process_nonpaged_pool_bytes", static_cast<double>(memory.QuotaNonPagedPoolUsage), {}});
		gauges.push_back({"process_peak_nonpaged_pool_bytes", static_cast<double>(memory.QuotaPeakNonPagedPoolUsage), {}});
		gauges.push_back({"process_commit_bytes", static_cast<double>(memory.PagefileUsage), {}});
		gauges.push_back({"process_peak_commit_bytes", static_cast<double>(memory.PeakPagefileUsage), {}});
		gauges.push_back({"process_private_bytes", static_cast<double>(memory.PrivateUsage), {}});
	}

	// 유효한 PDH 표본만 추가하고 가용 메모리는 바이트 단위로 맞춘다.
	void CSystemMetrics::collectPdh(bool hasInterval, SMetricsSnapshot &outSnapshot)
	{
		if (m_pdhQuery == nullptr || PdhCollectQueryData(m_pdhQuery) != ERROR_SUCCESS)
			return;

		auto &gauges = outSnapshot.m_gauges;
		if (const auto nonpagedPoolBytes = ReadPdhValue(m_nonpagedPoolCounter))
			gauges.push_back({"host_nonpaged_pool_bytes", *nonpagedPoolBytes, {}});
		if (const auto availableMemoryMegabytes = ReadPdhValue(m_availableMemoryCounter))
			gauges.push_back({"host_available_memory_bytes", *availableMemoryMegabytes * (1024.0 * 1024.0), {}});
		if (const auto retransmittedSegmentsPerSecond = ReadPdhValue(m_tcpV4RetransmittedSegmentsCounter))
			gauges.push_back({"host_tcpv4_retransmitted_segments_per_second", *retransmittedSegmentsPerSecond, {}});
		if (hasInterval)
			collectNicRates(outSnapshot);
	}

	// 호스트 NIC의 유효한 인터페이스별 송수신 합계를 추가한다.
	void CSystemMetrics::collectNicRates(SMetricsSnapshot &outSnapshot)
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

		for (DWORD index = 0; index < count; ++index)
		{
			const auto &item = m_nicBuffer[index];
			if (item.szName == nullptr || !IsValidPdhValue(item.FmtValue))
				continue;
			auto name = ToUtf8(item.szName);
			if (!name.empty())
				outSnapshot.m_gauges.push_back({"host_nic_total_bytes_per_second", item.FmtValue.doubleValue,
					{{"interface", std::move(name)}}});
		}
	}
}
