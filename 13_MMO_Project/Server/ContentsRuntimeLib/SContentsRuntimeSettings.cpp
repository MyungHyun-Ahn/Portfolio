#include "ContentsRuntimeLibPch.h"
#include <windows.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <share.h>
#include <string>

// 기존 로더의 C4244 억제를 이 파일의 설정 코드로 전파하지 않는다.
#pragma warning(push)
#include "MHLib/utils/CFileLoader.h"
#pragma warning(pop)
#include "SContentsRuntimeSettings.h"

namespace ContentsRuntimeLib
{
	bool SContentsRuntimeSettings::Load(MHLib::utils::CFileLoader &loader, SContentsRuntimeSettings &outSettings,
		const wchar_t *&outError)
	{
		constexpr const wchar_t *kSection = L"ContentsRuntime";
		std::wstring misplaced;
		outError = L"CONTENTS_WORKER_COUNT belongs in ContentsRuntime:";
		if (loader.Load(L"Server", L"CONTENTS_WORKER_COUNT", &misplaced))
			return false;
		outError = L"CONTENTS_FPS belongs in ContentsRuntime:";
		if (loader.Load(L"Server", L"CONTENTS_FPS", &misplaced))
			return false;

		INT64 workerCount = 0;
		outError = L"ContentsRuntime: CONTENTS_WORKER_COUNT must be an integer (1..64)";
		if (!loader.Load(kSection, L"CONTENTS_WORKER_COUNT", &workerCount) || workerCount < 1 || workerCount > 64)
			return false;

		INT64 fps = 0;
		outError = L"ContentsRuntime: CONTENTS_FPS must be an integer (1..1000)";
		if (!loader.Load(kSection, L"CONTENTS_FPS", &fps) || fps < 1 || fps > 1000)
			return false;

		outSettings.workerCount = static_cast<std::size_t>(workerCount);
		outSettings.frameInterval = std::chrono::milliseconds{1000 / fps};
		outError = nullptr;
		return true;
	}
}
