#pragma once

#pragma warning(disable : 26495)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)

#define SINGLE(type)		private:										\
								type();										\
								~type();									\
								inline static type *m_instPtr = nullptr;	\
							public:											\
								static type *GetInstance()					\
								{											\
									if (m_instPtr == nullptr)				\
									{										\
										m_instPtr = new type();				\
										atexit(Destory);					\
									}										\
																			\
									return m_instPtr;						\
								}											\
																			\
								static void Destory()						\
								{											\
									delete m_instPtr;						\
									m_instPtr = nullptr;					\
								}

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


#define MICRO_SECOND 1000000

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

	void Init();
	void Reset();
	void Clear();
};

// RAII 패턴으로 Profile 정보 기록
class CProfile
{
public:
	CProfile(int index, const char *funcName, const char *tagName);
	~CProfile();

	stPROFILE		*m_stProfile;
	LARGE_INTEGER	m_iStartTime;
};


class CProfileManager
{
	SINGLE(CProfileManager)

public:
	friend class CProfile;

	// 언제든지 저장 내역을 출력하고 싶으면
	// 이 함수를 호출한다.
	void DataOutToFile();
	void ResetProfile();
	void Clear();

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
};

extern CProfileManager *g_ProfileMgr;
