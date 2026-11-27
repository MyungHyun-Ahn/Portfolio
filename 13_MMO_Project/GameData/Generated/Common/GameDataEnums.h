#pragma once

namespace GameData
{
	enum class EResultCode : std::uint32_t
	{
		Success = 0u,
		Rejected = 1u,
	};

	enum class MapType : std::uint32_t
	{
		Town = 1u,
	};
}
