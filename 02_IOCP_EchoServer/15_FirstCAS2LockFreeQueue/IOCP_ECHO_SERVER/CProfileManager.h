#pragma once

#pragma warning(disable : 26495)
#pragma warning(disable : 6386)
#pragma warning(disable : 6385)

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

	void Init();
	void Reset();
	void Clear();
};

// RAII �������� Profile ���� ���
class CProfile
{
public:
	CProfile(int index, const char *funcName, const char *tagName);
	~CProfile();

	stPROFILE *m_stProfile;
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
	void ResetProfile();
	void Clear();

	// TlsIndex �Ҵ�
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
	DWORD						m_dwInfosTlsIdx; // Tls �ε���
	static constexpr int		m_iMaxThreadCount = 50; // �ִ� ������ ����
	LONG						m_lCurrentInfosIdx = 0; // ��ȿ �ε��� - Tls Ư�� �� 0���� ��� �Ұ�
	stTlsProfileInfo *m_arrProfileInfos[m_iMaxThreadCount]; // Tls Profile ����
	LARGE_INTEGER m_lFreq;

	LONG m_lResetFlag = FALSE;

	std::map<std::wstring, stPROFILE> m_mapProfiles;
};

extern CProfileManager *g_ProfileMgr;