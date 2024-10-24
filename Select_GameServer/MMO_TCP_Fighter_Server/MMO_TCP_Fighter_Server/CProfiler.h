#pragma once
#define __WFUNC__ _T(__FUNCTION__)

#define PROFILE_BEGIN(funcName, num)											\
				CProfile profile_##num(funcName, L#num)


#define MICRO_SECOND 1000000

struct stPROFILE
{
	__int64				m_iTotalTime = 0;
	__int64				m_iMinTime[2] = { INT64_MAX, INT64_MAX };
	__int64				m_iMaxTime[2] = { 0, 0 };
	__int64				m_iCall = 0;
};

class CProfile
{
public:
	CProfile(const std::wstring &tagName);
	CProfile(const std::wstring &tagName, const std::wstring &index);
	~CProfile();

	const std::wstring m_wsTagName;
	LARGE_INTEGER m_iStartTime;
};


class CProfileManager
{
	SINGLE(CProfileManager)

public:
	void RegisterProfile(const std::wstring &tagName);
	void UpdateProfile(const std::wstring &tagName, __int64 &runtime);

	// 언제든지 저장 내역을 출력하고 싶으면
	// 이 함수를 호출한다.
	void DataOutToFile();
	void ResetProfile();

private:
	LARGE_INTEGER m_lFreq;
	std::map<std::wstring, stPROFILE> m_mapProfiles;
};

extern CProfileManager *g_Profiler;
