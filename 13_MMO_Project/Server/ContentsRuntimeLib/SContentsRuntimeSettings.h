#pragma once

namespace MHLib::utils
{
	class CFileLoader;
}

namespace ContentsRuntimeLib
{
	struct SContentsRuntimeSettings
	{
		std::size_t workerCount;
		std::chrono::milliseconds frameInterval;

		// ContentsRuntime 섹션을 검증하고 FPS를 프레임 간격으로 환산한다.
		[[nodiscard]]
		static bool Load(MHLib::utils::CFileLoader &loader, SContentsRuntimeSettings &outSettings, const wchar_t *&outError);
	};
}
