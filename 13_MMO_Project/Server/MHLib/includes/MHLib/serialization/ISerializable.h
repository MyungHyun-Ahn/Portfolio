#pragma once

namespace NetworkCommonLib
{
	template<bool isLanServer> class CSerializableBuffer;
}

namespace MHLib::serialization
{
	template<bool isLanServer = false>
	class ISerializable
	{
	public:
		virtual ~ISerializable() = default;
		[[nodiscard]] virtual bool Serialize(NetworkCommonLib::CSerializableBuffer<isLanServer> &buffer) const noexcept = 0;
		[[nodiscard]] virtual bool Deserialize(NetworkCommonLib::CSerializableBuffer<isLanServer> &buffer) noexcept = 0;
	};
}
