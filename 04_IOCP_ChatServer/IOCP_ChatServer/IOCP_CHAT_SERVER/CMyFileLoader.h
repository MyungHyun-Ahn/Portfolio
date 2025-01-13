#pragma once
// �̱� �����忡���� ���ٵ� ��
class CMyFileLoader
{
public:
	// �⺻������ std::wstring ���� ����
	VOID Parse(const WCHAR *fileName) noexcept;

	// �ʿ��ϸ� �� ������ ��
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue) noexcept;
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, INT *outValue) noexcept;
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, unsigned int *outValue) noexcept;
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, BYTE *outValue) noexcept;
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::string *str) noexcept;
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::wstring *wstr) noexcept;

private:
	// �Ľ��ؼ� �����͸� ������ ���� �ڷᱸ��
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
};
