#pragma once

namespace MetricsLib
{
	struct SMetricsSnapshot;

	class CMetricsExporter final
	{
	public:
		CMetricsExporter() noexcept;
		~CMetricsExporter();
		CMetricsExporter(const CMetricsExporter&) = delete;
		CMetricsExporter& operator=(const CMetricsExporter&) = delete;

		// 호출자가 바인딩 주소를 지정한다. 수명 관리 호출은 한 스레드에서 수행한다.
		// 포트가 0이면 OS가 포트를 선택한다.
		// 성공 시에만 outBoundPort를 갱신한다.
		[[nodiscard]] 
		bool Start(const std::string& bindAddress, uint16_t port, uint16_t& outBoundPort);
		// 리스너와 HTTP 워커를 종료할 때까지 기다린다. 마지막 게시 데이터는 유지한다.
		void Stop() noexcept;

		// 소유자가 콜백 실행을 직렬화하고, 소멸 전에 등록을 해제하고 진행 중인 호출을 기다린다.
		[[nodiscard]] 
		std::function<void()> CreatePublishCallback(std::function<bool(SMetricsSnapshot&)> collector);

	private:
		struct SListener;
		// 리스너와 HTTP 워커를 먼저 정리한 뒤 응답 저장소를 해제한다.
		std::atomic<std::shared_ptr<const std::string>> 	m_spLatestBody;
		std::unique_ptr<SListener> 							m_pListener;
	};
}
