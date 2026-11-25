#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CGameDataReader.h"

namespace GameData
{
	namespace
	{
		// UTF-8 연속 바이트와 코드 포인트 범위를 검사한다.
		[[nodiscard]]
		bool IsValidUtf8(std::string_view text) noexcept
		{
			while (!text.empty())
			{
				const auto first = static_cast<unsigned char>(text.front());
				if (first < 0x80)
				{
					text.remove_prefix(1);
					continue;
				}
				if (first < 0xc2 || first > 0xf4)
				{
					return false;
				}
				const std::size_t width = first < 0xe0 ? 2 : (first < 0xf0 ? 3 : 4);
				if (text.size() < width)
				{
					return false;
				}
				const auto second = static_cast<unsigned char>(text[1]);
				if ((first == 0xe0 && second < 0xa0) || (first == 0xed && second > 0x9f) ||
					(first == 0xf0 && second < 0x90) || (first == 0xf4 && second > 0x8f))
				{
					return false;
				}
				for (std::size_t index = 1; index < width; ++index)
				{
					if ((static_cast<unsigned char>(text[index]) & 0xc0) != 0x80)
					{
						return false;
					}
				}
				text.remove_prefix(width);
			}
			return true;
		}
	}

	bool CGameDataReader::open(const std::filesystem::path& path)
	{
		m_stream.open(path, std::ios::binary | std::ios::ate);
		if (!m_stream)
		{
			return fail(EGameDataReadResult::OpenFailed);
		}
		const auto size = m_stream.tellg();
		if (size < 0 || !m_stream.seekg(0))
		{
			return fail(EGameDataReadResult::ReadFailed);
		}
		m_remainingBytes = static_cast<std::uint64_t>(size);
		return true;
	}

	bool CGameDataReader::readHeader(const SGameDataFormat& format, std::uint32_t& outRowCount)
	{
		char magic[4]{};
		if (!readBytes(magic, sizeof(magic)))
		{
			return false;
		}
		if (std::string_view(magic, sizeof(magic)) != "GDAT")
		{
			return fail(EGameDataReadResult::InvalidMagic);
		}
		std::uint16_t version = 0;
		if (!readUnsigned(version))
		{
			return false;
		}
		if (version != kSupportedFormatVersion || version != format.m_version)
		{
			return fail(EGameDataReadResult::UnsupportedVersion);
		}
		std::uint8_t target = 0;
		if (!readUnsigned(target))
		{
			return false;
		}
		if (target != format.m_target)
		{
			return fail(EGameDataReadResult::WrongTarget);
		}
		std::uint32_t tableNameBytes = 0;
		if (!ReadValue(tableNameBytes))
		{
			return false;
		}
		if (tableNameBytes != format.m_tableName.size())
		{
			return fail(EGameDataReadResult::WrongTable);
		}
		for (char expected : format.m_tableName)
		{
			char actual = '\0';
			if (!readBytes(&actual, 1))
			{
				return false;
			}
			if (actual != expected)
			{
				return fail(EGameDataReadResult::WrongTable);
			}
		}
		char hash[sizeof(format.m_schemaHash)]{};
		if (!readBytes(hash, sizeof(hash)))
		{
			return false;
		}
		if (std::memcmp(hash, format.m_schemaHash, sizeof(hash)) != 0)
		{
			return fail(EGameDataReadResult::WrongSchema);
		}
		return ReadValue(outRowCount);
	}

	bool CGameDataReader::readBytes(char* pOutBytes, std::size_t count)
	{
		if (count > m_remainingBytes)
		{
			return fail(EGameDataReadResult::Truncated);
		}
		if (!m_stream.read(pOutBytes, static_cast<std::streamsize>(count)))
		{
			return fail(m_stream.eof() && !m_stream.bad() ? EGameDataReadResult::Truncated : EGameDataReadResult::ReadFailed);
		}
		m_remainingBytes -= count;
		return true;
	}

	bool CGameDataReader::atEnd()
	{
		if (m_remainingBytes != 0 || m_stream.peek() != std::char_traits<char>::eof())
		{
			return fail(EGameDataReadResult::TrailingBytes);
		}
		if (m_stream.bad() || !m_stream.eof())
		{
			return fail(EGameDataReadResult::ReadFailed);
		}
		return true;
	}

	bool CGameDataReader::fail(EGameDataReadResult result) noexcept
	{
		m_result = result;
		return false;
	}

	bool CGameDataReader::ReadValue(std::uint32_t& outValue)
	{
		return readUnsigned(outValue);
	}

	bool CGameDataReader::ReadValue(std::int32_t& outValue)
	{
		std::uint32_t bits = 0;
		if (!ReadValue(bits))
		{
			return false;
		}
		outValue = std::bit_cast<std::int32_t>(bits);
		return true;
	}

	bool CGameDataReader::ReadValue(float& outValue)
	{
		std::uint32_t bits = 0;
		if (!ReadValue(bits))
		{
			return false;
		}
		outValue = std::bit_cast<float>(bits);
		return std::isfinite(outValue) || fail(EGameDataReadResult::InvalidValue);
	}

	bool CGameDataReader::ReadValue(std::string& outValue)
	{
		std::uint32_t length = 0;
		if (!ReadValue(length))
		{
			return false;
		}
		if (length > m_remainingBytes)
		{
			return fail(EGameDataReadResult::Truncated);
		}
		outValue.resize(length);
		if (!readBytes(outValue.data(), length))
		{
			return false;
		}
		return IsValidUtf8(outValue) || fail(EGameDataReadResult::InvalidValue);
	}
}