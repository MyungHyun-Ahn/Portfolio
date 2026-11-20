#pragma once

namespace MHLib::utils
{
	enum class ELogLevel
	{
		Debug,
		Info,
		Error
	};

	struct SLogEntry
	{
		ELogLevel m_level;			// 로그 레벨 분기용
		std::wstring m_message;		// 완성된 로그 문자열을 넣어야 한다.
	};

	class CLogger : public CSingleton<CLogger>
	{
		friend class CSingleton<CLogger>;

	private:
		CLogger()
		{
			InitializeSRWLock(&m_fileMapLock);
		}
		~CLogger()
		{
			Stop();
		}

		// Producer 구조체
		// 1. 로거가 Producer를 소유하고 m_pNext로 연결한다.
		// 2. m_drops는 큐가 가득 차서 버린 로그의 레벨별 건수다. (집계용)
		// 3. 각 생산자 스레드는 자기 Producer 포인터를 TLS에 보관한다.
		struct SProducer
		{
			explicit SProducer(std::size_t capacity)
				: m_queue(capacity)
			{
			}

			containers::CLFSpscQueue<SLogEntry> m_queue;
			std::atomic<std::uint64_t> m_drops[3]{};
			SProducer *m_pNext = nullptr;
		};

		static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

	public:
		static constexpr std::chrono::milliseconds kDefaultPollInterval{10};

		// 로그 스레드를 시작한다.
		// 1. 파일을 연다. (동기)
		// 2. 로그 스레드를 시작한다.
		[[nodiscard]] bool Start(const std::filesystem::path &directory, ELogLevel consoleLevel, ELogLevel fileLevel,
			std::chrono::milliseconds pollInterval = kDefaultPollInterval, std::size_t capacity = 1024)
		{
			if (m_isAsync || pollInterval.count() <= 0 || capacity == 0 || capacity == (std::numeric_limits<std::size_t>::max)())
				return false;

			std::ofstream file;
			openRunFile(directory, file);
			if (!file.is_open())
				return false;

			m_consoleLevel = consoleLevel;
			m_fileLevel = fileLevel;
			m_queueCapacity = capacity;
			m_outputThread = std::thread([this, pollInterval, file = std::move(file)]() mutable
			{
				runOutput(file, pollInterval);
			});
			m_isAsync = true;
			return true;
		}

		// 큐를 비울 수 없는 상황에 사용한다.
		// - ex) 워커 스레드 크래시 발생
		// - WriteLog를 사용하면 로거 큐에 로그를 Enqueue 해도
		// - 프로세스가 종료되기 때문에 로그가 남지 않을 수 있다.
		static void WriteEmergencyLogToStderr(const char *pFormat, ...) noexcept
		{
			va_list args;
			va_start(args, pFormat);
			vfprintf(stderr, pFormat, args);
			va_end(args);
			fflush(stderr);
		}

		// 각 스레드는 이 함수를 통해 Producer를 등록해야 한다.
		void RegisterLogProducer()
		{
			if (!m_isAsync || m_isStopRequested.load() || sm_pProducer != nullptr)
				return;

			auto *pProducer = new SProducer(m_queueCapacity);
			pProducer->m_pNext = m_pProducers.load();
			while (!m_pProducers.compare_exchange_weak(pProducer->m_pNext, pProducer))
			{
			}
			sm_pProducer = pProducer;
		}

		// Stop() 호출 전에 모든 생산자의 등록과 로그 제출을 끝내야 한다.
		// 1. 로그 스레드의 종료를 요청한다. (m_isStopRequested = true)
		// 2. 종료 요청이 들어오면 로그 스레드는 큐를 비운 뒤 종료한다.
		// 3. 로그 스레드가 종료되면 Producer를 해제한다.
		void Stop() noexcept
		{
			if (!m_isAsync || m_isStopRequested.exchange(true))
				return;
			m_outputThread.join();
			sm_pProducer = nullptr;
			auto *pProducer = m_pProducers.exchange(nullptr);
			while (pProducer != nullptr)
			{
				auto *pNext = pProducer->m_pNext;
				delete pProducer;
				pProducer = pNext;
			}
		}

		// sessionId 0은 세션에 속하지 않는 이벤트를 뜻한다.
		void WriteLog(ELogLevel logLevel, const WCHAR *pProject, UINT64 sessionId,
			std::source_location location, const WCHAR *pFmt, ...) noexcept
		{
			if (m_isAsync ? (m_isStopRequested.load() || sm_pProducer == nullptr) : m_logLevel > logLevel)
				return;
			
			// 1. 로그 본문을 만든다.
			const auto now = std::chrono::system_clock::now();
			const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
			const auto time = std::chrono::system_clock::to_time_t(seconds);

			WCHAR payload[512];
			va_list args;
			va_start(args, pFmt);
			const HRESULT result = StringCchVPrintfW(payload, _countof(payload), pFmt, args);
			va_end(args);
			if (FAILED(result))
				return;

			WCHAR line[1024];
			if (!formatLine(now, logLevel, pProject, sessionId, location, payload, line))
				return;
			
			// 2. Async 모드이면 큐에 넣는다. (실패 시 로그를 버린다.)
			//    - Start()에서 출력 스레드를 생성한 뒤 m_isAsync를 true로 설정한다.
			if (m_isAsync)
			{
				SLogEntry entry{logLevel, line};
				for (auto &character : entry.m_message)
				{
					if (character == L'\r' || character == L'\n')
						character = L' ';
				}
				if (!sm_pProducer->m_queue.TryEnqueue(std::move(entry)))
					sm_pProducer->m_drops[static_cast<std::size_t>(logLevel)].fetch_add(1);
				return;
			}

			// 3. Sync 모드이면 콘솔과 파일에 바로 쓴다.
			tm localTime{};
			if (localtime_s(&localTime, &time) != 0)
				return;
			WriteLogConsole(logLevel, L"%ls", line);
			WCHAR fileLine[1025];
			StringCchPrintfW(fileLine, _countof(fileLine), L"%ls\n", line);
			writeLogFile(L"SYSTEM", pProject, localTime, fileLine);
		}

		// 레거시 동기 파일 출력. 비동기 큐를 사용하지 않는다.
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

			writeConsole(logBuffer);
		}

		inline void SetMainDirectory(const WCHAR *pDirectoryName) noexcept
		{
			m_pMainLogDirectoryName = pDirectoryName;
			CreateDirectory(pDirectoryName, NULL);
		}
		inline void SetLogLevel(ELogLevel logLevel) noexcept { m_logLevel = logLevel; }

	private:
		static bool formatLine(std::chrono::system_clock::time_point now, ELogLevel level, const WCHAR *pProject,
			UINT64 sessionId, std::source_location location, const WCHAR *pPayload, WCHAR (&outLine)[1024]) noexcept
		{
			const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
			const auto time = std::chrono::system_clock::to_time_t(seconds);
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
			tm utcTime{};
			if (gmtime_s(&utcTime, &time) != 0)
				return false;

			WCHAR session[21] = L"-";
			if (sessionId != 0)
				StringCchPrintfW(session, _countof(session), L"%llu", sessionId);
			const WCHAR *pLevel = level == ELogLevel::Error ? L"Error" : level == ELogLevel::Info ? L"Info" : L"Debug";
			return SUCCEEDED(StringCchPrintfW(outLine, _countof(outLine),
				L"[%04d-%02d-%02dT%02d:%02d:%02d.%03dZ][%ls][%ls][%lu][%ls][%hs : %u] %ls",
				utcTime.tm_year + 1900, utcTime.tm_mon + 1, utcTime.tm_mday, utcTime.tm_hour, utcTime.tm_min, utcTime.tm_sec,
				static_cast<int>(milliseconds), pLevel, pProject, getLogThreadId(), session,
				location.function_name(), static_cast<unsigned>(location.line()), pPayload));
		}

		// Win32 콘솔/문자 변환 경계. 생산자의 스레드 번호만 이 경계에서 조회한다.
		static unsigned long getLogThreadId() noexcept
		{
			thread_local const auto id = GetCurrentThreadId();
			return id;
		}

		static std::string toUtf8(const WCHAR *pLine)
		{
			const int length = static_cast<int>(wcslen(pLine));
			const int size = WideCharToMultiByte(CP_UTF8, 0, pLine, length, nullptr, 0, nullptr, nullptr);
			std::string bytes(static_cast<std::size_t>(size), '\0');
			if (size != 0 && WideCharToMultiByte(CP_UTF8, 0, pLine, length, bytes.data(), size, nullptr, nullptr) != size)
				bytes.clear();
			return bytes;
		}

		void writeConsole(const WCHAR *pLine) noexcept
		{
			CLockGuard<ELockType::Exclusive> consoleOutputLock(m_consoleOutputLock);
			HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD consoleMode = 0;
			if (GetConsoleMode(output, &consoleMode))
			{
				fflush(stdout);
				DWORD written = 0;
				const auto length = static_cast<DWORD>(wcslen(pLine));
				if (!WriteConsoleW(output, pLine, length, &written, nullptr) || written != length
					|| !WriteConsoleW(output, L"\r\n", 2, &written, nullptr) || written != 2)
					reportConsoleErrorLocked();
			}
			else
			{
				auto bytes = toUtf8(pLine);
				bytes.push_back('\n');
				if (fwrite(bytes.data(), 1, bytes.size(), stdout) != bytes.size() || fflush(stdout) != 0)
					reportConsoleErrorLocked();
			}
		}

		// m_consoleOutputLock을 잡은 상태에서만 호출한다.
		void reportConsoleErrorLocked() noexcept
		{
			if (!m_hasConsoleError)
			{
				m_hasConsoleError = true;
				fputs("CLogger: console write failed\n", stderr);
			}
		}

		static void openRunFile(const std::filesystem::path &directory, std::ofstream &outFile)
		{
			constexpr DWORD kExecutablePathCapacity = 32768;
			std::wstring executablePath(kExecutablePathCapacity, L'\0');
			const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), kExecutablePathCapacity);
			if (length == 0 || length >= kExecutablePathCapacity)
				return;
			executablePath.resize(length);

			const auto now = std::chrono::system_clock::now();
			const auto time = std::chrono::system_clock::to_time_t(std::chrono::floor<std::chrono::seconds>(now));
			tm utcTime{};
			WCHAR dateTime[14];
			if (gmtime_s(&utcTime, &time) != 0 || std::wcsftime(dateTime, _countof(dateTime), L"%y%m%d-%H%M%S", &utcTime) == 0)
				return;
			const auto fileName = std::wstring(dateTime) + L"-" + std::filesystem::path(executablePath).stem().wstring() + L".log";

			std::error_code error;
			std::filesystem::create_directories(directory, error);
			const auto stamp = now.time_since_epoch().count();
			// 디렉터리 생성 성공이 실행 경로의 독점권이다. 다른 프로세스의 기록을 덮지 않는다.
			for (unsigned suffix = 0; !error && suffix < 128; ++suffix)
			{
				auto runDir = directory / ("run-" + std::to_string(stamp) + "-" + std::to_string(suffix));
				if (!std::filesystem::create_directory(runDir, error))
					continue;
				outFile.open(runDir / fileName, std::ios::binary);
				break;
			}
		}

		void emit(const SLogEntry &entry, std::ofstream &file, bool &outHasFileWrite)
		{
			if (entry.m_level >= m_fileLevel && file)
			{
				file << toUtf8(entry.m_message.c_str()) << '\n';
				outHasFileWrite = true;
			}
			if (entry.m_level >= m_consoleLevel)
				writeConsole(entry.m_message.c_str());
		}


		// 한 라운드에 생산자당 최대 64건을 꺼내고, 출력 대상별 레벨 설정에 따라 기록한다.
		bool drainRound(std::ofstream &file, bool &inOutHasFileError)
		{
			constexpr std::size_t kBatchPerProducer = 64;
			bool didDrain = false;
			bool hasFileWrite = false;
			SLogEntry entry{};
			std::uint64_t drops[3]{};
			for (auto *pProducer = m_pProducers.load(); pProducer != nullptr; pProducer = pProducer->m_pNext)
			{
				for (std::size_t index = 0; index < kBatchPerProducer && pProducer->m_queue.TryDequeue(entry); ++index)
				{
					emit(entry, file, hasFileWrite);
					didDrain = true;
				}
				for (std::size_t level = 0; level < 3; ++level)
					drops[level] += pProducer->m_drops[level].exchange(0);
			}

			if (drops[0] != 0 || drops[1] != 0 || drops[2] != 0)
			{
				// 소비자가 만드는 집계만 큐를 거치지 않는다.
				WCHAR payload[256];
				WCHAR line[1024];
				if (SUCCEEDED(StringCchPrintfW(payload, _countof(payload),
					L"event=log_queue_dropped debug=%llu info=%llu error=%llu | 로그 큐가 가득 찼습니다.", drops[0], drops[1], drops[2]))
					&& formatLine(std::chrono::system_clock::now(), ELogLevel::Error, L"MHLib", 0,
						std::source_location::current(), payload, line))
					emit({ELogLevel::Error, line}, file, hasFileWrite);
			}

			if (hasFileWrite && !file.flush() && !inOutHasFileError)
			{
				inOutHasFileError = true;
				fputs("CLogger: event file write/flush failed\n", stderr);
			}

			return didDrain;
		}

		void runOutput(std::ofstream &file, std::chrono::milliseconds pollInterval)
		{
			bool hasFileError = false;
			while (true)
			{
				const bool isStopping = m_isStopRequested.load();
				const bool didDrain = drainRound(file, hasFileError);

				if (isStopping && !didDrain)
					break;

				if (!isStopping)
					std::this_thread::sleep_for(pollInterval);
			}

			file.close();
			if (!file && !hasFileError)
				fputs("CLogger: event file close failed\n", stderr);
		}

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

	private:
		SRWLOCK m_fileMapLock;
		SRWLOCK m_consoleOutputLock = SRWLOCK_INIT;
		bool m_hasConsoleError = false;

		// Producer
		std::atomic<SProducer *> m_pProducers = nullptr;
		inline static thread_local SProducer *sm_pProducer = nullptr;
		std::thread m_outputThread;
		std::size_t m_queueCapacity = 0;

		// 콘솔과 파일의 최소 출력 레벨을 각각 설정한다.
		ELogLevel m_consoleLevel = ELogLevel::Info;
		ELogLevel m_fileLevel = ELogLevel::Debug;

		// 로거 상태 플래그
		bool m_isAsync = false; // 로그 스레드 시작 전 : 동기
								// 로그 스레드 시작 후 : 비동기(SPSC Queue)

		// 종료 절차가 시작되었음을 나타내는 플래그
		// - 새 로그 접수를 막는다.
		// - 큐에 쌓인 남은 로그를 처리한다.
		std::atomic<bool> m_isStopRequested = false;

		std::unordered_map<std::wstring, SRWLOCK *> m_lockMap;

		INT64 m_logCount = 0;
		ELogLevel m_logLevel = ELogLevel::Debug;
		const WCHAR *m_pMainLogDirectoryName = nullptr;
	};

	inline CLogger *g_pLogger = nullptr;
}