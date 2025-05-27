#pragma once

#include "DefineSingleton.h"

#include <windows.h>
#include <string>
#include <ctime>
#include <strsafe.h>
#include <unordered_map>

namespace MHLib::utils
{
	enum class LOG_LEVEL
	{
		DEBUG,
		SYSTEM,
		ERR
	};

	class CLogger : public Singleton<CLogger>
	{
		friend class Singleton<CLogger>;

	private:
		CLogger()
		{
			InitializeSRWLock(&m_FileMapLock);
		}
		~CLogger() = default;

	public:
		void WriteLog(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *fmt, ...) noexcept
		{
			if (m_LogLevel > logLevel)
				return;

			tm tmTime;
			time_t nowTime = time(nullptr);
			localtime_s(&tmTime, &nowTime);

			WCHAR directoryName[256];
			HRESULT ret;
			if (directory != nullptr)
			{
				ret = StringCchPrintf(directoryName, 256, L".\\%s\\%s", m_mainLogDirectoryName, directory);
				if (ret != S_OK)
				{
					if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
					{
						WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
					}
				}
			}
			else
			{
				ret = StringCchPrintf(directoryName, 256, L".\\%s", m_mainLogDirectoryName);
				if (ret != S_OK)
				{
					if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
					{
						WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
					}
				}
			}

			WCHAR fileName[256];

			ret = StringCchPrintf(fileName, 256, L"%s\\%d%02d_%s.txt", directoryName, tmTime.tm_year + 1900, tmTime.tm_mon + 1, type);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
				}
			}

			INT64 logCount = InterlockedIncrement64(&m_LogCount);

			WCHAR logLevelStr[10] = { 0, };

			switch (logLevel)
			{
			case LOG_LEVEL::DEBUG:
				StringCchPrintf(logLevelStr, 10, L"DEBUG");
				break;
			case LOG_LEVEL::SYSTEM:
				StringCchPrintf(logLevelStr, 10, L"SYSTEM");
				break;
			case LOG_LEVEL::ERR:
				StringCchPrintf(logLevelStr, 10, L"ERROR");
				break;
			default:
				break;
			}

			// lockMap 탐색
			AcquireSRWLockExclusive(&m_FileMapLock);
			auto it = m_lockMap.find(fileName);
			if (it == m_lockMap.end())
			{
				// 없다면 디렉터리 생성하고 SRWLOCK 객체 생성
				CreateDirectory(directoryName, NULL);
				SRWLOCK *newSRWLock = new SRWLOCK;
				InitializeSRWLock(newSRWLock);
				m_lockMap.insert(std::make_pair(fileName, newSRWLock));
				it = m_lockMap.find(fileName);
			}
			ReleaseSRWLockExclusive(&m_FileMapLock);

			// 여기 코드부터는 map에 없는 경우는 없음
			SRWLOCK *srwLock = it->second;

			WCHAR messageBuf[256];

			va_list va;
			va_start(va, fmt);
			ret = StringCchVPrintf(messageBuf, 256, fmt, va);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
				}
			}
			va_end(va);

			WCHAR totalBuf[1024];
			//토탈 버퍼에 넣고 파일 쓰기
			ret = StringCchPrintf(totalBuf, 1024, L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] %s\n",
				type, tmTime.tm_year + 1900, tmTime.tm_mon + 1, tmTime.tm_mday,
				tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec, logLevelStr, logCount, messageBuf);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
				}
			}

			AcquireSRWLockExclusive(srwLock);
			FILE *pFile = _wfsopen(fileName, L"a, ccs = UTF-16LE", _SH_DENYWR);
			if (pFile == nullptr)
			{
				wprintf(L"file open fail, errorCode = %d\n", GetLastError());
				ReleaseSRWLockExclusive(srwLock);
				return;
			}
			fwrite(totalBuf, 1, wcslen(totalBuf) * sizeof(WCHAR), pFile);

			fclose(pFile);
			ReleaseSRWLockExclusive(srwLock);
		}
		void WriteLogHex(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *logName, BYTE *pBytes, int byteLen) noexcept
		{
			int offset = 0;
			WCHAR *messageBuf = new WCHAR[10000];

			offset += swprintf(messageBuf + offset, 10000 - offset, L"%s: ", logName);

			for (int i = 0; i < byteLen; i++)
			{
				offset += swprintf(messageBuf + offset, 10000 - offset, L"%02X ", pBytes[i]);
			}

			WriteLog(directory, type, logLevel, messageBuf);

			delete[] messageBuf;
		}
		void WriteLogConsole(LOG_LEVEL logLevel, const WCHAR *fmt, ...) noexcept
		{
			if (m_LogLevel > logLevel)
			{
				return;
			}

			WCHAR logBuffer[1024] = { 0, };

			va_list va;

			va_start(va, fmt);
			HRESULT ret = StringCchVPrintf(logBuffer, 1024, fmt, va);
			if (ret != S_OK)
			{
				if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
				{
					WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"로그 버퍼 크기 부족");
				}
			}
			va_end(va);

			ConsoleLock();
			wprintf(L"%s\n", logBuffer);
			ConsoleUnLock();
		}

		inline void SetMainDirectory(const WCHAR *directoryName) noexcept
		{
			m_mainLogDirectoryName = directoryName;
			CreateDirectory(directoryName, NULL);
		}
		inline void SetLogLevel(LOG_LEVEL logLevel) noexcept { m_LogLevel = logLevel; }

	private:
		void ConsoleLock() noexcept
		{
			AcquireSRWLockExclusive(&m_Consolelock);
		}

		void ConsoleUnLock() noexcept
		{
			ReleaseSRWLockExclusive(&m_Consolelock);
		}

	private:
		SRWLOCK m_FileMapLock;
		SRWLOCK m_Consolelock;

		std::unordered_map<std::wstring, SRWLOCK *> m_lockMap;

		INT64 m_LogCount = 0;
		LOG_LEVEL m_LogLevel = LOG_LEVEL::DEBUG;
		const WCHAR *m_mainLogDirectoryName = nullptr;
	};

	inline CLogger *g_Logger = nullptr;
}