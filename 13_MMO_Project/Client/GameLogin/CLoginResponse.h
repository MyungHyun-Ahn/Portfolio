#pragma once

namespace Game
{
	// 연결마다 하나만 소유한다. 송신 접수 전과 단절 후에는 응답을 채택하지 않는다.
	class CLoginResponse final
	{
	public:
		enum class EState { Idle, Waiting, Rejected, Accepted };

		bool CanRequest() const noexcept { return m_state == EState::Idle || m_state == EState::Rejected; }
		void OnSubmitted() noexcept { m_state = EState::Waiting; }
		EState GetState() const noexcept { return m_state; }
		const SCharacterData& GetData() const noexcept { return m_data; }

		bool Apply(const SResLogin& packet) noexcept
		{
			if (m_state != EState::Waiting) return false;
			const auto& data = packet.m_characterData;
			if (packet.m_result == GameData::EResultCode::Rejected && !data.m_characterId && !data.m_lastLogoutMap)
			{
				m_state = EState::Rejected;
				return true;
			}
			if (packet.m_result != GameData::EResultCode::Success || !data.m_characterId) return false;
			m_data = data;
			m_state = EState::Accepted;
			return true;
		}

	private:
		EState m_state = EState::Idle;
		SCharacterData m_data{};
	};
}
