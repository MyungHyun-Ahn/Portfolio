#pragma once

namespace NetworkLib
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
		[[nodiscard]] virtual bool Serialize(NetworkLib::CSerializableBuffer<isLanServer> &buffer) const noexcept = 0;
		[[nodiscard]] virtual bool Deserialize(NetworkLib::CSerializableBuffer<isLanServer> &buffer) noexcept = 0;
	};
}
