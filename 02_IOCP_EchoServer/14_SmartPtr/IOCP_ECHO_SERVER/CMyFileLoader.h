#pragma once
// �̱� �����忡���� ���ٵ� ��
class CMyFileLoader
{
public:
	// �⺻������ std::wstring ���� ����
	VOID Parse(const WCHAR *fileName);

	// �ʿ��ϸ� �� ������ ��
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, INT *outValue);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::string *str);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::wstring *wstr);

private:
	// �Ľ��ؼ� �����͸� ������ ���� �ڷᱸ��
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
};
