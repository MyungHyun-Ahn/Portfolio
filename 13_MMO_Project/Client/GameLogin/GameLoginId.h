#pragma once

namespace Game
{
	inline constexpr size_t kMaxLoginIdUnits = 20;

	inline bool IsValidLoginId(std::wstring_view id) noexcept
	{
		if (id.empty() || id.size() > kMaxLoginIdUnits)
			return false;
		for (wchar_t digit : id)
			if (digit < L'0' || digit > L'9')
				return false;
		return true;
	}
}
