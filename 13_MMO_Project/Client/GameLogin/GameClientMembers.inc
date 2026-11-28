public:
	using ELoginState = CLoginResponse::EState;

	[[nodiscard]]
	bool ReqLogin(const std::wstring& loginId) noexcept;
	ELoginState GetLoginState() const noexcept { return m_login.GetState(); }
	const SCharacterData& GetCharacterData() const noexcept { return m_login.GetData(); }

protected:
	virtual void onGameDisconnected(bool wasConnected, DWORD error) noexcept = 0;

private:
	void onDisconnect(bool wasConnected, DWORD error) noexcept override final;

private:
	CLoginResponse m_login;
