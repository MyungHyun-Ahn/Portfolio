#pragma once
// 싱글 스레드에서만 접근될 것
class CMyFileLoader
{
public:
	// 기본적으로 std::wstring 으로 저장
	VOID Parse(const WCHAR *fileName);

	// 필요하면 더 구현할 것
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue)
	{
		auto classIt = m_parsedDatas.find(classStr);
		if (classIt == m_parsedDatas.end())
		{
			// class 없음
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value 없음
			return FALSE;
		}

		INT out = _wtoi(keyIt->second.c_str());
		*outValue = (USHORT)out;
		return TRUE;
	}

	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, INT *outValue)
	{
		auto classIt = m_parsedDatas.find(classStr);
		if (classIt == m_parsedDatas.end())
		{
			// class 없음
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value 없음
			return FALSE;
		}

		INT out = _wtoi(keyIt->second.c_str());
		*outValue = out;
		return TRUE;
	}

	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::string *str)
	{
		auto classIt = m_parsedDatas.find(classStr);
		if (classIt == m_parsedDatas.end())
		{
			// class 없음
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value 없음
			return FALSE;
		}

		str->assign(keyIt->second.begin(), keyIt->second.end());
		return TRUE;
	}

	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::wstring *wstr)
	{
		auto classIt = m_parsedDatas.find(classStr);
		if (classIt == m_parsedDatas.end())
		{
			// class 없음
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value 없음
			return FALSE;
		}
		wstr->assign(keyIt->second.begin(), keyIt->second.end());
		return TRUE;
	}

private:
	// 파싱해서 데이터를 가지고 있을 자료구조
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
};
