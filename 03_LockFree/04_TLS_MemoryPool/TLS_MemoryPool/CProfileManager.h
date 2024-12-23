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
				__int64 tlsIdx_##num = (__int64)TlsGetValue(g_ProfileMgr->m_dwInfosTlsIdx); \
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

	char				szName[300]; // �̸�
	__int64				iTotalTime = 0; // �� ���� �ð�
	// �ִ� �ּ� ���� �ŷڵ��� ������ �� �����Ƿ� ���ġ���� ������ ����
	static constexpr int MINMAX_REMOVE_COUNT = 2;
	__int64				iMinTime[MINMAX_REMOVE_COUNT]; // �ִ� ���� �ð�
	__int64				iMaxTime[MINMAX_REMOVE_COUNT]; // �ּ� ���� �ð�
	__int64				iCall = 0; // ȣ�� Ƚ��
	bool				isUsed = false;
};

// ������ ���� 1����
struct stTlsProfileInfo
{
	DWORD threadId = GetCurrentThreadId();
	stPROFILE *arrProfile;
	int currentProfileSize = 0;
	static constexpr int MAX_PROFILE_ARR_SIZE = 100;

	// Data out�� Reset�� ����� Ű �Է����� ����ǹǷ�
	// �� ������ ����ȭ ������ ����ų Ȯ���� �ſ� ����
	LONG resetFlag = FALSE;

	void Init() noexcept;
	void Reset() noexcept;
	void Clear() noexcept;
};

// RAII �������� Profile ���� ���
class CProfile
{
public:
	CProfile(int index, const char *funcName, const char *tagName) noexcept;
	~CProfile() noexcept;

	stPROFILE		*m_stProfile;
	LARGE_INTEGER	m_iStartTime;
};


class CProfileManager
{
	SINGLE(CProfileManager)

public:
	friend class CProfile;

	// �������� ���� ������ ����ϰ� ������
	// �� �Լ��� ȣ���Ѵ�.
	void DataOutToFile(); 
	void ResetProfile() noexcept;
	void Clear() noexcept;

	// TlsIndex �Ҵ�
	inline LONG AllocThreadInfo() noexcept
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
	DWORD						m_dwInfosTlsIdx; // Tls �ε���
	static constexpr int		m_iMaxThreadCount = 50; // �ִ� ������ ����
	LONG						m_lCurrentInfosIdx = 0; // ��ȿ �ε��� - Tls Ư�� �� 0���� ��� �Ұ�
	stTlsProfileInfo *m_arrProfileInfos[m_iMaxThreadCount]; // Tls Profile ����
	LARGE_INTEGER m_lFreq;

	LONG m_lResetFlag = FALSE;
};

extern CProfileManager *g_ProfileMgr;
