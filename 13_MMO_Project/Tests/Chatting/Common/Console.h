#pragma once

namespace Chatting
{
	enum class EInputResult
	{
		Waiting,
		Line,
		End
	};

	std::string WideToUtf8(const std::wstring &text);
	bool Utf8ToWide(const std::string &text, std::wstring &outText);
	EInputResult PollInput(std::string &pending, WCHAR &highSurrogate, std::string &outLine);
}
