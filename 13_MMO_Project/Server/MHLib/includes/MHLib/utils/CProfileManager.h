#pragma once

namespace MHLib::utils
{

#pragma warning(disable : 26495)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)

#define PROFILE_BEGIN(num, tagName)											\
				ULONG_PTR tlsIdx_##num = reinterpret_cast<ULONG_PTR>(TlsGetValue(g_pProfileManager->m_infosTlsIdx)); \
				if (tlsIdx_##num == 0)	\
				{	\
					tlsIdx_##num = g_pProfileManager->AllocThreadInfo(); \
					TlsSetValue(g_pProfileManager->m_infosTlsIdx, reinterpret_cast<LPVOID>(tlsIdx_##num)); \
				}	\
				STlsProfileInfo *pTlsInfo_##num = g_pProfileManager->m_pProfileInfos[tlsIdx_##num]; \
				thread_local static int profileIdx_##num = pTlsInfo_##num->m_currentProfileSize++; \
				CProfile profile_##num(profileIdx_##num, __FUNCSIG__, tagName)

	struct SProfile
	{
		SProfile()
		{
			for (int i = 0; i < kMinmaxRemoveCount; i++)
			{
				m_maxTime[i] = 0;
				m_minTime[i] = INT64_MAX;
			}
			m_name[0] = '\0';
		}

		char				m_name[300]; // 이름
		__int64				m_totalTime = 0; // 총 수행 시간
		// 최대 최소 값은 신뢰도가 떨어질 수 있으므로 평균치에서 제거할 개수
		static constexpr int kMinmaxRemoveCount = 2;
		__int64				m_minTime[kMinmaxRemoveCount]; // 최대 수행 시간
		__int64				m_maxTime[kMinmaxRemoveCount]; // 최소 수행 시간
		__int64				m_callCount = 0; // 호출 횟수
		bool				m_isUsed = false;
	};

	// 스레드 마다 1개씩
	struct STlsProfileInfo
	{
		DWORD m_threadId = GetCurrentThreadId();
		SProfile *m_pProfiles;
		int m_currentProfileSize = 0;
		static constexpr int kMaxProfileArrSize = 100;

		// Data out과 Reset은 사용자 키 입력으로 수행되므로
		// 두 가지가 동기화 문제를 일으킬 확률은 매우 낮음
		LONG m_resetFlag = FALSE;

		void Init() noexcept
		{
			m_pProfiles = new(std::nothrow) SProfile[kMaxProfileArrSize];
		}
		void Reset() noexcept
		{
			for (int i = 0; i < m_currentProfileSize; i++)
			{
				m_pProfiles[i].m_callCount = 0;
				m_pProfiles[i].m_totalTime = 0;

				for (int j = 0; j < SProfile::kMinmaxRemoveCount; j++)
				{
					m_pProfiles[i].m_minTime[j] = INT64_MAX;
					m_pProfiles[i].m_maxTime[j] = 0;
				}
			}
		}
		void Clear() noexcept
		{
			delete[] m_pProfiles;
		}
	};

	class CProfileManager : public CSingleton<CProfileManager>
	{
		friend class CSingleton<CProfileManager>;
		friend class CProfile;

	private:
		CProfileManager()
		{
			QueryPerformanceFrequency(&m_freq);

			m_infosTlsIdx = TlsAlloc();
			if (m_infosTlsIdx == TLS_OUT_OF_INDEXES)
			{
				DWORD err = GetLastError();
				throw err;
			}
		}
		~CProfileManager()
		{
			DataOutToFile();
			Clear();
			TlsFree(m_infosTlsIdx);
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


			FILE *pFp;
			errno_t err2 = fopen_s(&pFp, fileName, "w");
			if (err2 != 0 || pFp == nullptr)
				throw err2;

			for (int infoIdx = 1; infoIdx <= m_currentInfosIdx; infoIdx++)
			{
				fprintf_s(pFp,
					"---------------------------------------------------------------------------------------------------------"
					"---------------------------------------------------------------------------------------------------------\n");
				fprintf_s(pFp, "%-15s | %-100s | %-15s | %-15s | %-15s | %-15s | %-15s |\n",
					"ThreadId", "Name", "TotalTime(ns)", "Average(ns)", "Min(ns)", "Max(ns)", "Call");
				fprintf_s(pFp,
					"---------------------------------------------------------------------------------------------------------"
					"---------------------------------------------------------------------------------------------------------\n");

				STlsProfileInfo *pInfo = m_pProfileInfos[infoIdx];

				for (int profileIdx = 0; profileIdx < pInfo->m_currentProfileSize; profileIdx++)
				{
					SProfile *pProfile = &pInfo->m_pProfiles[profileIdx];
					if (pProfile->m_callCount < 4)
						continue;

					__int64 totalTime = pProfile->m_totalTime;
					totalTime -= pProfile->m_maxTime[0];
					totalTime -= pProfile->m_maxTime[1];
					totalTime -= pProfile->m_minTime[0];
					totalTime -= pProfile->m_minTime[1];

					if (pProfile->m_callCount - 4 > 0)
					{
						__int64 average = totalTime / (pProfile->m_callCount - 4);

						fprintf_s(pFp, "%-15d | %-100s | %-15lld | %-15lld | %-15lld | %-15lld | %-15lld |\n"
							, pInfo->m_threadId
							, pProfile->m_name
							, totalTime
							, average
							, pProfile->m_minTime[0]
							, pProfile->m_maxTime[0]
							, pProfile->m_callCount - 4);
					}
				}

				fprintf_s(pFp,
					"---------------------------------------------------------------------------------------------------------"
					"---------------------------------------------------------------------------------------------------------\n");
			}

			fclose(pFp);
		}
		void ResetProfile()
		{
			for (int infoIdx = 1; infoIdx <= m_currentInfosIdx; infoIdx++)
			{
				STlsProfileInfo *pInfo = m_pProfileInfos[infoIdx];

				// pInfo의 Reset Flag를 켜줌
				pInfo->m_resetFlag = TRUE;
			}
		}
		void Clear()
		{
			for (int infoIdx = 1; infoIdx <= m_currentInfosIdx; infoIdx++)
			{
				STlsProfileInfo *pInfo = m_pProfileInfos[infoIdx];
				pInfo->Clear();
				delete pInfo;
			}
		}

		// TlsIndex 할당
		inline LONG AllocThreadInfo()
		{
			LONG idx = InterlockedIncrement(&m_currentInfosIdx);
			if (kMaxThreadCount <= idx)
			{
				__debugbreak();
			}
			m_pProfileInfos[idx] = new STlsProfileInfo;
			m_pProfileInfos[idx]->Init();

			return idx;
		}

	public:
		DWORD						m_infosTlsIdx; // Tls 인덱스
		static constexpr int		kMaxThreadCount = 50; // 최대 스레드 개수
		LONG						m_currentInfosIdx = 0; // 유효 인덱스 - Tls 특성 상 0번은 사용 불가
		STlsProfileInfo *m_pProfileInfos[kMaxThreadCount]; // Tls Profile 정보
		LARGE_INTEGER m_freq;

		LONG m_resetFlag = FALSE;

		std::map<std::wstring, SProfile> m_mapProfiles;
	};

	inline CProfileManager *g_pProfileManager = nullptr;

	// RAII 패턴으로 Profile 정보 기록
	class CProfile
	{
	public:
		CProfile(int index, const char *pFuncName, const char *pTagName)
		{
			UINT64 idx = (UINT64)TlsGetValue(g_pProfileManager->m_infosTlsIdx);

			SProfile *pProfile = &g_pProfileManager->m_pProfileInfos[idx]->m_pProfiles[index];
			if (pProfile->m_isUsed == false)
			{
				strcat_s(pProfile->m_name, 300, pFuncName);
				if (pTagName != nullptr)
				{
					strcat_s(pProfile->m_name, 300 - strlen(pFuncName), " ");
					strcat_s(pProfile->m_name, 300 - strlen(pFuncName) - strlen(" "), pTagName);
				}

				pProfile->m_isUsed = true;
			}

			// 만약 reset Flag 가 켜져있다면
			// Reset을 수행하고 다시 측정
			// 어차피 기록과 리셋이 같은 스레드에서 수행되므로
			// Interlocked 계열의 동기화가 필요없음
			// 그리고 대략적인 리셋 시점으로부터의 테스트 결과가 필요하므로 정확한 타이밍에 리셋이 필요한 것도 아님
			if (g_pProfileManager->m_pProfileInfos[idx]->m_resetFlag)
			{
				g_pProfileManager->m_pProfileInfos[idx]->Reset();
				g_pProfileManager->m_pProfileInfos[idx]->m_resetFlag = FALSE;
			}

			m_pProfile = pProfile;
			m_startTime = std::chrono::high_resolution_clock::now();

			// std::chrono::steady_clock::time_point
			// QueryPerformanceCounter(&m_iStartTime);
		}
		~CProfile()
		{
			// LARGE_INTEGER endTime;
			// QueryPerformanceCounter(&endTime);

			std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
			// __int64 runtime = endTime.QuadPart - m_iStartTime.QuadPart;
			std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - m_startTime);
			__int64 runtime = ns.count();
			m_pProfile->m_callCount++;
			m_pProfile->m_totalTime += runtime;

			// MAX 갱신
			if (m_pProfile->m_maxTime[1] < runtime)
			{
				m_pProfile->m_maxTime[1] = runtime;
				if (m_pProfile->m_maxTime[0] < m_pProfile->m_maxTime[1])
				{
					__int64 temp = m_pProfile->m_maxTime[0];
					m_pProfile->m_maxTime[0] = m_pProfile->m_maxTime[1];
					m_pProfile->m_maxTime[1] = temp;
				}
			}

			// MIN 갱신
			if (m_pProfile->m_minTime[1] > runtime)
			{
				m_pProfile->m_minTime[1] = runtime;
				if (m_pProfile->m_minTime[0] > m_pProfile->m_minTime[1])
				{
					__int64 temp = m_pProfile->m_minTime[0];
					m_pProfile->m_minTime[0] = m_pProfile->m_minTime[1];
					m_pProfile->m_minTime[1] = temp;
				}
			}
		}

		SProfile *m_pProfile;
		std::chrono::steady_clock::time_point m_startTime;
		// LARGE_INTEGER	m_iStartTime;
	};
}