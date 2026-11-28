#include "pch.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CClientManager.h"
#include "CClient.h"
#include "GameClient.g.h"
#include "GameLoginId.h"

namespace Game
{
	bool GameClient::ReqLogin(const std::wstring& loginId) noexcept
	{
		if (!IsConnected() || !m_login.CanRequest() ||
			!IsValidLoginId(loginId) || !SendReqLogin(loginId))
			return false;
		m_login.OnSubmitted();
		return true;
	}

	bool GameClient::HandleResLogin(const SResLogin& packet) noexcept
	{
		return m_login.Apply(packet);
	}

	void GameClient::onDisconnect(bool wasConnected, DWORD error) noexcept
	{
		m_login = {};
		onGameDisconnected(wasConnected, error);
	}
}
