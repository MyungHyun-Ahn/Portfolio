#pragma once

#pragma warning(disable : 6011) // Crash 위한 nullptr 참조

class CCrashDump
{
public:
	CCrashDump()
	{
		_invalid_parameter_handler oldHandler, newHandler;
		newHandler = myInvalidParameterHandler;

		oldHandler = _set_invalid_parameter_handler(newHandler);	// CRT 함수에 nullptr 전달
		_CrtSetReportMode(_CRT_WARN, 0);							// CRT 오류 메시지 표시 중단. 바로 덤프에 남기도록
		_CrtSetReportMode(_CRT_ASSERT, 0);							// CRT 오류 메시지 표시 중단. 바로 덤프에 남기도록
		_CrtSetReportMode(_CRT_ERROR, 0);							// CRT 오류 메시지 표시 중단. 바로 덤프에 남기도록

		_CrtSetReportHook(_custom_Report_hook);

		// pure virtual function called 에러 핸들러를 사용자 정의 함수로 우회
		_set_purecall_handler(myPurecallHandler);

		SetHandlerDump();
	}

	// nullptr을 참조함으로 Crash 유발
	static void Crash()
	{
		int *p = nullptr;
		*p = 0;
	}

	// 실제 예외 발생시 덤프를 생성시키는 함수
	// 카운터
	//    멀티 스레드 환경에서는 한 스레드에서 예외 발생시 연속으로 다른 스레드가 예외를 발생시킴
	//    이 경우 첫번째 예외 핸들러가 덤프를 저장하는 과정에서 다른 예외 핸들러가 덮어 쓰거나 제대로 덤프를 남기지 못할 수 있음
	//    그래서 각자 별도의 이름으로 넘버링하여 덤프를 생성
	static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
	{
		int iWorkingMemory = 0;
		SYSTEMTIME nowTime;

		long dumpCount = InterlockedIncrement(&m_lDumpCount);

		WCHAR fileName[MAX_PATH];

		// 현재 시간을 알아옴
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


