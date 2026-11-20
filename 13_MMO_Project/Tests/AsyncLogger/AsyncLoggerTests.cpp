#include <atomic>
#include <regex>
#include <sstream>
#include "MHLib/MHLibPch.h"

using MHLib::utils::CLogger;
using MHLib::utils::ELogLevel;

namespace
{
	constexpr auto kOutputDirectory = L"AsyncLoggerNative";
	constexpr unsigned kCapacity = 4;
	constexpr unsigned kEvents = 12;
	constexpr auto kUnicode = " | 한글 😀 100% %ls line";

	int CheckWindowsConsole(const std::filesystem::path &testDir)
	{
		DWORD mode = 0;
		if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
			return 2;
		auto &logger = *CLogger::GetInstance();
		if (!logger.Start(testDir, ELogLevel::Info, ELogLevel::Error))
			return 2;
		logger.RegisterLogProducer();
		logger.WriteLog(ELogLevel::Info, L"Native", 0, std::source_location::current(), L"event=attached | 한글 😀 100%% %%ls");
		logger.Stop();
		return 0;
	}

	std::string ReadBytes(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	}

	bool CheckOutput(const std::string &file, const std::string &console)
	{
		const auto validBytes = [](const std::string &bytes)
		{
			return !bytes.empty() && bytes.starts_with('[') && bytes.ends_with('\n') && bytes.find('\0') == std::string::npos;
		};
		if (!validBytes(file) || !validBytes(console))
			return false;

		unsigned accepted[2]{};
		unsigned nextMinSequence[2]{};
		unsigned admittedByLevel[3]{};
		unsigned debugDrops = 0, infoDrops = 0, errorDrops = 0;
		std::vector<std::string> expectedConsole;
		std::istringstream lines(file);
		for (std::string line; std::getline(lines, line);)
		{
			const auto batch = line.find("event=slow_sink");
			if (batch != std::string::npos)
			{
				unsigned producer = 0, sequence = 0;
				if (sscanf_s(line.c_str() + batch, "event=slow_sink producer=%u seq=%u", &producer, &sequence) != 2
					|| producer >= 2 || sequence >= kEvents || sequence < nextMinSequence[producer] || !line.ends_with(kUnicode))
					return false;
				nextMinSequence[producer] = sequence + 1;
				++accepted[producer];
				const auto level = sequence % 3;
				if (level != 0)
					expectedConsole.push_back(line);
				++admittedByLevel[level];
			}
			const auto drop = line.find("event=log_queue_dropped");
			if (drop != std::string::npos)
			{
				unsigned debug = 0, info = 0, error = 0;
				if (sscanf_s(line.c_str() + drop, "event=log_queue_dropped debug=%u info=%u error=%u", &debug, &info, &error) != 3)
					return false;
				debugDrops += debug;
				infoDrops += info;
				errorDrops += error;
				expectedConsole.push_back(line);
			}
		}

		std::vector<std::string> actualConsole;
		std::istringstream consoleLines(console);
		for (std::string line; std::getline(consoleLines, line);)
		{
			if (line.ends_with('\r'))
				line.pop_back();
			actualConsole.push_back(std::move(line));
		}

		constexpr unsigned kSubmittedPerLevel = 2 * kEvents / 3;
		return actualConsole == expectedConsole && accepted[0] >= kCapacity && accepted[1] >= kCapacity
			&& debugDrops != 0 && infoDrops != 0 && errorDrops != 0
			&& admittedByLevel[0] + debugDrops == kSubmittedPerLevel
			&& admittedByLevel[1] + infoDrops == kSubmittedPerLevel
			&& admittedByLevel[2] + errorDrops == kSubmittedPerLevel
			&& file.find("event=after_stop") == std::string::npos && console.find("event=after_stop") == std::string::npos;
	}

	bool CheckOutputOracle()
	{
		const auto record = [](unsigned producer, unsigned sequence)
		{
			return std::format("[event] event=slow_sink producer={} seq={}{}", producer, sequence, kUnicode);
		};
		std::string file;
		std::string console;
		for (unsigned producer = 0; producer < 2; ++producer)
		{
			for (unsigned sequence : {0u, 1u, 2u, 4u})
			{
				const auto line = record(producer, sequence);
				file += line + '\n';
				if (sequence != 0)
					console += line + "\r\n";
			}
		}
		const auto drops = "[drop] event=log_queue_dropped debug=6 info=4 error=6";
		file += std::string(drops) + '\n';
		console += std::string(drops) + "\r\n";
		const auto first = console.find('\n') + 1;
		const auto second = console.find('\n', first) + 1;
		const auto duplicate = console.substr(0, first) + console;
		const auto reordered = console.substr(first, second - first) + console.substr(0, first) + console.substr(second);
		return CheckOutput(file, console) && !CheckOutput(file, duplicate) && !CheckOutput(file, reordered)
			&& !CheckOutput(file, console.substr(first));
	}
}

int main(void)
{
	if (!CheckOutputOracle())
	{
		fputs("FAIL: oracle must accept FIFO gaps and reject missing, duplicate or reordered console records\n", stderr);
		return 1;
	}
	std::filesystem::create_directories(kOutputDirectory);
	const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
	const auto testDir = std::filesystem::path(kOutputDirectory) / (std::to_string(stamp) + "-" + std::to_string(GetCurrentProcessId()));
	if (!std::filesystem::create_directory(testDir))
		return 1;
	WCHAR consoleTest[2]{};
	if (GetEnvironmentVariableW(L"MMO_LOGGER_CONSOLE_TEST", consoleTest, _countof(consoleTest)) == 1 && consoleTest[0] == L'1')
		return CheckWindowsConsole(testDir);
	const auto stdoutPath = testDir / L"stdout.txt";
	FILE *pOutput = nullptr;
	if (_wfreopen_s(&pOutput, stdoutPath.c_str(), L"wb", stdout) != 0)
		return 1;

	auto &logger = *CLogger::GetInstance();
	const auto pollInterval = std::chrono::milliseconds(25);
	if (logger.Start(stdoutPath, ELogLevel::Info, ELogLevel::Debug, pollInterval, kCapacity)
		|| !logger.Start(testDir, ELogLevel::Info, ELogLevel::Debug, pollInterval, kCapacity))
		return 1;
	std::atomic<unsigned> submitted{0};
	// 소비자가 첫 콘솔 출력에서 멈추도록 실제 CRT stdout 잠금을 잡는다.
	_lock_file(stdout);
	const auto produce = [&](unsigned producer)
	{
		logger.RegisterLogProducer();
		logger.RegisterLogProducer();
		for (unsigned sequence = 0; sequence < kEvents; ++sequence)
		{
			logger.WriteLog(static_cast<ELogLevel>(sequence % 3), L"AsyncLoggerTests", producer + 1,
				std::source_location::current(), L"event=slow_sink producer=%u seq=%u | 한글 😀 100%% %%ls\nline", producer, sequence);
		}
		submitted.fetch_add(1, std::memory_order_relaxed);
	};
	std::thread first(produce, 0);
	std::thread second(produce, 1);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	// stdout 잠금을 풀기 전에 두 생산자가 제출을 마쳤는지만 확인한다.
	while (submitted.load(std::memory_order_relaxed) != 2 && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const bool didSubmit = submitted.load(std::memory_order_relaxed) == 2;
	_unlock_file(stdout);
	first.join();
	second.join();
	logger.Stop();
	logger.WriteLog(ELogLevel::Error, L"AsyncLoggerTests", 0, std::source_location::current(), L"event=after_stop | 금지");
	fclose(stdout);
	if (!didSubmit)
	{
		fputs("FAIL: producer waited for the stdout sink\n", stderr);
		return 1;
	}
	std::vector<std::filesystem::path> paths;
	for (const auto &item : std::filesystem::directory_iterator(testDir))
	{
		if (item.is_directory())
		{
			for (const auto &entry : std::filesystem::directory_iterator(item.path()))
			{
				if (entry.is_regular_file() && entry.path().extension() == L".log")
					paths.push_back(entry.path());
			}
		}
	}
	const std::wregex logName(L"[0-9]{6}-[0-9]{6}-AsyncLoggerTests[.]log");
	if (paths.size() != 1 || !std::regex_match(paths.front().filename().wstring(), logName))
	{
		fputs("FAIL: expected one YYMMDD-HHMMSS-AsyncLoggerTests.log\n", stderr);
		return 1;
	}
	if (!CheckOutput(ReadBytes(paths.front()), ReadBytes(stdoutPath)))
	{
		fputs("FAIL: persisted FIFO, drops, independent filters or UTF-8 line contract\n", stderr);
		return 1;
	}
	fputs("PASS: blocked stdout, two producer FIFO, exact per-level drops, UTF-8, independent filters, stop drain\n", stderr);
	return 0;
}
