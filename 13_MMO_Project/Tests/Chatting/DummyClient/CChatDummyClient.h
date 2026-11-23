#pragma once

namespace Chatting
{
	struct SDummyConfig;
	struct SDummyRunStats;
	enum class EChatPhase;

	class CChatDummyClient final : public CChatNetClient
	{
	public:
		CChatDummyClient(NetworkLib::CNetClientManager &network, const SDummyConfig &config, SDummyRunStats &stats, size_t index, const std::wstring &runMarker);

		[[nodiscard]] 
		bool 		Start() noexcept;
		void 		Close() noexcept;

		// 쌓인 이벤트를 꺼내 처리한다.
		void 		Pump() noexcept;

		[[nodiscard]] // 연결 성공하고 첫 섹터 진입 후 Ready 상태가 된다.
		bool 		IsReady() const noexcept;
		[[nodiscard]] 
		bool 		HasPendingAction() const noexcept;
		[[nodiscard]] 
		bool 		TakeActionCompletion() noexcept;

		[[nodiscard]] 
		int 		GetSectorIndex() const noexcept;
		void 		StartMove(int sectorIndex) noexcept;
		void 		StartMessage() noexcept;
		void 		SendHeartbeat() noexcept;

	private:
		using CChatNetClient::Connect;
		using CChatNetClient::Disconnect;
		using CChatNetClient::Tick;
		using TClock = std::chrono::steady_clock;
		enum class EAction { None, Move, Message };

		bool 	onConnect() noexcept override;
		void 	onRecvRejected() noexcept override;
		void 	onLoginAccepted(const SResLogin &packet) noexcept override;
		void 	onSectorEntered(const SResSectorMove &packet) noexcept override;
		void 	onChatMessageEcho(const SResMessage &packet) noexcept override;
		void 	onChatMessageNoti(const SMessageNoti &packet) noexcept override;
		void 	onChatDisconnected(bool wasConnected, DWORD error) noexcept override;

		void 	setPhase(EChatPhase phase, DWORD timeoutMilliseconds = 0) noexcept;
		[[nodiscard]] 
		bool 	checkDeadline() noexcept;
		void 	completeAction() noexcept;
		void 	recordFailure(const char *pReason, DWORD error = 0) noexcept;

		const SDummyConfig 	&m_config;
		SDummyRunStats 		&m_stats;
		const std::wstring 	&m_runMarker;
		const INT64 		m_dummyAccount;
		const std::wstring 	m_id;
		std::wstring 		m_name;
		EChatPhase 			m_phase;
		EChatPhase 			m_phaseBeforeClose;
		TClock::time_point 	m_deadline{};
		TClock::time_point 	m_messageStartedAt{};
		EAction 			m_action = EAction::None;
		bool 				m_hasActionCompletion = false;
		bool 				m_hasReportedRecvRejection = false;
		int 				m_sectorIndex = 0;
		UINT64 				m_sequence = 0;
		std::wstring 		m_expectedMessage;
	};
}
