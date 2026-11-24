// Handwritten protocol client members. Add client state and member declarations here.
	protected:
		// 검증된 맵 응답을 테스트 클라이언트에 전달한다.
		virtual bool onAddResponse(const SResAdd &packet) noexcept = 0;
