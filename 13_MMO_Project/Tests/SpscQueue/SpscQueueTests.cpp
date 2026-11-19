#include <atomic>
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include "MHLib/containers/CLFSpscQueue.h"

using MHLib::containers::CLFSpscQueue;

bool RejectsInvalidCapacity()
{
	for (const std::size_t capacity : {std::size_t{0}, (std::numeric_limits<std::size_t>::max)()})
	{
		try
		{
			CLFSpscQueue<int> queue(capacity);
			return false;
		}
		catch (const std::invalid_argument &)
		{
		}
	}
	return true;
}

bool ChecksRingBoundaries()
{
	CLFSpscQueue<int> queue(3);
	int outValue = -1;
	if (queue.TryDequeue(outValue) || outValue != -1)
		return false;

	for (int sequence = 0; sequence < 300; sequence += 3)
	{
		if (!queue.TryEnqueue(sequence) || !queue.TryEnqueue(sequence + 1) || !queue.TryEnqueue(sequence + 2)
			|| queue.TryEnqueue(-1))
			return false;
		for (int offset = 0; offset < 3; ++offset)
		{
			if (!queue.TryDequeue(outValue) || outValue != sequence + offset)
				return false;
		}
		if (queue.TryDequeue(outValue) || outValue != sequence + 2)
			return false;
	}
	return true;
}

bool PreservesMoveOnlyOwnership()
{
	CLFSpscQueue<std::unique_ptr<int>> queue(1);
	auto first = std::make_unique<int>(1);
	auto second = std::make_unique<int>(2);
	const int *pFirst = first.get();
	const int *pSecond = second.get();
	std::unique_ptr<int> outValue;
	return queue.TryEnqueue(std::move(first)) && !first
		&& !queue.TryEnqueue(std::move(second)) && second.get() == pSecond
		&& queue.TryDequeue(outValue) && outValue.get() == pFirst
		&& !queue.TryDequeue(outValue) && outValue.get() == pFirst
		&& queue.TryEnqueue(std::move(second)) && !second
		&& queue.TryDequeue(outValue) && outValue.get() == pSecond;
}

bool TransfersBetweenThreads()
{
	constexpr int kCount = 500000;
	CLFSpscQueue<int> queue(7);
	std::atomic<bool> isFailed = false;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	std::jthread producer([&]()
	{
		for (int sequence = 0; sequence < kCount; ++sequence)
		{
			while (!queue.TryEnqueue(sequence))
			{
				if (isFailed.load() || std::chrono::steady_clock::now() >= deadline)
				{
					isFailed.store(true);
					return;
				}
				std::this_thread::yield();
			}
		}
	});
	std::jthread consumer([&]()
	{
		for (int sequence = 0; sequence < kCount; ++sequence)
		{
			int value = -1;
			while (!queue.TryDequeue(value))
			{
				if (isFailed.load() || std::chrono::steady_clock::now() >= deadline)
				{
					isFailed.store(true);
					return;
				}
				std::this_thread::yield();
			}
			if (value != sequence)
			{
				isFailed.store(true);
				return;
			}
		}
	});
	producer.join();
	consumer.join();
	int outValue = -1;
	return !isFailed.load() && !queue.TryDequeue(outValue);
}

int main(void)
{
	if (!RejectsInvalidCapacity())
		return 1;
	if (!ChecksRingBoundaries())
		return 2;
	if (!PreservesMoveOnlyOwnership())
		return 3;
	if (!TransfersBetweenThreads())
		return 4;
	return 0;
}