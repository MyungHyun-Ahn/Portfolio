#pragma once

#pragma warning(disable : 6011) // Crash ���� nullptr ����

class CCrashDump
{
public:
	CCrashDump()
	{
		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = myInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler);	// CRT �Լ��� nullptr ����
		_CrtSetReportMode(_CRT_WARN, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��
		_CrtSetReportMode(_CRT_ASSERT, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��
		_CrtSetReportMode(_CRT_ERROR, 0);							// CRT ���� �޽��� ǥ�� �ߴ�. �ٷ� ������ ���⵵��

		_CrtSetReportHook(_custom_Report_hook);

		// pure virtual function called ���� �ڵ鷯�� ����� ���� �Լ��� ��ȸ
		_set_purecall_handler(myPurecallHandler);

		SetHandlerDump();
	}

	// nullptr�� ���������� Crash ����
	static void Crash()
	{
		int *p = nullptr;
		*p = 0;
	}

	// ���� ���� �߻��� ������ ������Ű�� �Լ�
	// ī����
	//    ��Ƽ ������ ȯ�濡���� �� �����忡�� ���� �߻��� �������� �ٸ� �����尡 ���ܸ� �߻���Ŵ
	//    �� ��� ù��° ���� �ڵ鷯�� ������ �����ϴ� �������� �ٸ� ���� �ڵ鷯�� ���� ���ų� ����� ������ ������ ���� �� ����
	//    �׷��� ���� ������ �̸����� �ѹ����Ͽ� ������ ����
	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME nowTime;

		long dumpCount = InterlockedIncrement(&m_lDumpCount);

		WCHAR fileName[MAX_PATH];

		// ���� �ð��� �˾ƿ�
		GetLocalTime(&nowTime);
		wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d.%02d_%d.dmp", nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond, dumpCount);

		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"\n\n\n Crash Error!!!");
		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"Saving dump file...");

		HANDLE hDumpFile = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hDumpFile != INVALID_HANDLE_VALUE)
		{
			_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInformation{ 0, };
			miniDumpExceptionInformation.ThreadId = ::GetCurrentThreadId();
			miniDumpExceptionInformation.ExceptionPointers = pExceptionPointer;
			miniDumpExceptionInformation.ClientPointers = TRUE;

			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &miniDumpExceptionInformation, NULL, NULL);
			CloseHandle(hDumpFile);
			g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"CrashDump saved completed");
		}
		else
		{
			g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"CrashDump failed");
		}

		return EXCEPTION_EXECUTE_HANDLER;
	}

	static void SetHandlerDump()
	{
		SetUnhandledExceptionFilter(MyExceptionFilter);
	}

	static void myInvalidParameterHandler(const wchar_t *expression, const wchar_t *function, const wchar_t *file, unsigned int line, uintptr_t pReserved)
	{
		Crash();
	}

	static int _custom_Report_hook(int ireposttype, char *message, int *returnvalue)
	{
		Crash();
		return true;
	}

	static void myPurecallHandler(void)
	{
		Crash();
	}

public:
	inline static long m_lDumpCount = 0;
};
