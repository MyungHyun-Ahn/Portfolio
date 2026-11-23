#include "NetworkCommonLibPch.h"
#include "MHLib/serialization/ISerializable.h"
#include "PacketTypes.g.h"
#include "Chatting/Chatting.g.h"
#include "../Chatting/Common/CommonProtocol.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
	using MHLib::security::CEncryption;
	using NetworkCommonLib::CSerializableBuffer;
	using NetworkCommonLib::SPacketHeader;
	using Clock = std::chrono::steady_clock;

	constexpr std::size_t kHeaderBytes = sizeof(SPacketHeader);
	constexpr std::size_t kChecksumOffset = offsetof(SPacketHeader, checkSum);
	static_assert(kChecksumOffset + 1 == kHeaderBytes);
	constexpr int kTrials = 21;
	constexpr int kWarmups = 3;

	enum class EPath
	{
		Encode,
		Legacy,
		Fused
	};

	bool BuildBatch(std::size_t count, std::vector<char> &outBatch)
	{
		constexpr std::size_t kMessageLengths[]{ 16, 64, 256 };
		for (std::size_t i = 0; i < count; ++i)
		{
			Chatting::SReqMessage packet;
			packet.m_accountNo = static_cast<std::int64_t>(i + 1);
			packet.m_message.assign(kMessageLengths[i % 3], L'가');
			packet.m_message.back() = static_cast<wchar_t>(L'A' + i % 26);
			auto spBuffer = CSerializableBuffer::MakeShared();
			if (!spBuffer)
				return false;
			*spBuffer << static_cast<std::uint16_t>(Chatting::EPacketType::ReqMessage) << packet;
			if (!spBuffer->IsValid())
				return false;

			char *pBody = spBuffer->GetContentBufferPtr();
			const auto bodyBytes = static_cast<USHORT>(spBuffer->GetDataSize());
			const SPacketHeader header{ Chatting::kPacketCode, bodyBytes, 0, CEncryption::CalCheckSum(pBody, bodyBytes) };
			const auto *pHeader = reinterpret_cast<const char *>(&header);
			outBatch.insert(outBatch.end(), pHeader, pHeader + kHeaderBytes);
			outBatch.insert(outBatch.end(), pBody, pBody + bodyBytes);
		}
		return true;
	}

	// 연속 프레임을 순서대로 걷되, 패킷 생성과 입출력은 측정하지 않는다.
	template<EPath kPath>
	__declspec(noinline) std::size_t ProcessBatch(std::vector<char> &batch) noexcept
	{
		std::size_t processed = 0;
		for (std::size_t offset = 0; offset < batch.size();)
		{
			SPacketHeader header;
			std::memcpy(&header, batch.data() + offset, kHeaderBytes);
			char *pChecksum = batch.data() + offset + kChecksumOffset;
			const int bytes = header.len + 1;
			if constexpr (kPath == EPath::Encode)
			{
				*pChecksum = static_cast<char>(CEncryption::CalCheckSum(pChecksum + 1, header.len));
				CEncryption::Encoding(pChecksum, bytes, header.randKey);
			}
			else if constexpr (kPath == EPath::Legacy)
			{
				CEncryption::Decoding(pChecksum, bytes, header.randKey);
				if (CEncryption::CalCheckSum(pChecksum + 1, header.len) != static_cast<BYTE>(*pChecksum))
					break;
			}
			else
			{
				if (!CEncryption::DecodeAndValidateChecksum(pChecksum, bytes, header.randKey))
					break;
			}
			offset += kHeaderBytes + header.len;
			++processed;
		}
		return processed;
	}

	template<EPath kPath>
	bool Measure(std::vector<char> &work, const std::vector<char> &input, const std::vector<char> &expected,
		std::size_t count, double &outNanoseconds)
	{
		work = input;
		const auto start = Clock::now();
		const auto processed = ProcessBatch<kPath>(work);
		const auto end = Clock::now();
		outNanoseconds = std::chrono::duration<double, std::nano>(end - start).count();
		return processed == count && work == expected;
	}

	double Median(std::array<double, kTrials> samples)
	{
		std::sort(samples.begin(), samples.end());
		return samples[samples.size() / 2];
	}

	bool Run(std::size_t count)
	{
		std::vector<char> plain;
		if (!BuildBatch(count, plain))
			return false;
		auto encoded = plain;
		if (ProcessBatch<EPath::Encode>(encoded) != count || encoded == plain)
			return false;

		std::vector<char> work(plain.size());
		std::array<double, kTrials> encode{};
		std::array<double, kTrials> legacy{};
		std::array<double, kTrials> fused{};
		for (int i = -kWarmups; i < kTrials; ++i)
		{
			double encodeNs = 0;
			double legacyNs = 0;
			double fusedNs = 0;
			if (!Measure<EPath::Encode>(work, plain, encoded, count, encodeNs))
				return false;
			const auto measureLegacy = [&]() { return Measure<EPath::Legacy>(work, encoded, plain, count, legacyNs); };
			const auto measureFused = [&]() { return Measure<EPath::Fused>(work, encoded, plain, count, fusedNs); };
			if (!(i % 2 == 0 ? measureLegacy() && measureFused() : measureFused() && measureLegacy()))
				return false;
			if (i >= 0)
			{
				encode[i] = encodeNs;
				legacy[i] = legacyNs;
				fused[i] = fusedNs;
			}
		}

		const double oldNs = Median(legacy);
		const double newNs = Median(fused);
		std::printf("%zu,%zu,%.2f,%.2f,%.2f,%+.2f%%\n", count, plain.size(), Median(encode) / count,
			oldNs / count, newNs / count, (1.0 - newNs / oldNs) * 100.0);
		return true;
	}
}

int main(void)
{
	CEncryption::sm_packetKey = Chatting::kPacketKey;
	std::printf("ReqMessage (16/64/256 UTF-16 units), %d warmups, median of %d trials\n", kWarmups, kTrials);
	std::printf("packets,wire_bytes,encode_ns_per_packet,legacy_receive_ns_per_packet,fused_receive_ns_per_packet,receive_time_reduction\n");
	for (const std::size_t count : { 256, 2048, 8192 })
	{
		if (!Run(count))
		{
			std::fprintf(stderr, "FAIL: frame creation or byte-for-byte verification (%zu packets)\n", count);
			return 1;
		}
	}
	std::puts("PASS: both receive paths restored the same complete frames in every trial");
	return 0;
}
