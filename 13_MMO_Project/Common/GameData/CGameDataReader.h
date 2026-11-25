#pragma once

namespace GameData
{
	enum class EGameDataReadResult
	{
		NotRead,
		Success,
		OpenFailed,
		ReadFailed,
		InvalidMagic,
		UnsupportedVersion,
		WrongTarget,
		WrongTable,
		WrongSchema,
		Truncated,
		TrailingBytes,
		InvalidPrimaryKey,
		DuplicatePrimaryKey,
		InvalidValue,
	};

	struct SGameDataFormat
	{
		std::uint16_t m_version;
		std::uint8_t m_target;
		std::string_view m_tableName;
		const std::uint8_t (&m_schemaHash)[32];
	};

	class CGameDataReader
	{
	public:
		static constexpr std::uint16_t kSupportedFormatVersion = 1;

		// 파일이 끝까지 정상일 때만 타입별 행과 PK 인덱스를 교체한다.
		template<typename TRow>
		[[nodiscard]]
		static EGameDataReadResult Read(const std::filesystem::path& path, const SGameDataFormat& format,
			bool (*pReadRow)(CGameDataReader&, TRow&), std::uint32_t TRow::* pKey, std::unordered_map<std::uint32_t, TRow>& outRows)
		{
			CGameDataReader reader;
			std::uint32_t rowCount = 0;
			if (!reader.open(path) || !reader.readHeader(format, rowCount))
			{
				return reader.m_result;
			}
			std::unordered_map<std::uint32_t, TRow> rows;
			for (std::uint32_t index = 0; index < rowCount; ++index)
			{
				TRow row{};
				if (!pReadRow(reader, row))
				{
					return reader.m_result == EGameDataReadResult::Success ? EGameDataReadResult::InvalidValue : reader.m_result;
				}
				const std::uint32_t key = row.*pKey;
				if (key == 0)
				{
					return EGameDataReadResult::InvalidPrimaryKey;
				}
				if (!rows.try_emplace(key, std::move(row)).second)
				{
					return EGameDataReadResult::DuplicatePrimaryKey;
				}
			}
			if (!reader.atEnd())
			{
				return reader.m_result;
			}
			outRows.swap(rows);
			return EGameDataReadResult::Success;
		}

		[[nodiscard]]
		bool ReadValue(std::uint32_t& outValue);
		[[nodiscard]]
		bool ReadValue(std::int32_t& outValue);
		[[nodiscard]]
		bool ReadValue(float& outValue);
		[[nodiscard]]
		bool ReadValue(std::string& outValue);

		// 공용 Enum의 저장 형식은 uint32다.
		template<typename TEnum> requires std::is_enum_v<TEnum>
		[[nodiscard]]
		bool ReadValue(TEnum& outValue)
		{
			static_assert(std::is_same_v<std::underlying_type_t<TEnum>, std::uint32_t>);
			std::uint32_t value = 0;
			if (!ReadValue(value))
			{
				return false;
			}
			outValue = static_cast<TEnum>(value);
			return true;
		}

		// 미입력과 존재하는 값(빈 배열 포함)을 구분한다.
		template<typename T>
		[[nodiscard]]
		bool ReadValue(std::optional<T>& outValue)
		{
			std::uint8_t present = 0;
			if (!readUnsigned(present))
			{
				return false;
			}
			if (present == 0)
			{
				outValue.reset();
				return true;
			}
			if (present != 1)
			{
				return fail(EGameDataReadResult::InvalidValue);
			}
			return ReadValue(outValue.emplace());
		}

		// 모든 지원 원소는 최소 4바이트이며 파일 크기로 개수를 제한한다.
		template<typename T>
		[[nodiscard]]
		bool ReadValue(std::vector<T>& outValue)
		{
			std::uint32_t count = 0;
			if (!ReadValue(count))
			{
				return false;
			}
			if (count > m_remainingBytes / sizeof(std::uint32_t))
			{
				return fail(EGameDataReadResult::Truncated);
			}
			outValue.clear();
			for (std::uint32_t index = 0; index < count; ++index)
			{
				if (!ReadValue(outValue.emplace_back()))
				{
					return false;
				}
			}
			return true;
		}

	private:
		CGameDataReader() = default;

		[[nodiscard]]
		bool open(const std::filesystem::path& path);
		[[nodiscard]]
		bool readHeader(const SGameDataFormat& format, std::uint32_t& outRowCount);
		[[nodiscard]]
		bool readBytes(char* pOutBytes, std::size_t count);
		[[nodiscard]]
		bool atEnd();
		[[nodiscard]]
		bool fail(EGameDataReadResult result) noexcept;

		// 객체의 메모리 배치와 무관하게 little-endian 값을 복원한다.
		template<typename T>
		[[nodiscard]]
		bool readUnsigned(T& outValue)
		{
			char bytes[sizeof(T)]{};
			if (!readBytes(bytes, sizeof(bytes)))
			{
				return false;
			}
			std::uint32_t value = 0;
			for (unsigned int index = 0; index < sizeof(bytes); ++index)
			{
				value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index])) << (index * 8);
			}
			outValue = static_cast<T>(value);
			return true;
		}

	private:
		std::ifstream m_stream;
		std::uint64_t m_remainingBytes = 0;
		EGameDataReadResult m_result = EGameDataReadResult::Success;
	};
}