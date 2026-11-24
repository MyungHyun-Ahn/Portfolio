#pragma once

namespace MHLib::utils
{
	class IExecutionTimeSink
	{
	public:
		virtual ~IExecutionTimeSink() = default;

		// 측정한 경과 시간을 기록한다.
		virtual void Record(std::chrono::steady_clock::duration elapsed) noexcept = 0;
	};

	class CScopedExecutionTimer final
	{
	public:
		// 기록 대상은 타이머보다 오래 살아야 한다.
		explicit CScopedExecutionTimer(IExecutionTimeSink &sink) noexcept
			: m_sink(sink)
			, m_start(std::chrono::steady_clock::now())
		{
		}

		// 스코프를 벗어날 때 경과 시간을 한 번 기록한다.
		~CScopedExecutionTimer() noexcept
		{
			m_sink.Record(std::chrono::steady_clock::now() - m_start);
		}

		CScopedExecutionTimer(const CScopedExecutionTimer &) = delete;
		CScopedExecutionTimer &operator=(const CScopedExecutionTimer &) = delete;

	private:
		IExecutionTimeSink &m_sink;
		const std::chrono::steady_clock::time_point m_start;
	};
}