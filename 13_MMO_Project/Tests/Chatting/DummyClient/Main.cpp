#include "pch.h"
#include <filesystem>
#include "SDummyConfig.h"
#include "SDummyRunStats.h"
#include "CDummyRunner.h"

int main(void)
{
	const auto processStart = std::chrono::steady_clock::now();
	MHLib::utils::g_pLogger = MHLib::utils::CLogger::GetInstance();
	Chatting::CDummyRunner runner;
	const int exitCode = runner.Run(processStart);
	fflush(nullptr);

	// 기존 ChatClient와 같은 정적 풀 종료 경계. 모든 클라이언트 IO와 결과 파일은 위에서 정리한다.
	ExitProcess(exitCode);
}
