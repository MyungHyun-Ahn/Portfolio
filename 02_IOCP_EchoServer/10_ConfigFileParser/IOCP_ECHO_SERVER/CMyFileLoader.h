#pragma once
// 싱글 스레드에서만 접근될 것
class CMyFileLoader
{
public:
	// 기본적으로 std::wstring 으로 저장
	VOID Parse(const WCHAR *fileName);

	// 필요하면 더 구현할 것
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, INT *outValue);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::string *str);
	BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::wstring *wstr);

private:
	// 파싱해서 데이터를 가지고 있을 자료구조
	std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
};
