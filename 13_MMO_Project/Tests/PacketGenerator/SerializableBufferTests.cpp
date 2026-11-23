#include "NetworkCommonLibPch.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include "MHLib/serialization/ISerializable.h"
#include "CSerializableBuffer.h"
#include "../../Common/Packet/Struct/Chatting/Login.g.h"
#include "../../Common/Packet/Struct/Chatting/Chatting.g.h"
#include "../Chatting/Common/CommonProtocol.h"

#ifdef NDEBUG
#error SerializableBufferTests requires active assertions.
#endif

using CBuffer = NetworkCommonLib::CSerializableBuffer<false>;

void TestFixedCapacity()
{
	CBuffer buffer;
	std::array<char, 1018> prefix{};
	const char last = '\x42';
	assert(buffer.Enqueue(prefix.data(), static_cast<int>(prefix.size())));
	buffer << last;
	assert(buffer.IsValid() && buffer.GetFullSize() == buffer.GetBufferSize());
	assert(buffer.GetBufferPtr()[buffer.GetBufferSize() - 1] == last);
	buffer << std::uint16_t{0x1234};
	assert(!buffer.IsValid() && buffer.GetFullSize() == buffer.GetBufferSize());
	char value = '\x55';
	buffer >> value;
	assert(value == '\x55' && buffer.GetFullSize() == buffer.GetBufferSize());
	buffer.Clear();
	assert(buffer.IsValid() && buffer.GetDataSize() == 0);
	buffer << last;
	std::uint16_t word = 0xabcd;
	buffer >> word;
	assert(!buffer.IsValid() && word == 0xabcd && buffer.GetDataSize() == 1);
}

class CLeaf final : public MHLib::serialization::ISerializable<>
{
public:
	bool Serialize(CBuffer &buffer) const noexcept override { return (buffer << m_value).IsValid(); }
	bool Deserialize(CBuffer &buffer) noexcept override { return (buffer >> m_value).IsValid(); }
	std::uint16_t m_value = 0;
};

class CEnvelope final : public MHLib::serialization::ISerializable<>
{
public:
	bool Serialize(CBuffer &buffer) const noexcept override { return (buffer << m_leaf << m_tag).IsValid(); }
	bool Deserialize(CBuffer &buffer) noexcept override { return (buffer >> m_leaf >> m_tag).IsValid(); }
	CLeaf m_leaf;
	char m_tag = 0;
};

class CReject final : public MHLib::serialization::ISerializable<>
{
public:
	bool Serialize(CBuffer &) const noexcept override { return false; }
	bool Deserialize(CBuffer &) noexcept override { return false; }
};

void TestNestedSerialization()
{
	CBuffer buffer;
	CEnvelope input;
	input.m_leaf.m_value = 0x1234;
	input.m_tag = 'A';
	const std::array<unsigned char, 3> kWire{0x34, 0x12, 0x41};
	buffer << input;
	assert(buffer.GetDataSize() == kWire.size());
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWire.data(), kWire.size()) == 0);
	CEnvelope output;
	buffer >> output;
	assert(buffer.IsValid() && buffer.GetDataSize() == 0);
	assert(output.m_leaf.m_value == input.m_leaf.m_value && output.m_tag == input.m_tag);
	CReject reject;
	buffer << reject << input;
	assert(!buffer.IsValid() && buffer.GetDataSize() == 0);
	buffer.Clear();
	buffer << input;
	buffer >> reject >> output;
	assert(!buffer.IsValid() && buffer.GetDataSize() == kWire.size());
}

void TestPrimitiveWire()
{
	CBuffer buffer;
	buffer << std::int8_t{-2} << std::uint8_t{0x12} << std::int16_t{-3} << std::uint16_t{0x1234}
		<< std::int32_t{-4} << std::uint32_t{0x12345678} << std::int64_t{-5} << std::uint64_t{0x0102030405060708};
	constexpr unsigned char kWire[]{
		0xfe, 0x12, 0xfd, 0xff, 0x34, 0x12, 0xfc, 0xff, 0xff, 0xff, 0x78, 0x56, 0x34, 0x12,
		0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
	assert(buffer.IsValid() && buffer.GetDataSize() == sizeof(kWire));
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWire, sizeof(kWire)) == 0);
	std::int8_t signed8 = 0;
	std::uint8_t unsigned8 = 0;
	std::int16_t signed16 = 0;
	std::uint16_t unsigned16 = 0;
	std::int32_t signed32 = 0;
	std::uint32_t unsigned32 = 0;
	std::int64_t signed64 = 0;
	std::uint64_t unsigned64 = 0;
	buffer >> signed8 >> unsigned8 >> signed16 >> unsigned16 >> signed32 >> unsigned32 >> signed64 >> unsigned64;
	assert(buffer.IsValid() && buffer.GetDataSize() == 0);
	assert(signed8 == -2 && unsigned8 == 0x12 && signed16 == -3 && unsigned16 == 0x1234);
	assert(signed32 == -4 && unsigned32 == 0x12345678 && signed64 == -5 && unsigned64 == 0x0102030405060708);
}

void TestFixedArrays()
{
	CBuffer buffer;
	const std::array<char, 3> narrow{'Q', '\0', 'Z'};
	const std::array<wchar_t, 2> wide{L'\uac00', 0};
	constexpr unsigned char kWire[]{0x51, 0, 0x5a, 0, 0xac, 0, 0};
	buffer << narrow << wide;
	assert(buffer.IsValid() && buffer.GetDataSize() == sizeof(kWire));
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWire, sizeof(kWire)) == 0);
	std::array<char, 3> narrowOut{};
	std::array<wchar_t, 2> wideOut{};
	buffer >> narrowOut >> wideOut;
	assert(buffer.IsValid() && narrowOut == narrow && wideOut == wide && buffer.GetDataSize() == 0);
	buffer.Clear();
	buffer << narrow;
	buffer >> wideOut;
	assert(!buffer.IsValid() && wideOut == wide && buffer.GetDataSize() == narrow.size());
}

void TestUtf16StringWire()
{
	CBuffer buffer;
	const std::wstring input{L'\uac00', L'\0', wchar_t{0xd83d}, wchar_t{0xde00}};
	constexpr unsigned char kWire[]{4, 0, 0, 0, 0, 0xac, 0, 0, 0x3d, 0xd8, 0, 0xde, 0, 0, 0, 0};
	buffer << input << std::wstring{};
	assert(buffer.IsValid() && buffer.GetDataSize() == sizeof(kWire));
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWire, sizeof(kWire)) == 0);
	std::wstring output;
	buffer >> output;
	assert(buffer.IsValid() && output == input);
	buffer >> output;
	assert(buffer.IsValid() && output.empty() && buffer.GetDataSize() == 0);
	// 버퍼는 코드 단위를 복사하고 텍스트 정책을 적용하지 않는다.
	const std::wstring codeUnits{wchar_t{0xd800}};
	buffer << codeUnits;
	buffer >> output;
	assert(buffer.IsValid() && output == codeUnits && buffer.GetDataSize() == 0);
}

void TestInvalidUtf16StringPreservesField()
{
	CBuffer buffer;
	std::wstring output = L"unchanged";
	buffer << std::uint16_t{1};
	buffer >> output;
	assert(!buffer.IsValid() && output == L"unchanged" && buffer.GetDataSize() == 2);
	buffer.Clear();
	buffer << std::uint32_t{(std::numeric_limits<std::uint32_t>::max)()} << std::uint16_t{0x41};
	buffer >> output;
	assert(!buffer.IsValid() && output == L"unchanged" && buffer.GetDataSize() == 6);
	buffer.Clear();
	buffer << std::uint32_t{2} << std::uint16_t{0x41};
	buffer >> output;
	assert(!buffer.IsValid() && output == L"unchanged" && buffer.GetDataSize() == 6);
	buffer.Clear();
	buffer << std::wstring(508, L'A');
	assert(!buffer.IsValid() && buffer.GetDataSize() == 0);
}

void TestChatNameBounds()
{
	assert(Chatting::IsValidString(L"AB", 2));
	assert(!Chatting::IsValidString(L"AB", 1));
	assert(Chatting::IsValidString(L"\uac00", Chatting::kMaxNameCodeUnits));
	assert(Chatting::IsValidString(std::wstring(17, L'\ub098') + L"\U0001f600", Chatting::kMaxNameCodeUnits));
	assert(!Chatting::IsValidString(L"", Chatting::kMaxNameCodeUnits));
	assert(!Chatting::IsValidString(std::wstring(18, L'\ub098') + L"\U0001f600", Chatting::kMaxNameCodeUnits));
	assert(!Chatting::IsValidString(std::wstring{L'A', L'\0', L'B'}, Chatting::kMaxNameCodeUnits));
}

void TestChatNameWire()
{
	constexpr unsigned char kWirePrefix[]{
		1, 0, 0, 0, 0, 0, 0, 0, // accountNo
		1, 0, 0, 0, 0x41, 0, // id: A
		3, 0, 0, 0, 0, 0xac, 0x3d, 0xd8, 0, 0xde // nickname: 가😀
	};
	CBuffer buffer;
	Chatting::SReqLogin login;
	login.m_accountNo = 1;
	login.m_id = L"A";
	login.m_nickname = L"\uac00\U0001f600";
	login.m_sessionKey.fill('k');
	assert(login.Serialize(buffer));
	assert(static_cast<std::size_t>(buffer.GetDataSize()) == sizeof(kWirePrefix) + login.m_sessionKey.size());
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWirePrefix, sizeof(kWirePrefix)) == 0);
	assert(std::memcmp(buffer.GetContentBufferPtr() + sizeof(kWirePrefix), login.m_sessionKey.data(), login.m_sessionKey.size()) == 0);
	Chatting::SReqLogin receivedLogin;
	assert(receivedLogin.Deserialize(buffer) && buffer.GetDataSize() == 0);
	assert(receivedLogin.m_accountNo == 1 && receivedLogin.m_id == login.m_id && receivedLogin.m_nickname == login.m_nickname);
	assert(receivedLogin.m_sessionKey == login.m_sessionKey);

	buffer.Clear();
	Chatting::SResMessage chat;
	chat.m_accountNo = 1;
	chat.m_id = login.m_id;
	chat.m_nickname = login.m_nickname;
	chat.m_message = L"B";
	constexpr unsigned char kMessage[]{1, 0, 0, 0, 0x42, 0};
	assert(chat.Serialize(buffer));
	assert(static_cast<std::size_t>(buffer.GetDataSize()) == sizeof(kWirePrefix) + sizeof(kMessage));
	assert(std::memcmp(buffer.GetContentBufferPtr(), kWirePrefix, sizeof(kWirePrefix)) == 0);
	assert(std::memcmp(buffer.GetContentBufferPtr() + sizeof(kWirePrefix), kMessage, sizeof(kMessage)) == 0);
	Chatting::SResMessage receivedChat;
	assert(receivedChat.Deserialize(buffer) && buffer.GetDataSize() == 0);
	assert(receivedChat.m_accountNo == 1 && receivedChat.m_id == chat.m_id && receivedChat.m_nickname == chat.m_nickname);
	assert(receivedChat.m_message == chat.m_message);

	buffer.Clear();
	buffer << std::int64_t{1} << login.m_id << std::uint32_t{3} << std::uint16_t{0xac00};
	assert(!receivedChat.Deserialize(buffer) && !buffer.IsValid());
}

void TestPoolReset()
{
	const LONG usage = CBuffer::GetPoolUsage();
	CBuffer *pBuffer = CBuffer::Alloc();
	assert(pBuffer->IsValid() && pBuffer->GetDataSize() == 0);
	pBuffer->MoveReadPos(1);
	assert(!pBuffer->IsValid());
	CBuffer::Free(pBuffer);
	pBuffer = CBuffer::Alloc();
	assert(pBuffer->IsValid() && pBuffer->GetDataSize() == 0 && pBuffer->GetIsEnqueueHeader() == FALSE);
	CBuffer::Free(pBuffer);
	assert(CBuffer::GetPoolUsage() == usage);
}

void TestRawBufferBoundaries()
{
	NetworkCommonLib::CSerializableBuffer<true> buffer;
	char header[]{1, 2};
	char replacement[]{3, 4};
	assert(buffer.GetHeaderSize() == 2 && buffer.GetBufferSize() == 128);
	assert(buffer.EnqueueHeader(header, sizeof(header)));
	assert(buffer.EnqueueHeader(replacement, sizeof(replacement)));
	assert(std::memcmp(buffer.GetBufferPtr(), header, sizeof(header)) == 0);
	assert(buffer.Enqueue(nullptr, 0) && buffer.Dequeue(nullptr, 0));
	assert(!buffer.Enqueue(nullptr, 1) && !buffer.IsValid() && buffer.GetDataSize() == 0);
	buffer.Clear();
	assert(buffer.Enqueue(header, sizeof(header)));
	assert(!buffer.Dequeue(replacement, -1) && !buffer.IsValid() && buffer.GetDataSize() == 2);
	assert(replacement[0] == 3 && replacement[1] == 4);
	buffer.Clear();
	assert(buffer.MoveWritePos(126) == 128 && buffer.MoveReadPos(126) == 128);
	assert(buffer.IsValid() && buffer.GetDataSize() == 0);
	assert(buffer.MoveWritePos((std::numeric_limits<int>::max)()) == 128 && !buffer.IsValid());
}

int main(void)
{
	TestFixedCapacity();
	TestNestedSerialization();
	TestPrimitiveWire();
	TestFixedArrays();
	TestUtf16StringWire();
	TestInvalidUtf16StringPreservesField();
	TestChatNameBounds();
	TestChatNameWire();
	TestPoolReset();
	TestRawBufferBoundaries();
}
