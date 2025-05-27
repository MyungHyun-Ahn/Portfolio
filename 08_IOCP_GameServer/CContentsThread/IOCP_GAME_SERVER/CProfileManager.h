#pragma once

#pragma warning(disable : 26495)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)

#ifdef USE_PROFILE

#define PROFILE_BEGIN(num, tagName)											\
				int tlsIdx_##num = (int)TlsGetValue(g_ProfileMgr->m_dwInfosTlsIdx); \
				if (tlsIdx_##num == 0)	\
				{	\
					tlsIdx_##num = g_ProfileMgr->AllocThreadInfo(); \
					TlsSetValue(g_ProfileMgr->m_dwInfosTlsIdx, (LPVOID)tlsIdx_##num); \
				}	\
				stTlsProfileInfo *tlsInfo_##num = g_ProfileMgr->m_arrProfileInfos[tlsIdx_##num]; \
				thread_local static int profileIdx_##num = tlsInfo_##num->currentProfileSize++; \
				CProfile profile_##num(profileIdx_##num, __FUNCSIG__, tagName)
#else

#define PROFILE_BEGIN(num, tagName)	do { } while(0)

#endif

struct stPROFILE
{
	stPROFILE()
	{
		for (int i = 0; i < MINMAX_REMOVE_COUNT; i++)
		{
			iMaxTime[i] = 0;
			iMinTime[i] = INT64_MAX;
		}
		szName[0] = '\0';
	}

	char				szName[300]; // 이름
	__int64				iTotalTime = 0; // 총 수행 시간
	// 최대 최소 값은 신뢰도가 떨어질 수 있으므로 평균치에서 제거할 개수
	static constexpr int MINMAX_REMOVE_COUNT = 2;
	__int64				iMinTime[MINMAX_REMOVE_COUNT]; // 최대 수행 시간
	__int64				iMaxTime[MINMAX_REMOVE_COUNT]; // 최소 수행 시간
	__int64				iCall = 0; // 호출 횟수
	bool				isUsed = false;
};

// 스레드 마다 1개씩
struct stTlsProfileInfo
{
	DWORD threadId = GetCurrentThreadId();
	stPROFILE *arrProfile;
	int currentProfileSize = 0;
	static constexpr int MAX_PROFILE_ARR_SIZE = 100;

	// Data out과 Reset은 사용자 키 입력으로 수행되므로
	// 두 가지가 동기화 문제를 일으킬 확률은 매우 낮음
	LONG resetFlag = FALSE;

	void Init() noexcept
	{
		arrProfile = new(std::nothrow) stPROFILE[MAX_PROFILE_ARR_SIZE];
	}
	void Reset() noexcept
	{
		for (int i = 0; i < currentProfileSize; i++)
		{
			arrProfile[i].iCall = 0;
			arrProfile[i].iTotalTime = 0;

			for (int j = 0; j < stPROFILE::MINMAX_REMOVE_COUNT; j++)
			{
				arrProfile[i].iMinTime[j] = INT64_MAX;
				arrProfile[i].iMaxTime[j] = 0;
			}
		}
	}
	void Clear() noexcept
	{
		delete[] arrProfile;
	}
};

class CProfileManager : public Singleton<CProfileManager>
{
	friend class Singleton<CProfileManager>;
	friend class CProfile;

private:
	CProfileManager()
	{
		QueryPerformanceFrequency(&m_lFreq);

		m_dwInfosTlsIdx = TlsAlloc();
		if (m_dwInfosTlsIdx == TLS_OUT_OF_INDEXES)
		{
			DWORD err = GetLastError();
			throw err;
		}
	}
	~CProfileManager() 
	{
		DataOutToFile();
		Clear();
		TlsFree(m_dwInfosTlsIdx);
	}

public:
	// 언제든지 저장 내역을 출력하고 싶으면
	// 이 함수를 호출한다.
	void DataOutToFile()
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
	void ResetProfile()
	{
		for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
		{
			stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];

			// pInfo의 Reset Flag를 켜줌
			pInfo->resetFlag = TRUE;
		}
	}
	void Clear()
	{
		for (int infoIdx = 1; infoIdx <= m_lCurrentInfosIdx; infoIdx++)
		{
			stTlsProfileInfo *pInfo = m_arrProfileInfos[infoIdx];
			pInfo->Clear();
			delete pInfo;
		}
	}

	// TlsIndex 할당
	inline LONG AllocThreadInfo()
	{
		LONG idx = InterlockedIncrement(&m_lCurrentInfosIdx);
		if (m_iMaxThreadCount + 1 <= idx)
		{
			__debugbreak();
		}
		m_arrProfileInfos[idx] = new stTlsProfileInfo;
		m_arrProfileInfos[idx]->Init();

		return idx;
	}

public:
	DWORD						m_dwInfosTlsIdx; // Tls 인덱스
	static constexpr int		m_iMaxThreadCount = 50; // 최대 스레드 개수
	LONG						m_lCurrentInfosIdx = 0; // 유효 인덱스 - Tls 특성 상 0번은 사용 불가
	stTlsProfileInfo *m_arrProfileInfos[m_iMaxThreadCount]; // Tls Profile 정보
	LARGE_INTEGER m_lFreq;

	LONG m_lResetFlag = FALSE;

	std::map<std::wstring, stPROFILE> m_mapProfiles;
};

inline CProfileManager *g_ProfileMgr = nullptr;

// RAII 패턴으로 Profile 정보 기록
class CProfile
{
public:
	CProfile(int index, const char *funcName, const char *tagName)
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
	~CProfile()
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

	stPROFILE *m_stProfile;
	std::chrono::steady_clock::time_point m_iStartTime;
	// LARGE_INTEGER	m_iStartTime;
};