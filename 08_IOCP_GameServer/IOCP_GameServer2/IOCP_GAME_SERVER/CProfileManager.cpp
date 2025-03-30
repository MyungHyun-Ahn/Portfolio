#include "pch.h"
#include "CProfileManager.h"

CProfileManager *g_ProfileMgr;

CProfileManager::CProfileManager()
{
	QueryPerformanceFrequency(&m_lFreq);

	m_dwInfosTlsIdx = TlsAlloc();
	if (m_dwInfosTlsIdx == TLS_OUT_OF_INDEXES)
	{
		DWORD err = GetLastError();
		throw err;
	}
}

CProfileManager::~CProfileManager()
{
	DataOutToFile();
	Clear();
	TlsFree(m_dwInfosTlsIdx);
}

void CProfileManager::DataOutToFile()
{
	CHAR fileName[MAX_PATH];

	__time64_t time64 = _time64(nullptr);
	tm nowTm;
	errno_t err1 = localtime_s(&nowTm, &time64);
	if (err1 != 0)
	{
		throw;
	}

	CreateDirectory(L"PROFILE", NULL);

	sprintf_s(fileName, "PROFILE\\Profile_%04d%02d%02d_%02d%02d%02d.profile"
		, nowTm.tm_year + 1900
		, nowTm.tm_mon
		, nowTm.tm_mday
		, nowTm.tm_hour
		, nowTm.tm_min
		, nowTm.tm_sec);


	FILE *fp;
	errno_t err2 = fopen_s(&fp, fileName, "w");
	if (err2 != 0 || fp == nullptr)
		throw err2;

	for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
	{
		fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
		fprintf_s(fp, "%-15s | %-100s | %-15s | %-15s | %-15s | %-15s | %-15s |\n", "ThreadId", "Name", "TotalTime(ns)", "Average(ns)", "Min(ns)", "Max(ns)", "Call");
		fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

		stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

		for (int profileIdx = 0; profileIdx < pInfo->currentProfileSize; profileIdx++)
		{
			stPROFILE *pProfile = &pInfo->arrProfile[profileIdx];
			if (pProfile->iCall < 4)
				continue;

			__int64 totalTime = pProfile->iTotalTime;
			totalTime -= pProfile->iMaxTime[0];
			totalTime -= pProfile->iMaxTime[1];
			totalTime -= pProfile->iMinTime[0];
			totalTime -= pProfile->iMinTime[1];

			if (pProfile->iCall - 4 > 0)
			{
				__int64 average = totalTime / (pProfile->iCall - 4);

				fprintf_s(fp, "%-15d | %-100s | %-15lld | %-15lld | %-15lld | %-15lld | %-15lld |\n"
					, pInfo->threadId
					, pProfile->szName
					, totalTime
					, average
					, pProfile->iMinTime[0]
					, pProfile->iMaxTime[0]
					, pProfile->iCall - 4);
			}
		}

		fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
	}

	fclose(fp);
}

void CProfileManager::ResetProfile()
{
	for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
	{
		stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

		// pInfo의 Reset Flag를 켜줌
		pInfo->resetFlag = TRUE;
	}
}

void CProfileManager::Clear()
{
	for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
	{
		stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];
		pInfo->Clear();
		delete pInfo;
	}
}

CProfile::CProfile(int index, const char *funcName, const char *tagName)
{
	UINT64 idx = (UINT64)TlsGetValue(g_ProfileMgr->m_dwInfosTlsIdx);

	stPROFILE *pProfile = &g_ProfileMgr->m_arrProfileInfos[idx]->arrProfile[index];
	if (pProfile->isUsed == false)
	{
		strcat_s(pProfile->szName, 300, funcName);
		if (tagName != nullptr)
		{
			strcat_s(pProfile->szName, 300 - strlen(funcName), " ");
			strcat_s(pProfile->szName, 300 - strlen(funcName) - strlen(" "), tagName);
		}

		pProfile->isUsed = true;
	}

	// 만약 reset Flag 가 켜져있다면
	// Reset을 수행하고 다시 측정
	// 어차피 기록과 리셋이 같은 스레드에서 수행되므로
	// Interlocked 계열의 동기화가 필요없음
	// 그리고 대략적인 리셋 시점으로부터의 테스트 결과가 필요하므로 정확한 타이밍에 리셋이 필요한 것도 아님
	if (g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag)
	{
		g_ProfileMgr->m_arrProfileInfos[idx]->Reset();
		g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag = FALSE;
	}

	m_stProfile = pProfile;
	m_iStartTime = std::chrono::high_resolution_clock::now();

	// std::chrono::steady_clock::time_point
	// QueryPerformanceCounter(&m_iStartTime);
}

CProfile::~CProfile()
{
	// LARGE_INTEGER endTime;
	// QueryPerformanceCounter(&endTime);

	std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
	// __int64 runtime = endTime.QuadPart - m_iStartTime.QuadPart;
	std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_iStartTime);
	__int64 runtime = ns.count();
	m_stProfile->iCall++;
	m_stProfile->iTotalTime += runtime;

	// MAX 갱신
	if (m_stProfile->iMaxTime[1] < runtime)
	{
		m_stProfile->iMaxTime[1] = runtime;
		if (m_stProfile->iMaxTime[0] < m_stProfile->iMaxTime[1])
		{
			__int64 temp = m_stProfile->iMaxTime[0];
			m_stProfile->iMaxTime[0] = m_stProfile->iMaxTime[1];
			m_stProfile->iMaxTime[1] = temp;
		}
	}

	// MIN 갱신
	if (m_stProfile->iMinTime[1] > runtime)
	{
		m_stProfile->iMinTime[1] = runtime;
		if (m_stProfile->iMinTime[0] > m_stProfile->iMinTime[1])
		{
			__int64 temp = m_stProfile->iMinTime[0];
			m_stProfile->iMinTime[0] = m_stProfile->iMinTime[1];
			m_stProfile->iMinTime[1] = temp;
		}
	}
}

void stTlsProfileInfo::Init()
{
	arrProfile = new stPROFILE[MAX_PROFILE_ARR_SIZE];
}

void stTlsProfileInfo::Reset()
{
	for (int i = 0; i < currentProfileSize; i++)
	{
		arrProfile->iCall = 0;
		arrProfile->iTotalTime = 0;

		for (int i = 0; i < stPROFILE::MINMAX_REMOVE_COUNT; i++)
		{
			arrProfile->iMinTime[i] = INT64_MAX;
			arrProfile->iMaxTime[i] = 0;
		}
	}
}

void stTlsProfileInfo::Clear()
{
	delete[] arrProfile;
}