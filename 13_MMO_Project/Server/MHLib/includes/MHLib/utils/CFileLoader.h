#pragma once

#pragma warning(disable : 4244) // xutility

namespace MHLib::utils
{
	// 싱글 스레드에서만 접근될 것
	class CFileLoader
	{
	public:
		// 파일을 읽어 키를 등록한다. 읽지 못했거나 등록할 키가 없으면 실패한다.
		[[nodiscard]]
		bool Parse(const WCHAR *pFileName) noexcept
		{
			FILE *pFile = _wfsopen(pFileName, L"r, ccs = UTF-16LE", _SH_DENYWR);
			if (pFile == nullptr)
				return false;

			size_t fileBytes;
			fseek(pFile, 0, SEEK_END);
			fileBytes = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);

			WCHAR *pFileBuffer = new WCHAR[fileBytes / sizeof(WCHAR) + 1/*마지막 널문자 자리 확보*/];
			const size_t retVal = fread_s(pFileBuffer, fileBytes, sizeof(char), fileBytes, pFile);
			const bool isRead = retVal != 0 && ferror(pFile) == 0;
			fclose(pFile);
			if (!isRead)
			{
				delete[] pFileBuffer;
				return false;
			}

			pFileBuffer[retVal / sizeof(WCHAR)] = L'\0';

			std::wstring wStr(pFileBuffer);

			// wstring 으로 만들었다면 파일 버퍼 삭제
			delete[] pFileBuffer;

			// \t\r 띄워쓰기 제거
			wStr.erase(std::remove(wStr.begin(), wStr.end(), L'\t'), wStr.end());
			wStr.erase(std::remove(wStr.begin(), wStr.end(), L' '), wStr.end());

			// 주석 삭제 "//"
			{
				size_t xPos = wStr.find(L"//");
				while (xPos != std::wstring::npos)
				{
					// nPos 부터
					size_t yPos;
					for (yPos = xPos; yPos < wStr.size(); yPos++)
					{
						if (wStr[yPos] == L'\n')
							break;
					}

					// \n 문자 1개 추가 + 1
					wStr.erase(xPos, yPos - xPos + 1);

					// 여기도 지웠으니 ok
					xPos = wStr.find(L"//");
				}
			}
			// 한줄 주석을 지웠으면 \n 또한 지워도 됨
			wStr.erase(std::remove(wStr.begin(), wStr.end(), L'\n'), wStr.end());

			// /* ~ */ 제거
			{
				size_t xPos = wStr.find(L"/*");
				size_t yPos = wStr.find(L"*/");
				while (xPos != std::wstring::npos)
				{
					// */로 닫지 않았다면 이후 모든 것을 지워버림
					if (yPos == std::wstring::npos)
						yPos = wStr.size() - 1;

					// */ 문자 2개니깐 + 2
					wStr.erase(xPos, yPos - xPos + 2);

					// 여긴 지웠으니깐 ok
					xPos = wStr.find(L"/*", xPos);
					yPos = wStr.find(L"*/", yPos);
				}
			}

			// 모든 Key Value를 std::wstring으로 저장
			//	* Get 계열 함수에서 원하는 타입으로 변환

			bool hasEntries = false;
			// ClassName 무조건 :으로 끝남
			{
				size_t keyStart = wStr.find(L"-"); // 첫 -
				size_t valueEnd = 0;
				size_t xPos = 1; // wStr의 맨 앞 스킵
				size_t yPos = 0;
				size_t nextYPos = wStr.find(L":");
				while (true)
				{
					yPos = nextYPos;
					if (yPos == std::wstring::npos)
						break;

					// 클래스 이름
					std::wstring className = wStr.substr(xPos, yPos - xPos);

					// 이 뒤에 오는 것은 - Key = Value;
					// -를 찾음
					nextYPos = wStr.find(L":", nextYPos + 1);
					while (keyStart != std::wstring::npos)
					{
						// 다음 className 끝보다 keyStart가 앞지른 경우
						if (nextYPos < keyStart)
							break;

						size_t keyEnd = wStr.find(L"=", keyStart);
						keyStart++;
						std::wstring keyStr = wStr.substr(keyStart, keyEnd - keyStart);

						size_t valueStart = keyEnd + 1;
						valueEnd = wStr.find(L";", valueEnd + 1);
						std::wstring valueStr = wStr.substr(valueStart, valueEnd - valueStart);

						m_parsedDatas[className].insert(std::make_pair(keyStr, valueStr));
						hasEntries = true;

						keyStart = wStr.find(L"-", keyStart);
					}

					xPos = valueEnd + 1;
				}
			}

			return hasEntries;
		}

		// 필요하면 더 구현할 것
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, USHORT *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}

			INT out = _wtoi(keyIt->second.c_str());
			*outValue = (USHORT)out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, INT *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}

			INT out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, INT64 *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
				return FALSE;

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
				return FALSE;

			// 문자열 전체가 INT64 범위의 정수일 때만 반환한다.
			const std::wstring &text = keyIt->second;
			const WCHAR *pText = text.c_str();
			WCHAR *pEnd = nullptr;
			errno = 0;
			INT64 value = std::wcstoll(pText, &pEnd, 10);
			if (errno != 0 || pEnd == pText || pEnd != pText + text.size())
				return FALSE;

			*outValue = value;
			return TRUE;
		}
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, unsigned int *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}

			unsigned int out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, BYTE *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}

			BYTE out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, std::string *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}

			outValue->assign(keyIt->second.begin(), keyIt->second.end());
			return TRUE;
		}
		BOOL		Load(const WCHAR *pClassStr, const WCHAR *pKeyStr, std::wstring *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(pClassStr);
			if (classIt == m_parsedDatas.end())
			{
				// class 없음
				return FALSE;
			}

			auto keyIt = classIt->second.find(pKeyStr);
			if (keyIt == classIt->second.end())
			{
				// value 없음
				return FALSE;
			}
			outValue->assign(keyIt->second.begin(), keyIt->second.end());
			return TRUE;
		}

	private:
		// 파싱해서 데이터를 가지고 있을 자료구조
		std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
	};
}