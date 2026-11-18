#include "pch.h"
#include <conio.h>
#include "CommonProtocol.h"
#include "Console.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	std::string WideToUtf8(const std::wstring &text)
	{
		if (text.empty())
			return {};
		int bytes = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string result(bytes, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), bytes, nullptr, nullptr);
		return result;
	}

	bool Utf8ToWide(const std::string &text, std::wstring &outText)
	{
		int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
		if (length <= 0)
			return false;
		outText.resize(length);
		return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), outText.data(), length) != 0;
	}

	EInputResult PollInput(std::string &pending, WCHAR &highSurrogate, std::string &outLine)
	{
		HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
		DWORD consoleMode = 0;
		if (GetConsoleMode(input, &consoleMode))
		{
			while (_kbhit())
			{
				WCHAR character = static_cast<WCHAR>(_getwch());
				if (character == 0 || character == 0xe0)
				{
					_getwch();
					continue;
				}
				if (character == 26 || character == 4)
				{
					highSurrogate = 0;
					return EInputResult::End;
				}
				if (character == L'\r')
				{
					if (highSurrogate != 0)
					{
						g_pLogger->WriteLogConsole(ELogLevel::Error, L"INPUT ERROR: incomplete UTF-16 surrogate pair");
						highSurrogate = 0;
						return EInputResult::End;
					}
					fputc('\n', stdout);
					outLine.swap(pending);
					pending.clear();
					return EInputResult::Line;
				}
				if (character == L'\b')
				{
					if (highSurrogate != 0)
						highSurrogate = 0;
					else if (!pending.empty())
					{
						size_t offset = pending.size() - 1;
						while (offset > 0 && (pending[offset] & 0xc0) == 0x80)
							--offset;
						pending.resize(offset);
						fputs("\b \b", stdout);
					}
				}
				else
				{
					bool isLowSurrogate = character >= 0xdc00 && character <= 0xdfff;
					if ((highSurrogate != 0) != isLowSurrogate)
					{
						g_pLogger->WriteLogConsole(ELogLevel::Error, L"INPUT ERROR: invalid UTF-16 surrogate pair");
						highSurrogate = 0;
						return EInputResult::End;
					}
					// 상위 대리는 다음 poll까지 보관하고 쌍을 함께 변환한다.
					if (character >= 0xd800 && character <= 0xdbff)
					{
						highSurrogate = character;
						continue;
					}
					std::wstring text;
					if (highSurrogate != 0)
						text += highSurrogate;
					text += character;
					highSurrogate = 0;
					std::string encoded = WideToUtf8(text);
					pending += encoded;
					fputs(encoded.c_str(), stdout);
				}
				fflush(stdout);
			}
			return EInputResult::Waiting;
		}

		DWORD fileType = GetFileType(input);
		for (;;)
		{
			if (fileType == FILE_TYPE_PIPE)
			{
				DWORD available = 0;
				if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr))
				{
					if (pending.empty())
						return EInputResult::End;
					outLine.swap(pending);
					pending.clear();
					return EInputResult::Line;
				}
				if (available == 0)
					return EInputResult::Waiting;
			}
			char character;
			DWORD bytes;
			if (!ReadFile(input, &character, 1, &bytes, nullptr) || bytes == 0)
			{
				if (pending.empty())
					return EInputResult::End;
				outLine.swap(pending);
				pending.clear();
				return EInputResult::Line;
			}
			if (character == '\n')
			{
				if (!pending.empty() && pending.back() == '\r')
					pending.pop_back();
				outLine.swap(pending);
				pending.clear();
				return EInputResult::Line;
			}
			pending += character;
		}
	}
}
