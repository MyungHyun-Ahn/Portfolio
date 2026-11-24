#pragma once

namespace ContentsRuntimeLib
{
	using ContentsInstanceId = std::uint64_t;

	class CContentsRuntime;

	class IContents
	{
	public:
		virtual ~IContents() = default;
		IContents(const IContents &) = delete;
		IContents &operator=(const IContents &) = delete;

		// 등록에 사용하는 인스턴스 식별자를 반환한다.
		[[nodiscard]]
		ContentsInstanceId GetInstanceId() const noexcept;

	protected:
		// 인스턴스 식별자를 지정한다.
		explicit IContents(ContentsInstanceId instanceId) noexcept;

		// 프레임을 등록한 인스턴스에서만 호출한다.
		virtual void OnFrame();

	private:
		friend class CContentsRuntime;

	private:
		const ContentsInstanceId m_instanceId;
		std::mutex m_completionLock;
		std::deque<std::function<void()>> m_completionQueue;
	};
}