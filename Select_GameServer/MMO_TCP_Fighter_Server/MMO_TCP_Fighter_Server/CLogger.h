#pragma once
#pragma warning(disable : 26110) // Lock 래핑 경고
enum class LOG_LEVEL
{
	DEBUG,
	SYSTEM,
	ERR
};

class CLogger
{
	SINGLE(CLogger)
public:
	void WriteLog(const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *fmt, ...);
	void WriteLogHex(const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *log, BYTE *pByte, int byteLen);
	void WriteLogConsole(LOG_LEVEL logLevel, const WCHAR *fmt, ...);

	inline void SetDirectory(const WCHAR *directoryName) { m_directoryName = directoryName; }
	inline void SetLogLevel(LOG_LEVEL logLevel) { m_LogLevel = logLevel; }

private:
	inline void FileLock()
	{
		AcquireSRWLockExclusive(&m_srwFileLock);
	}

	inline void FileUnLock()
	{
		ReleaseSRWLockExclusive(&m_srwFileLock);
	}

private:
	SRWLOCK m_srwFileLock;

	INT64 m_LogCount = 0;
	LOG_LEVEL m_LogLevel = LOG_LEVEL::DEBUG;
	const WCHAR *m_directoryName = nullptr;
	FILE *m_pFile = nullptr;
};

extern CLogger *g_Logger;