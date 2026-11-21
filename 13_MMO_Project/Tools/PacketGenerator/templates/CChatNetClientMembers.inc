// 생성된 CChatNetClient 클래스 내부에 삽입하는 수동 선언이다.
	public:
		[[nodiscard]] bool IsChatReady() const noexcept;

	protected:
		[[nodiscard]] bool ReqLogin(std::int64_t accountNo, const std::wstring &id,
			const std::wstring &nickname, const std::array<char, 64> &sessionKey) noexcept;
		[[nodiscard]] bool ReqSectorMove(std::uint16_t sectorX, std::uint16_t sectorY) noexcept;

	private:
		// 앱 스레드에서 상태를 갱신한 뒤 호출한다. 패킷 참조는 호출 중에만 유효하다.
		virtual void onLoginAccepted(const SResLogin &packet) noexcept = 0;
		virtual void onSectorEntered(const SResSectorMove &packet) noexcept = 0;
		virtual void onChatMessageEcho(const SResMessage &packet) noexcept = 0;
		virtual void onChatMessageNoti(const SMessageNoti &packet) noexcept = 0;
		virtual void onChatDisconnected(bool wasConnected, DWORD error) noexcept = 0;
		void onDisconnect(bool wasConnected, DWORD error) noexcept override final;

		ELoginState m_loginState = ELoginState::Idle;
		std::int64_t m_accountNo = 0;
		std::optional<SSectorPosition> m_sector;
		std::optional<SSectorPosition> m_requestedSector;
