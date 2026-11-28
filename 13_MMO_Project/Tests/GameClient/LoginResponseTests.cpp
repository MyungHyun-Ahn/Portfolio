#include "pch.h"
#include "GameLoginId.h"

// 소켓이나 UE 없이 실제 응답 정책에 생성 DTO를 전달한다.
int main(void)
{
	using Game::CLoginResponse;
	using State = CLoginResponse::EState;
	if (!Game::IsValidLoginId(L"00123") || Game::IsValidLoginId(L"12a")) return 1;
	CLoginResponse login;
	Game::SResLogin packet{};
	packet.m_result = GameData::EResultCode::Success;
	packet.m_characterData.m_characterId = 23;
	if (login.Apply(packet)) return 2;
	login.OnSubmitted();
	if (login.CanRequest() || login.GetState() != State::Waiting) return 3;
	packet.m_result = GameData::EResultCode::Rejected;
	if (login.Apply(packet)) return 4;
	packet.m_characterData.m_characterId = 0;
	if (!login.Apply(packet) || !login.CanRequest() || login.GetState() != State::Rejected) return 5;
	login.OnSubmitted();
	packet.m_result = GameData::EResultCode::Success;
	if (login.Apply(packet)) return 6;
	packet.m_characterData.m_characterId = 23;
	packet.m_characterData.m_lastLogoutMap = 7;
	if (!login.Apply(packet) || login.GetState() != State::Accepted || login.CanRequest()) return 7;
	packet.m_characterData.m_characterId = 99;
	if (login.Apply(packet) || login.GetData().m_characterId != 23 || login.GetData().m_lastLogoutMap != 7) return 8;
	login = {};
	return login.GetData().m_characterId == 0 && !login.Apply(packet) ? 0 : 9;
}
