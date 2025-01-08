#include "pch.h"
#include "CLogger.h"

CLogger *g_Logger;

CLogger::CLogger()
{
	InitializeSRWLock(&m_FileMapLock);
}

CLogger::~CLogger()
{
}

void CLogger::WriteLog(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *fmt, ...) noexcept
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
				WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
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
				WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
			}
		}
	}

	WCHAR fileName[256];

	ret = StringCchPrintf(fileName, 256, L"%s\\%d%02d_%s.txt", directoryName, tmTime.tm_year + 1900, tmTime.tm_mon + 1, type);
	if (ret != S_OK)
	{
		if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
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

	// lockMap Ž��
	AcquireSRWLockExclusive(&m_FileMapLock);
	auto it = m_lockMap.find(fileName);
	if (it != m_lockMap.end())
	{
		// ���ٸ� ���͸� �����ϰ� SRWLOCK ��ü ����
		CreateDirectory(directoryName, NULL);
		SRWLOCK newSRWLock;
		InitializeSRWLock(&newSRWLock);
		m_lockMap.insert(std::make_pair(fileName, newSRWLock));
	}
	ReleaseSRWLockExclusive(&m_FileMapLock);

	// ���� �ڵ���ʹ� map�� ���� ���� ����
	SRWLOCK srwLock = m_lockMap[fileName];

	WCHAR messageBuf[256];

	va_list va;
	va_start(va, fmt);
	ret = StringCchVPrintf(messageBuf, 256, fmt, va);
	if (ret != S_OK)
	{
		if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
		}
	}
	va_end(va);

	WCHAR totalBuf[1024];
	//��Ż ���ۿ� �ְ� ���� ����
	ret = StringCchPrintf(totalBuf, 1024, L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] %s\n",
		type, tmTime.tm_year + 1900, tmTime.tm_mon + 1, tmTime.tm_mday,
		tmTime.tm_hour, tmTime.tm_min, tmTime.tm_sec, logLevelStr, logCount, messageBuf);
	if (ret != S_OK)
	{
		if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
		}
	}

	AcquireSRWLockExclusive(&srwLock);
	FILE *pFile = _wfsopen(fileName, L"a, ccs = UTF-16LE", _SH_DENYWR);
	if (pFile == nullptr)
	{
		wprintf(L"file open fail, errorCode = %d\n", GetLastError());
		ReleaseSRWLockExclusive(&srwLock);
		return;
	}
	fwrite(totalBuf, 1, wcslen(totalBuf) * sizeof(WCHAR), pFile);

	fclose(pFile);
	ReleaseSRWLockExclusive(&srwLock);
}

void CLogger::WriteLogHex(const WCHAR *directory, const WCHAR *type, LOG_LEVEL logLevel, const WCHAR *logName, BYTE *pBytes, int byteLen) noexcept
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

void CLogger::WriteLogConsole(LOG_LEVEL logLevel, const WCHAR *fmt, ...) noexcept
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
			WriteLog(L"SYSTEM", L"Logging", LOG_LEVEL::ERR, L"�α� ���� ũ�� ����");
		}
	}
	va_end(va);

	ConsoleLock();
	wprintf(L"%s\n", logBuffer);
	ConsoleUnLock();
}