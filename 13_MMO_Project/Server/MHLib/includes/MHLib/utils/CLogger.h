#pragma once

namespace MHLib::utils
{
	enum class ELogLevel
	{
		Debug,
		Info,
		Error
	};

	class CLogger : public CSingleton<CLogger>
	{
		friend class CSingleton<CLogger>;

	private:
		CLogger()
		{
			InitializeSRWLock(&m_fileMapLock);
		}
		~CLogger() = default;

	public:
		// 세션 ID 0은 세션에 속하지 않는 이벤트를 뜻한다.
		void WriteLog(ELogLevel logLevel, const WCHAR *pProject, UINT64 sessionId,
			std::source_location location, const WCHAR *pFmt, ...) noexcept
		{
			if (m_logLevel > logLevel)
				return;

			const auto now = std::chrono::system_clock::now();
			const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
			const auto time = std::chrono::system_clock::to_time_t(seconds);
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
			tm utcTime{};
			tm localTime{};
			if (gmtime_s(&utcTime, &time) != 0 || localtime_s(&localTime, &time) != 0)
				return;

			WCHAR payload[512];
			va_list args;
			va_start(args, pFmt);
			const HRESULT result = StringCchVPrintfW(payload, _countof(payload), pFmt, args);
			va_end(args);
			if (FAILED(result))
				return;

			WCHAR session[21] = L"-";
			if (sessionId != 0)
				StringCchPrintfW(session, _countof(session), L"%llu", sessionId);
			const WCHAR *pLevel = logLevel == ELogLevel::Error ? L"Error" : logLevel == ELogLevel::Info ? L"Info" : L"Debug";
			WCHAR line[1024];
			if (FAILED(StringCchPrintfW(line, _countof(line),
				L"[%04d-%02d-%02dT%02d:%02d:%02d.%03dZ][%ls][%ls][%lu][%ls][%hs : %u] %ls",
				utcTime.tm_year + 1900, utcTime.tm_mon + 1, utcTime.tm_mday, utcTime.tm_hour, utcTime.tm_min, utcTime.tm_sec,
				static_cast<int>(milliseconds), pLevel, pProject, GetCurrentThreadId(), session,
				location.function_name(), static_cast<unsigned>(location.line()), payload)))
				return;

			// 기존 동기 출력 경로를 사용한다. 출력 정책 분리는 다음 단계에서 다룬다.
			WriteLogConsole(logLevel, L"%ls", line);
			WCHAR fileLine[1025];
			StringCchPrintfW(fileLine, _countof(fileLine), L"%ls\n", line);
			writeLogFile(L"SYSTEM", pProject, localTime, fileLine);
		}

		void WriteLog(const WCHAR *pDirectory, const WCHAR *pType, ELogLevel logLevel, const WCHAR *pFmt, ...) noexcept
		{
			if (m_logLevel > logLevel)
				return;

			tm tmTime;
			time_t nowTime = time(nullptr);
			localtime_s(&tmTime, &nowTime);

			HRESULT ret;
			INT64 logCount = InterlockedIncrement64(&m_logCount);

			WCHAR logLevelStr[10] = { 0, };

			switch (logLevel)
			{
			case ELogLevel::Debug:
				StringCchPrintf(logLevelStr, 10, L"DEBUG");
				break;
			case ELogLevel::Info:
				StringCchPrintf(logLevelStr, 10, L"INFO");
				break;
			case ELogLevel::Error:
				StringCchPrintf(logLevelStr, 10, L"ERROR");
				break;
			default:
				break;
			}

			WCHAR messageBuf[256];

			va_list va;
			va_start(va, pFmt);
			ret = StringCchVPrintf(messageBuf, 256, pFmt, va);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
				}
			}
			va_end(va);

			WCHAR totalBuf[1024];
			//토탈 버퍼에 넣고 파일 쓰기
			ret = StringCchPrintf(totalBuf, 1024, L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] %s\n",
				pType, tmTime.tm_year + 1900, tmTime.tm_mon + 1, tmTime.tm_mday,
				tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec, logLevelStr, logCount, messageBuf);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
				}
			}

			writeLogFile(pDirectory, pType, tmTime, totalBuf);
		}
		void WriteLogHex(const WCHAR *pDirectory, const WCHAR *pType, ELogLevel logLevel,
			const WCHAR *pLogName, BYTE *pBytes, int byteLen) noexcept
		{
			int offset = 0;
			WCHAR *pMessageBuf = new WCHAR[10000];

			offset += swprintf(pMessageBuf + offset, 10000 - offset, L"%s: ", pLogName);

			for (int i = 0; i < byteLen; i++)
			{
				offset += swprintf(pMessageBuf + offset, 10000 - offset, L"%02X ", pBytes[i]);
			}

			WriteLog(pDirectory, pType, logLevel, pMessageBuf);

			delete[] pMessageBuf;
		}
		void WriteLogConsole(ELogLevel logLevel, const WCHAR *pFmt, ...) noexcept
		{
			if (m_logLevel > logLevel)
			{
				return;
			}

			WCHAR logBuffer[1024] = { 0, };

			va_list va;

			va_start(va, pFmt);
			HRESULT ret = StringCchVPrintf(logBuffer, 1024, pFmt, va);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
				}
			}
			va_end(va);

			consoleLock();
			// 콘솔은 UTF-16, 리디렉션된 stdout은 UTF-8로 출력한다.
			HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD consoleMode = 0;
			if (GetConsoleMode(output, &consoleMode))
			{
				fflush(stdout);
				DWORD written = 0;
				WriteConsoleW(output, logBuffer, static_cast<DWORD>(wcslen(logBuffer)), &written, nullptr);
				WriteConsoleW(output, L"\r\n", 2, &written, nullptr);
			}
			else
			{
				char utf8Buffer[1024 * 3];
				int bytes = WideCharToMultiByte(CP_UTF8, 0, logBuffer, -1, utf8Buffer, sizeof(utf8Buffer), nullptr, nullptr);
				if (bytes > 0)
				{
					fwrite(utf8Buffer, 1, bytes - 1, stdout);
					fputc('\n', stdout);
					fflush(stdout);
				}
			}
			consoleUnlock();
		}

		inline void SetMainDirectory(const WCHAR *pDirectoryName) noexcept
		{
			m_pMainLogDirectoryName = pDirectoryName;
			CreateDirectory(pDirectoryName, NULL);
		}
		inline void SetLogLevel(ELogLevel logLevel) noexcept { m_logLevel = logLevel; }

	private:
		void writeLogFile(const WCHAR *pDirectory, const WCHAR *pType, const tm &tmTime, const WCHAR *pLine) noexcept
		{
			WCHAR directoryName[256];
			HRESULT ret;
			if (pDirectory != nullptr)
			{
				ret = StringCchPrintf(directoryName, 256, L".\\%s\\%s", m_pMainLogDirectoryName, pDirectory);
				if (ret != S_OK)
				{
					if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
					{
						WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
					}
				}
			}
			else
			{
				ret = StringCchPrintf(directoryName, 256, L".\\%s", m_pMainLogDirectoryName);
				if (ret != S_OK)
				{
					if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
					{
						WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
					}
				}
			}

			WCHAR fileName[256];

			ret = StringCchPrintf(fileName, 256, L"%s\\%d%02d_%s.txt", directoryName, tmTime.tm_year + 1900, tmTime.tm_mon + 1, pType);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", ELogLevel::Error, L"로그 버퍼 크기 부족");
				}
			}

			// lockMap 탐색
			AcquireSRWLockExclusive(&m_fileMapLock);
			auto it = m_lockMap.find(fileName);
			if (it == m_lockMap.end())
			{
				// 없다면 디렉터리 생성하고 SRWLOCK 객체 생성
				CreateDirectory(directoryName, NULL);
				SRWLOCK *pNewSRWLock = new SRWLOCK;
				InitializeSRWLock(pNewSRWLock);
				m_lockMap.insert(std::make_pair(fileName, pNewSRWLock));
				it = m_lockMap.find(fileName);
			}
			SRWLOCK *pSrwLock = it->second;
			ReleaseSRWLockExclusive(&m_fileMapLock);

			AcquireSRWLockExclusive(pSrwLock);
			FILE *pFile = _wfsopen(fileName, L"a, ccs = UTF-16LE", _SH_DENYWR);
			if (pFile == nullptr)
			{
				const int errorCode = errno;
				ReleaseSRWLockExclusive(pSrwLock);
				WriteLogConsole(ELogLevel::Error, L"log file open failed, errno=%d", errorCode);
				return;
			}
			fwrite(pLine, 1, wcslen(pLine) * sizeof(WCHAR), pFile);

			fclose(pFile);
			ReleaseSRWLockExclusive(pSrwLock);
		}

		void consoleLock() noexcept
		{
			AcquireSRWLockExclusive(&m_consolelock);
		}

		void consoleUnlock() noexcept
		{
			ReleaseSRWLockExclusive(&m_consolelock);
		}

	private:
		SRWLOCK m_fileMapLock;
		SRWLOCK m_consolelock = SRWLOCK_INIT;

		std::unordered_map<std::wstring, SRWLOCK *> m_lockMap;

		INT64 m_logCount = 0;
		ELogLevel m_logLevel = ELogLevel::Debug;
		const WCHAR *m_pMainLogDirectoryName = nullptr;
	};

	inline CLogger *g_pLogger = nullptr;
}