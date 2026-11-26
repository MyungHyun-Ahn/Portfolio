#pragma once

namespace MHLib::utils
{
	class CFileLoader;
}

namespace MetricsLib
{
	struct SMetricsSettings
	{
		std::string bindAddress;
		std::uint16_t port;

		// 메트릭 주소와 포트를 검증한다. 포트 0은 비활성을 뜻한다.
		[[nodiscard]]
		static bool Load(MHLib::utils::CFileLoader &loader, SMetricsSettings &outSettings, const wchar_t *&outError);
	};
}
