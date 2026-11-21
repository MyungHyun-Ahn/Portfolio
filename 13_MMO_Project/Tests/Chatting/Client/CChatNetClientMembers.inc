// 생성된 CChatNetClient 클래스 내부에 삽입하는 수동 선언이다.
// 필요한 선언 함수는 여기에 작성합니다.

	public:
		[[nodiscard]] 
		bool IsChatReady() const noexcept;

	protected:
		// 여기에 클라이언트의 요청 코드를 작성합니다. (수동)
		[[nodiscard]] 
		bool ReqLogin(std::int64_t accountNo, const std::wstring &id, const std::wstring &nickname, const std::array<char, 64> &sessionKey) noexcept;
		[[nodiscard]]
		bool ReqSectorMove(std::uint16_t sectorX, std::uint16_t sectorY) noexcept;

	private:
		// 여기에 더미 클라와 클라이언트 간 서로 다른 동작을 정의할 순수 가상함수를 작성합니다.
		// - Ex) 콘솔 출력, 더미 클라이언트 정상 동작 검증 코드
		virtual void 		onLoginAccepted(const SResLogin &packet) noexcept = 0;
		virtual void 		onSectorEntered(const SResSectorMove &packet) noexcept = 0;
		virtual void 		onChatMessageEcho(const SResMessage &packet) noexcept = 0;
		virtual void 		onChatMessageNoti(const SMessageNoti &packet) noexcept = 0;
		virtual void 		onChatDisconnected(bool wasConnected, DWORD error) noexcept = 0;
		void 				onDisconnect(bool wasConnected, DWORD error) noexcept override final;

	private:
		// 여기에 멤버 변수를 작성합니다. (수동)
		ELoginState m_loginState = ELoginState::Idle;
		std::int64_t m_accountNo = 0;

		std::optional<SSectorPosition> m_sector;
		std::optional<SSectorPosition> m_requestedSector;
