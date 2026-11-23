#pragma once

namespace NetworkCommonLib
{
	class CSerializableBuffer;
}

namespace MHLib::serialization
{
	class ISerializable
	{
	public:
		virtual ~ISerializable() = default;
		[[nodiscard]] virtual bool Serialize(NetworkCommonLib::CSerializableBuffer &buffer) const noexcept = 0;
		[[nodiscard]] virtual bool Deserialize(NetworkCommonLib::CSerializableBuffer &buffer) noexcept = 0;
	};
}
