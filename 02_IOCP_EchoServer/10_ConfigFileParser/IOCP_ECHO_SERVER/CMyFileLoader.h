#pragma once
// �̱� �����忡���� ���ٵ� ��
class CMyFileLoader
{
public:
	// �⺻������ std::wstring ���� ����
	VOID Parse(const WCHAR *fileName);

	// �ʿ��ϸ� �� ������ ��
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue)
	{
		auto classIt = m_parsedDatas.find(classStr);
		if (classIt == m_parsedDatas.end())
		{
			// class ����
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value ����
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
			// class ����
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value ����
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
			// class ����
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value ����
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
			// class ����
			return FALSE;
		}

		auto keyIt = classIt->second.find(keyStr);
		if (keyIt == classIt->second.end())
		{
			// value ����
			return FALSE;
		}
		wstr->assign(keyIt->second.begin(), keyIt->second.end());
		return TRUE;
	}

private:
	// �Ľ��ؼ� �����͸� ������ ���� �ڷᱸ��
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
};
