#pragma once
class CEncodingConvertor
{
public:
	// 두 번째 인자에 -1을 전달하면 알아서 널 문자 찾아서
	inline static bool Utf16ToUtf8(const WCHAR *utf16String, const int utf16StringLen, char *utf8String, const int utf8StringLen)
	{
		int result = WideCharToMultiByte(CP_UTF8, 0, utf16String, utf16StringLen, utf8String, utf8StringLen, nullptr, nullptr);
		return result > 0;
	}

	inline static bool Utf8ToUtf16(const char *utf8String, const int utf8StringLen, WCHAR *utf16String, const int utf16StringLen)
	{
		int result = MultiByteToWideChar(CP_UTF8, 0, utf8String, utf8StringLen, utf16String, utf16StringLen);
		return result > 0;
	}
};