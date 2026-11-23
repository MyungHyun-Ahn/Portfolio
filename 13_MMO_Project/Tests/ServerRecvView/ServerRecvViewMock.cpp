#include "ServerNetworkLibPch.h"
#include "SNetworkCounters.h"
#include <chrono>
#include <cstdlib>
#include <latch>
#include <span>
#include <vector>

namespace ServerNetworkLib
{
	// 소켓 없이 실제 세션의 수신 완료 로직을 호출한다.
	class CRecvViewMockSession
	{
	public:
		CRecvBuffer &Prepare() noexcept
		{
			m_session.m_pRecvBuffer = CRecvBuffer::Alloc();
			m_session.m_pRecvBuffer->IncreaseRef();
			return *m_session.m_pRecvBuffer;
		}

		bool Complete(int size, BYTE packetCode, EPacketMode mode) noexcept
		{
			return m_session.RecvCompleted(size, packetCode, mode);
		}

		bool Dequeue(CSerializableBufferView **outPacket) noexcept
		{
			return m_session.m_recvMsgQueue.Dequeue(outPacket);
		}

		void Clear() noexcept
		{
			if (m_session.m_pRecvBuffer != nullptr && m_session.m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_session.m_pRecvBuffer);
			m_session.m_pRecvBuffer = nullptr;

			if (m_session.m_pDelayedBuffer != nullptr)
			{
				CSerializableBufferView::Free(m_session.m_pDelayedBuffer);
				m_session.m_pDelayedBuffer = nullptr;
			}
		}

	private:
		CSession m_session;
	};
}

namespace
{
	using namespace NetworkCommonLib;
	using ServerNetworkLib::CRecvViewMockSession;

	constexpr BYTE kPacketCode = 119;
	constexpr int kHeaderBytes = sizeof(SPacketHeader);
	constexpr int kChecksumOffset = offsetof(SPacketHeader, checkSum);
	constexpr int kMaxBodyBytes = static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize) - kHeaderBytes;
	constexpr int kFramesPerGroup = 4;
	constexpr int kBenchmarkPasses = 12;

	void Require(bool isOk, const char *pMessage, int producer = -1, int sequence = -1)
	{
		if (isOk)
			return;
		CLogger::WriteEmergencyLogToStderr("FAIL: %s (producer=%d sequence=%d)\n", pMessage, producer, sequence);
		std::_Exit(EXIT_FAILURE);
	}

	struct SPoolUsage
	{
		LONG m_recvPages;
		LONG m_views;
		LONG m_serializable;

		bool operator==(const SPoolUsage &) const = default;
	};

	SPoolUsage GetPoolUsage()
	{
		return { CRecvBuffer::GetPoolUsage(), CSerializableBufferView::GetPoolUsage(), CSerializableBuffer::GetPoolUsage() };
	}

	struct SFrame
	{
		std::vector<char> m_body;
		std::vector<char> m_wire;
	};

	SFrame MakeFrame(int producer, int sequence, EPacketMode mode, bool hasBadChecksum = false)
	{
		SFrame frame;
		const int bodyBytes = sequence % 16 == 15 ? kMaxBodyBytes : 16 + (producer * 13 + sequence * 17) % 96;
		frame.m_body.resize(bodyBytes);
		std::memcpy(frame.m_body.data(), &producer, sizeof(producer));
		std::memcpy(frame.m_body.data() + sizeof(producer), &sequence, sizeof(sequence));
		for (int i = 2 * sizeof(int); i < bodyBytes; ++i)
			frame.m_body[i] = static_cast<char>(i * 31 + producer * 7 + sequence);

		SPacketHeader header{ kPacketCode, static_cast<USHORT>(bodyBytes), 0, 0 };
		if (mode == EPacketMode::Net)
		{
			header.randKey = static_cast<BYTE>(producer * 11 + sequence + 73);
			header.checkSum = CEncryption::CalCheckSum(frame.m_body.data(), bodyBytes);
			// 암호화는 정상적으로 하되 평문의 체크섬만 틀리게 만든다.
			if (hasBadChecksum)
				header.checkSum ^= 1;
		}
		frame.m_wire.resize(kHeaderBytes + bodyBytes);
		std::memcpy(frame.m_wire.data(), &header, kHeaderBytes);
		std::memcpy(frame.m_wire.data() + kHeaderBytes, frame.m_body.data(), bodyBytes);
		if (mode == EPacketMode::Net)
			CEncryption::Encoding(frame.m_wire.data() + kChecksumOffset, bodyBytes + 1, header.randKey);
		return frame;
	}

	void Append(std::vector<char> &outBytes, std::span<const char> bytes)
	{
		outBytes.insert(outBytes.end(), bytes.begin(), bytes.end());
	}

	struct SGroup
	{
		std::array<SFrame, kFramesPerGroup> m_frames;
		std::array<std::vector<char>, 4> m_completions;
	};

	SGroup MakeGroup(int producer, int sequence, EPacketMode mode, bool isPacked = false)
	{
		SGroup group;
		for (int i = 0; i < kFramesPerGroup; ++i)
			group.m_frames[i] = MakeFrame(producer, sequence + i, mode);
		if (isPacked)
		{
			for (const auto &frame : group.m_frames)
				Append(group.m_completions[0], frame.m_wire);
			return group;
		}
		const std::span<const char> split(group.m_frames[2].m_wire);
		// 완성 프레임 2개 뒤에서 헤더 시작, 다음 2회에 헤더/본문 일부, 4회째에 완료.
		Append(group.m_completions[0], group.m_frames[0].m_wire);
		Append(group.m_completions[0], group.m_frames[1].m_wire);
		Append(group.m_completions[0], split.first(2));
		Append(group.m_completions[1], split.subspan(2, kHeaderBytes - 2));
		Append(group.m_completions[2], split.subspan(kHeaderBytes, 7));
		Append(group.m_completions[3], split.subspan(kHeaderBytes + 7));
		Append(group.m_completions[3], group.m_frames[3].m_wire);
		return group;
	}

	// 기존 CSession::RecvCompleted의 ring -> TLS 직렬화 버퍼 경로만 재현한다.
	class CLegacyRecv
	{
	public:
		bool Complete(std::span<const char> bytes, EPacketMode mode)
		{
			const auto completedBytes = static_cast<int>(bytes.size());
			Require(m_ring.GetFreeSize() >= completedBytes, "legacy ring has insufficient space");
			const int firstBytes = (std::min)(completedBytes, m_ring.DirectEnqueueSize());
			std::memcpy(m_ring.GetRearPtr(), bytes.data(), firstBytes);
			if (firstBytes != completedBytes)
				std::memcpy(m_ring.GetPQueuePtr(), bytes.data() + firstBytes, completedBytes - firstBytes);
			m_ring.MoveRear(completedBytes);
			while (m_ring.GetUseSize() >= kHeaderBytes)
			{
				SPacketHeader header;
				m_ring.Peek(reinterpret_cast<char *>(&header), kHeaderBytes);
				if (header.code != kPacketCode || header.len > kMaxBodyBytes || header.len >= m_ring.GetCapacity())
					return false;
				if (m_ring.GetUseSize() < kHeaderBytes + header.len)
					break;
				auto *pBuffer = CSerializableBuffer::Alloc();
				m_ring.MoveFront(kHeaderBytes);
				Require(pBuffer->EnqueueHeader(reinterpret_cast<char *>(&header), kHeaderBytes), "legacy header copy");
				Require(m_ring.Dequeue(pBuffer->GetContentBufferPtr(), header.len) == header.len, "legacy body copy");
				pBuffer->MoveWritePos(header.len);
				if (mode == EPacketMode::Net && !CEncryption::DecodeAndValidateChecksum(
					pBuffer->GetBufferPtr() + kChecksumOffset, header.len + 1, header.randKey))
				{
					CSerializableBuffer::Free(pBuffer);
					return false;
				}
				pBuffer->IncreaseRef();
				InterlockedIncrement(&g_networkCounters.m_recvTps);
				m_recvMsgQueue.Enqueue(pBuffer);
			}
			return true;
		}

		bool Dequeue(CSerializableBuffer **outPacket) noexcept
		{
			return m_recvMsgQueue.Dequeue(outPacket);
		}

		void Clear() noexcept { m_ring.Clear(); }

	private:
		CRingBuffer m_ring{ 4096 };
		CLfQueue<CSerializableBuffer *> m_recvMsgQueue;
	};

	template<typename TPacket>
	void ReleasePacket(TPacket *pPacket)
	{
		if constexpr (std::is_same_v<TPacket, CSerializableBuffer>)
		{
			Require(pPacket->DecreaseRef() == 0, "legacy queue should own one reference");
			CSerializableBuffer::Free(pPacket);
		}
		else
			CSerializableBufferView::Free(pPacket);
	}

	template<typename TStream>
	bool Complete(TStream &stream, std::span<const char> bytes, EPacketMode mode)
	{
		if constexpr (std::is_same_v<TStream, CRecvViewMockSession>)
		{
			auto &page = stream.Prepare();
			Require(bytes.size() <= static_cast<std::size_t>(page.GetCapacity()), "completion exceeds page capacity");
			char *pPage = page.GetPQueuePtr();
			std::memcpy(pPage, bytes.data(), bytes.size());
			return stream.Complete(static_cast<int>(bytes.size()), kPacketCode, mode);
		}
		else
			return stream.Complete(bytes, mode);
	}

	void CheckPayload(std::span<const char> bytes, const SFrame &frame, int producer, int sequence)
	{
		Require(bytes.size() == frame.m_body.size(), "body length mismatch", producer, sequence);
		Require(std::memcmp(bytes.data(), frame.m_body.data(), bytes.size()) == 0, "body content mismatch", producer, sequence);
	}

	void CheckPayload(auto &packet, const SFrame &frame, int producer, int sequence)
	{
		std::array<char, kMaxBodyBytes> body;
		const int bytes = packet.GetDataSize();
		Require(bytes >= 0 && bytes <= kMaxBodyBytes && packet.Dequeue(body.data(), bytes), "body read failed", producer, sequence);
		CheckPayload(std::span<const char>(body.data(), bytes), frame, producer, sequence);
	}

	void CheckPageReset()
	{
		const auto baseline = CRecvBuffer::GetPoolUsage();
		auto spPage = MHLib::utils::CSmartPtr<CRecvBuffer>::MakeShared();
		spPage.Reset();
		Require(spPage.GetRealPtr() == nullptr && CRecvBuffer::GetPoolUsage() == baseline, "reset did not return page");
		spPage.Reset();
		const auto spNext = MHLib::utils::CSmartPtr<CRecvBuffer>::MakeShared();
		spPage = spNext;
		Require(spPage.GetRealPtr() == spNext.GetRealPtr(), "reset pointer could not be reused");
		spPage.Reset();
		Require(CRecvBuffer::GetPoolUsage() == baseline + 1, "reset returned a page still referenced");
	}

	void CheckPageView(EPacketMode mode)
	{
		const auto baseline = GetPoolUsage();
		CRecvViewMockSession stream;
		auto frame = MakeFrame(0, 0, mode);
		auto &page = stream.Prepare();
		MHLib::utils::CSmartPtr<CRecvBuffer> spPage(&page);
		char *pPage = page.GetPQueuePtr();
		std::memcpy(pPage, frame.m_wire.data(), frame.m_wire.size());
		Require(stream.Complete(static_cast<int>(frame.m_wire.size()), kPacketCode, mode), "page view rejected");
		CSerializableBufferView *pView = nullptr;
		Require(stream.Dequeue(&pView), "page view missing");
		// 검증 뒤 테스트가 보유한 페이지를 바꿔 View가 같은 바이트를 읽는지 확인한다.
		pPage[kHeaderBytes] ^= 1;
		frame.m_body[0] ^= 1;
		CheckPayload(*pView, frame, 0, 0);
		ReleasePacket(pView);
		Require(CSerializableBufferView::GetPoolUsage() == baseline.m_views, "page view not returned");
	}

	template<typename TStream, typename TPacket>
	void CheckRejected(EPacketMode mode, std::span<const char> badWire, int badPrefixBytes = 0)
	{
		const auto baseline = GetPoolUsage();
		{
			TStream stream;
			const auto good = MakeFrame(0, 0, mode);
			const auto following = MakeFrame(0, 1, mode);

			auto prefix = good.m_wire;
			Append(prefix, badWire.first(badPrefixBytes));
			if (badPrefixBytes != 0)
			{
				Require(Complete(stream, prefix, mode), "incomplete bad frame rejected early");
				prefix.clear();
			}
			Append(prefix, badWire.subspan(badPrefixBytes));
			Append(prefix, following.m_wire);
			Require(!Complete(stream, prefix, mode), "bad frame was accepted");
			TPacket *pPacket = nullptr;
			Require(stream.Dequeue(&pPacket), "valid frame before failure lost");
			CheckPayload(*pPacket, good, 0, 0);
			ReleasePacket(pPacket);
			Require(!stream.Dequeue(&pPacket), "bad frame or subsequent frame published");
			stream.Clear();
			Require(Complete(stream, following.m_wire, mode), "valid frame after reset rejected");
			Require(stream.Dequeue(&pPacket), "valid frame after reset lost");
			CheckPayload(*pPacket, following, 0, 1);
			ReleasePacket(pPacket);
			Require(!stream.Dequeue(&pPacket), "unexpected packet after failure");
		}
		Require(GetPoolUsage() == baseline, "pool usage not restored after rejection/reset");
	}

	struct SCounts
	{
		int m_delivered = 0;
		int m_direct = 0;
		int m_split = 0;
		std::size_t m_inputBytes = 0;
		std::size_t m_bodyBytes = 0;
	};

	template<typename TStream, typename TPacket>
	SCounts RunTraffic(const std::vector<std::vector<SGroup>> &input, EPacketMode mode)
	{
		constexpr bool kIsView = std::is_same_v<TPacket, CSerializableBufferView>;
		const auto baseline = GetPoolUsage();
		const int producers = static_cast<int>(input.size());
		const int groups = static_cast<int>(input[0].size());
		CLfQueue<TPacket *> queue;
		std::latch start(1), delayed(producers), consume(1);
		std::atomic<int> remaining = producers;
		std::vector<std::thread> workers;
		std::vector<SCounts> stats(producers);
		workers.reserve(producers);
		for (int producer = 0; producer < producers; ++producer)
		{
			workers.emplace_back([&, producer]
			{
				{
					TStream stream;
					auto &counts = stats[producer];
					start.wait();
					for (int groupId = 0; groupId < groups; ++groupId)
					{
						const auto &group = input[producer][groupId];
						static constexpr int kPublications[]{ 2, 0, 0, 2 };
						int frameId = 0;
						for (int i = 0; i < static_cast<int>(group.m_completions.size()); ++i)
						{
							const int before = frameId;
							const auto &bytes = group.m_completions[i];
							counts.m_inputBytes += bytes.size();
							Require(Complete(stream, bytes, mode), "valid completion rejected", producer, groupId * kFramesPerGroup);
							TPacket *pPacket = nullptr;
							while (stream.Dequeue(&pPacket))
							{
								const int sequence = groupId * kFramesPerGroup + frameId;
								Require(frameId < before + kPublications[i], "early or excess publication", producer, sequence);
								const auto &frame = group.m_frames[frameId];
								if constexpr (kIsView)
								{
									if (frameId == 2)
										++counts.m_split;
									else
										++counts.m_direct;
								}
								counts.m_bodyBytes += frame.m_body.size();
								++frameId;
								++counts.m_delivered;
								queue.Enqueue(pPacket); // 게시 후 생산자는 패킷을 읽지 않는다.
							}
							Require(frameId == before + kPublications[i], "missing publication", producer, groupId * kFramesPerGroup);
						}

						if (groupId == 0)
						{
							constexpr std::array<char, 2> kAbandonedPrefix{ static_cast<char>(kPacketCode), 0 };
							counts.m_inputBytes += kAbandonedPrefix.size();
							Require(Complete(stream, kAbandonedPrefix, mode), "partial prefix rejected before reset", producer);
							TPacket *pPacket = nullptr;
							Require(!stream.Dequeue(&pPacket), "partial prefix published before reset", producer);
							stream.Clear();
						}
						else if (groupId == 1)
						{
							// 리셋 뒤 첫 새 페이지들까지 기록할 때까지 소비자를 막는다.
							delayed.count_down();
							consume.wait();
						}
					}
				}
				--remaining;
			});
		}

		start.count_down();
		delayed.wait();
		const auto held = GetPoolUsage();
		const auto expectedHeld = static_cast<LONG>(2 * kFramesPerGroup * producers);
		Require(queue.GetUseSize() == expectedHeld, "delayed queue count mismatch");
		if constexpr (kIsView)
		{
			Require(held.m_views - baseline.m_views == expectedHeld && held.m_recvPages > baseline.m_recvPages,
				"delayed views did not retain pooled views and receive pages");
			Require(held.m_serializable == baseline.m_serializable, "view path allocated a legacy buffer");
		}
		else
			Require(held.m_serializable - baseline.m_serializable == expectedHeld, "delayed legacy pool usage mismatch");
		consume.count_down();

		std::vector<int> nextSequence(producers);
		int consumed = 0;
		while (true)
		{
			const bool isFinished = remaining.load() == 0;
			TPacket *pPacket = nullptr;
			if (!queue.Dequeue(&pPacket))
			{
				if (isFinished)
					break;
				std::this_thread::yield();
				continue;
			}
			Require(pPacket != nullptr && pPacket->GetDataSize() >= 2 * static_cast<int>(sizeof(int)), "missing producer/sequence");
			int producer, sequence;
			std::array<char, kMaxBodyBytes> body;
			const int bodyBytes = pPacket->GetDataSize();
			Require(bodyBytes <= kMaxBodyBytes && pPacket->Dequeue(body.data(), bodyBytes), "body read failed");
			const auto *pBody = body.data();
			std::memcpy(&producer, pBody, sizeof(producer));
			std::memcpy(&sequence, pBody + sizeof(producer), sizeof(sequence));
			Require(producer >= 0 && producer < producers, "producer ID invalid", producer, sequence);
			Require(sequence == nextSequence[producer] && sequence < groups * kFramesPerGroup,
				"per-producer FIFO, duplicate or missing frame", producer, sequence);
			CheckPayload(std::span<const char>(body.data(), bodyBytes),
				input[producer][sequence / kFramesPerGroup].m_frames[sequence % kFramesPerGroup], producer, sequence);
			++nextSequence[producer];
			++consumed;
			ReleasePacket(pPacket); // 생산자 TLS에서 할당하고 소비자에서 반환한다.
		}
		for (auto &worker : workers)
			worker.join();
		Require(queue.GetUseSize() == 0, "queue not empty at shutdown");
		Require(GetPoolUsage() == baseline, "pool usage not restored after consumer drain");

		SCounts total;
		for (int producer = 0; producer < producers; ++producer)
		{
			const auto &counts = stats[producer];
			Require(nextSequence[producer] == groups * kFramesPerGroup && counts.m_delivered == nextSequence[producer],
				"producer/consumer counts mismatch", producer);
			total.m_delivered += counts.m_delivered;
			total.m_direct += counts.m_direct;
			total.m_split += counts.m_split;
			total.m_inputBytes += counts.m_inputBytes;
			total.m_bodyBytes += counts.m_bodyBytes;
		}
		Require(total.m_delivered == consumed, "publish/consume totals differ");
		return total;
	}

	std::vector<std::vector<SGroup>> MakeInput(int producers, int groups, EPacketMode mode, bool isPacked = false)
	{
		std::vector<std::vector<SGroup>> input(producers);
		for (int producer = 0; producer < producers; ++producer)
		{
			input[producer].reserve(groups);
			for (int group = 0; group < groups; ++group)
				input[producer].push_back(MakeGroup(producer, group * kFramesPerGroup, mode, isPacked));
		}
		return input;
	}

	void Run(int producers, int groups, EPacketMode mode)
	{
		CheckPageReset();
		CheckPageView(mode);
		const auto input = MakeInput(producers, groups, mode);

		const auto view = RunTraffic<CRecvViewMockSession, CSerializableBufferView>(input, mode);
		const auto ring = RunTraffic<CLegacyRecv, CSerializableBuffer>(input, mode);
		Require(view.m_delivered == ring.m_delivered && view.m_inputBytes == ring.m_inputBytes && view.m_bodyBytes == ring.m_bodyBytes,
			"view/ring comparison counts mismatch");
		Require(view.m_direct == producers * groups * 3 && view.m_split == producers * groups, "view direct/split coverage mismatch");

		auto badCode = MakeFrame(0, 2, mode).m_wire;
		badCode[0] ^= 1;
		constexpr SPacketHeader kOversized{ kPacketCode, kMaxBodyBytes + 1, 0, 0 };
		const std::span<const char> oversized(reinterpret_cast<const char *>(&kOversized), kHeaderBytes);
		const auto rejectBoth = [&](std::span<const char> bytes, int prefixBytes = 0)
		{
			CheckRejected<CRecvViewMockSession, CSerializableBufferView>(mode, bytes, prefixBytes);
			CheckRejected<CLegacyRecv, CSerializableBuffer>(mode, bytes, prefixBytes);
		};
		rejectBoth(badCode);
		rejectBoth(oversized, 2);
		if (mode == EPacketMode::Net)
		{
			const auto badChecksum = MakeFrame(0, 2, mode, true);
			rejectBoth(badChecksum.m_wire);
			rejectBoth(badChecksum.m_wire, kHeaderBytes + 3);
		}

		CLogger::GetInstance()->WriteLogConsole(ELogLevel::Info,
			L"PASS mode=%ls producers=%d groups=%d view_packets=%d ring_packets=%d direct=%d split=%d input_bytes=%zu body_bytes=%zu",
			mode == EPacketMode::Net ? L"Net" : L"Lan", producers, groups, view.m_delivered, ring.m_delivered,
			view.m_direct, view.m_split, view.m_inputBytes, view.m_bodyBytes);
	}

	// 프로세스의 사용자·커널 CPU 시간을 100ns 단위로 합산한다.
	std::uint64_t ReadCpuTicks()
	{
		FILETIME created{}, exited{}, kernel{}, user{};
		Require(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != 0, "GetProcessTimes failed");
		const auto ticks = [](const FILETIME &time)
		{
			return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
		};
		return ticks(kernel) + ticks(user);
	}

	template<typename TStream, typename TPacket>
	void Measure(const std::vector<std::vector<SGroup>> &input, EPacketMode mode)
	{
		using Clock = std::chrono::steady_clock;
		const auto baseline = GetPoolUsage();
		const int producers = static_cast<int>(input.size());
		const int groups = static_cast<int>(input[0].size());
		const int warmupGroups = (std::min)(groups, 256);
		const int framesPerProducer = groups * kFramesPerGroup;
		CLfQueue<TPacket *> queue;
		std::latch warmed(producers), start(1), finished(producers);
		std::vector<std::thread> workers;
		workers.reserve(producers);
		for (int producer = 0; producer < producers; ++producer)
		{
			workers.emplace_back([&, producer]
			{
				TStream stream;
				const auto produce = [&](int groupCount)
				{
					for (int group = 0; group < groupCount; ++group)
					{
						for (const auto &bytes : input[producer][group].m_completions)
						{
							if (bytes.empty())
								continue;
							Require(Complete(stream, bytes, mode), "benchmark completion rejected");
							TPacket *pPacket = nullptr;
							while (stream.Dequeue(&pPacket))
								queue.Enqueue(pPacket);
						}
					}
				};
				produce(warmupGroups);
				warmed.count_down();
				start.wait();
				for (int pass = 0; pass < kBenchmarkPasses; ++pass)
					produce(groups);
				finished.count_down();
			});
		}

		std::vector<int> nextSequence(producers);
		const auto drain = [&](int groupCount, int passes)
		{
			const int framesInPass = groupCount * kFramesPerGroup;
			const int expected = framesInPass * producers * passes;
			for (int consumed = 0; consumed < expected;)
			{
				TPacket *pPacket = nullptr;
				if (!queue.Dequeue(&pPacket))
				{
					std::this_thread::yield();
					continue;
				}
				int producer = -1, sequence = -1;
				Require(pPacket->Dequeue(reinterpret_cast<char *>(&producer), sizeof(producer))
					&& pPacket->Dequeue(reinterpret_cast<char *>(&sequence), sizeof(sequence)), "benchmark packet read failed");
				Require(producer >= 0 && producer < producers, "benchmark producer ID");
				Require(sequence == nextSequence[producer]++ % framesInPass, "benchmark packet order");
				ReleasePacket(pPacket);
				++consumed;
			}
		};

		drain(warmupGroups, 1);
		warmed.wait();
		std::fill(nextSequence.begin(), nextSequence.end(), 0);
		const auto cpuStart = ReadCpuTicks();
		const auto wallStart = Clock::now();
		start.count_down();
		drain(groups, kBenchmarkPasses);
		finished.wait();
		const auto wallEnd = Clock::now();
		const auto cpuTicks = ReadCpuTicks() - cpuStart;
		for (auto &worker : workers)
			worker.join();
		Require(queue.GetUseSize() == 0 && GetPoolUsage() == baseline, "benchmark did not drain");
		for (const int sequence : nextSequence)
			Require(sequence == framesPerProducer * kBenchmarkPasses, "benchmark packet count mismatch");

		const auto wallNs = std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd - wallStart).count();
		Require(wallNs > 0 && cpuTicks > 0, "benchmark timer delta too small");
		CLogger::GetInstance()->WriteLogConsole(ELogLevel::Info, L"BENCH path=%ls packets=%d wall_ns=%lld cpu_100ns=%llu",
			std::is_same_v<TPacket, CSerializableBufferView> ? L"view" : L"ring", framesPerProducer * producers * kBenchmarkPasses,
			wallNs, cpuTicks);
	}
}

int main(void)
{
	CLogger::GetInstance()->SetLogLevel(ELogLevel::Info);
	CFileLoader file;
	file.Parse(L"ServerRecvView.ini");
	int producers = 0, groups = 0;
	Require(file.Load(L"ServerRecvView", L"Producers", &producers) && producers >= 1 && producers <= 32, "invalid Producers");
	Require(file.Load(L"ServerRecvView", L"Groups", &groups) && groups >= 2 && groups <= 4096, "invalid Groups");
	CEncryption::sm_packetKey = 0x97;
	int benchmark = 0;
	if (file.Load(L"ServerRecvView", L"Benchmark", &benchmark) && benchmark != 0)
	{
		int isNet = 0, isPacked = 0, isViewFirst = 0;
		Require(file.Load(L"ServerRecvView", L"Net", &isNet), "missing benchmark mode");
		Require(file.Load(L"ServerRecvView", L"Packed", &isPacked), "missing benchmark layout");
		Require(file.Load(L"ServerRecvView", L"ViewFirst", &isViewFirst), "missing benchmark order");
		const auto mode = isNet ? EPacketMode::Net : EPacketMode::Lan;
		const auto input = MakeInput(producers, groups, mode, isPacked != 0);
		const auto view = [&] { Measure<CRecvViewMockSession, CSerializableBufferView>(input, mode); };
		const auto ring = [&] { Measure<CLegacyRecv, CSerializableBuffer>(input, mode); };
		if (isViewFirst)
			view();
		ring();
		if (!isViewFirst)
			view();
		return EXIT_SUCCESS;
	}
	Run(producers, groups, EPacketMode::Net);
	Run(producers, groups, EPacketMode::Lan);
	return EXIT_SUCCESS;
}
