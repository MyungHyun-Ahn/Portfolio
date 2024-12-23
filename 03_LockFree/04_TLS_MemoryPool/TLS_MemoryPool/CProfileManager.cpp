#include <new>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <time.h>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CTLSSharedMemoryPool.h"
#include "CTLSMemoryPool.h"
#include "CProfileManager.h"
#include "CLFQueue.h"

CProfileManager *g_ProfileMgr;

CProfileManager::CProfileManager()
{
	QueryPerformanceFrequency(&m_lFreq);

	m_dwInfosTlsIdx = TlsAlloc();
	if (m_dwInfosTlsIdx == TLS_OUT_OF_INDEXES)
	{
		DWORD err = GetLastError();
		__debugbreak();
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

		fprintf_s(fp, "------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
	}

	fclose(fp);
}

void CProfileManager::ResetProfile() noexcept
{
	for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
	{
		stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

		// pInfo�� Reset Flag�� ����
		pInfo->resetFlag = TRUE;
	}
}

void CProfileManager::Clear() noexcept
{
	for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
	{
		stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];
		pInfo->Clear();
		delete pInfo;
	}
}

CProfile::CProfile(int index, const char *funcName, const char *tagName) noexcept
{
	__int64 idx = (__int64)TlsGetValue(g_ProfileMgr->m_dwInfosTlsIdx);

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

	// ���� reset Flag �� �����ִٸ�
	// Reset�� �����ϰ� �ٽ� ����
	// ������ ��ϰ� ������ ���� �����忡�� ����ǹǷ�
	// Interlocked �迭�� ����ȭ�� �ʿ����
	// �׸��� �뷫���� ���� �������κ����� �׽�Ʈ ����� �ʿ��ϹǷ� ��Ȯ�� Ÿ�ֿ̹� ������ �ʿ��� �͵� �ƴ�
	if (g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag)
	{
		g_ProfileMgr->m_arrProfileInfos[idx]->Reset();
		g_ProfileMgr->m_arrProfileInfos[idx]->resetFlag = FALSE;
	}

	m_stProfile = pProfile;

	QueryPerformanceCounter(&m_iStartTime);
}

CProfile::~CProfile() noexcept
{
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);

	__int64 runtime = endTime.QuadPart - m_iStartTime.QuadPart;

	m_stProfile->iCall++;
	m_stProfile->iTotalTime += runtime;

	// MAX ����
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

	// MIN ����
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

void stTlsProfileInfo::Init() noexcept
{
	arrProfile = new stPROFILE[MAX_PROFILE_ARR_SIZE];
}

void stTlsProfileInfo::Reset() noexcept
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

void stTlsProfileInfo::Clear() noexcept
{
	delete[] arrProfile;
}
