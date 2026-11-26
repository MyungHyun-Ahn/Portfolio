#pragma once

namespace MHLib::utils
{
	struct SLoggingSettings
	{
		ELogLevel consoleLevel;
		ELogLevel fileLevel;
		std::chrono::milliseconds pollInterval;
		std::size_t queueCapacity;

		// Logging 섹션을 검증한다. 로그 경로와 시작 시점은 호출자가 정한다.
		[[nodiscard]]
		static bool Load(CFileLoader &loader, SLoggingSettings &outSettings, const wchar_t *&outError)
		{
			constexpr const wchar_t *kSection = L"Logging";
			std::wstring misplaced;
			outError = L"CONSOLE_LOG_LEVEL belongs in Logging:";
			if (loader.Load(L"Server", L"CONSOLE_LOG_LEVEL", &misplaced))
				return false;
			outError = L"FILE_LOG_LEVEL belongs in Logging:";
			if (loader.Load(L"Server", L"FILE_LOG_LEVEL", &misplaced))
				return false;
			outError = L"LOG_QUEUE_CAPACITY belongs in Logging:";
			if (loader.Load(L"Server", L"LOG_QUEUE_CAPACITY", &misplaced))
				return false;
			outError = L"LOG_POLL_INTERVAL_MS belongs in Logging:";
			if (loader.Load(L"Server", L"LOG_POLL_INTERVAL_MS", &misplaced))
				return false;

			ELogLevel consoleLevel{};
			outError = L"Logging: CONSOLE_LOG_LEVEL must be Debug, Info or Error";
			if (!loadLevel(loader, kSection, L"CONSOLE_LOG_LEVEL", consoleLevel))
				return false;

			ELogLevel fileLevel{};
			outError = L"Logging: FILE_LOG_LEVEL must be Debug, Info or Error";
			if (!loadLevel(loader, kSection, L"FILE_LOG_LEVEL", fileLevel))
				return false;

			INT64 queueCapacity = 0;
			outError = L"Logging: LOG_QUEUE_CAPACITY must be an integer (1..1048576)";
			if (!loader.Load(kSection, L"LOG_QUEUE_CAPACITY", &queueCapacity) || queueCapacity < 1 || queueCapacity > 1024 * 1024)
				return false;

			INT64 pollMilliseconds = 0;
			outError = L"Logging: LOG_POLL_INTERVAL_MS must be an integer (1..1000)";
			if (!loader.Load(kSection, L"LOG_POLL_INTERVAL_MS", &pollMilliseconds) || pollMilliseconds < 1 || pollMilliseconds > 1000)
				return false;

			outSettings.consoleLevel = consoleLevel;
			outSettings.fileLevel = fileLevel;
			outSettings.pollInterval = std::chrono::milliseconds{pollMilliseconds};
			outSettings.queueCapacity = static_cast<std::size_t>(queueCapacity);
			outError = nullptr;
			return true;
		}

	private:
		// 출력 레벨의 키를 읽어 로거가 쓰는 열거형으로 바꾼다.
		static bool loadLevel(CFileLoader &loader, const wchar_t *pSection, const wchar_t *pKey, ELogLevel &outLevel)
		{
			std::wstring value;
			if (!loader.Load(pSection, pKey, &value))
				return false;
			if (value == L"Debug")
				outLevel = ELogLevel::Debug;
			else if (value == L"Info")
				outLevel = ELogLevel::Info;
			else if (value == L"Error")
				outLevel = ELogLevel::Error;
			else
				return false;
			return true;
		}
	};
}
