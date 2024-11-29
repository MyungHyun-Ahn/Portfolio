#pragma once
enum class LOG_LEVEL
{
	DEBUG,
	SYSTEM,
	ERR
};

// Directory / type ±¸ºÐ
// 

class CLogger
{
	SINGLE(CLogger)
public:
	void WriteLog(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *fmt, ...);
	void WriteLogHex(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *log, BYTE *pByte, int byteLen);
	void WriteLogConsole(LOG_LEVEL logLevel, const WCHAR *fmt, ...);

	inline void SetMainDirectory(const WCHAR *directoryName) 
	{ 
		m_mainLogDirectoryName = directoryName; 
		CreateDirectory(directoryName, NULL);
	}
	inline void SetLogLevel(LOG_LEVEL logLevel) { m_LogLevel = logLevel; }

private:
	void ConsoleLock()
	{
		AcquireSRWLockExclusive(&m_Consolelock);
	}

	void ConsoleUnLock()
	{
		ReleaseSRWLockExclusive(&m_Consolelock);
	}

private:
	SRWLOCK m_FileMapLock;
	SRWLOCK m_Consolelock;

	std::unordered_map<std::wstring, SRWLOCK> m_lockMap;

	INT64 m_LogCount = 0;
	LOG_LEVEL m_LogLevel = LOG_LEVEL::DEBUG;
	const WCHAR *m_mainLogDirectoryName = nullptr;
};

extern CLogger *g_Logger;