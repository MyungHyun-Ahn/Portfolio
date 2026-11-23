#include "pch.h"

#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "CClientManager.h"
#include "CClient.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "CChatNetClient.g.h" // 실제 선언 부가 여기에 자동 생성됨

namespace Chatting
{
	namespace
	{
		bool IsValidChatMessage(const std::wstring &id, const std::wstring &nickname, const std::wstring &message) noexcept
		{
			return !message.empty() && message.size() <= kMaxMessageBytes / sizeof(WCHAR)
				&& IsValidString(id, kMaxNameCodeUnits) && IsValidString(nickname, kMaxNameCodeUnits);
		}
	}

	bool CChatNetClient::IsChatReady() const noexcept
	{
		// 섹터에 진입한 유저만 채팅이 가능하므로, 섹터 진입 여부로 채팅 가능 상태를 확인한다.
		return m_sector.has_value();
	}

	bool CChatNetClient::ReqLogin(std::int64_t accountNo, const std::wstring &id,
		const std::wstring &nickname, const std::array<char, 64> &sessionKey) noexcept
	{
		if (!IsConnected() || m_loginState != ELoginState::Idle || accountNo <= 0
			|| !IsValidString(id, kMaxNameCodeUnits) || !IsValidString(nickname, kMaxNameCodeUnits)
			|| !SendReqLogin(accountNo, id, nickname, sessionKey))
			return false;

		// 로그인 응답과 대조할 계정을 저장한다.
		m_accountNo = accountNo;
		m_loginState = ELoginState::Pending;
		return true;
	}

	bool CChatNetClient::ReqSectorMove(std::uint16_t sectorX, std::uint16_t sectorY) noexcept
	{
		if (!IsConnected() || m_loginState != ELoginState::Accepted || m_requestedSector.has_value()
			|| sectorX >= kMaxSectorX || sectorY >= kMaxSectorY || !SendReqSectorMove(m_accountNo, sectorX, sectorY))
			return false;

		m_requestedSector = SSectorPosition{ sectorX, sectorY };
		return true;
	}

	bool CChatNetClient::HandleResLogin(const SResLogin &packet) noexcept
	{
		if (m_loginState != ELoginState::Pending || packet.m_status != TRUE || packet.m_accountNo != m_accountNo)
			return false;
		m_loginState = ELoginState::Accepted;
		onLoginAccepted(packet);
		return true;
	}

	bool CChatNetClient::HandleResSectorMove(const SResSectorMove &packet) noexcept
	{
		if (m_loginState != ELoginState::Accepted || !m_requestedSector || packet.m_accountNo != m_accountNo
			|| packet.m_sectorX != m_requestedSector->m_x || packet.m_sectorY != m_requestedSector->m_y)
			return false;
		m_sector = m_requestedSector;
		m_requestedSector.reset();
		onSectorEntered(packet);
		return true;
	}

	bool CChatNetClient::HandleResMessage(const SResMessage &packet) noexcept
	{
		if (!IsChatReady() || packet.m_accountNo != m_accountNo
			|| !IsValidChatMessage(packet.m_id, packet.m_nickname, packet.m_message))
			return false;
		onChatMessageEcho(packet);
		return true;
	}

	bool CChatNetClient::HandleMessageNoti(const SMessageNoti &packet) noexcept
	{
		if (!IsChatReady() || packet.m_accountNo <= 0 || packet.m_accountNo == m_accountNo
			|| !IsValidChatMessage(packet.m_id, packet.m_nickname, packet.m_message))
			return false;
		onChatMessageNoti(packet);
		return true;
	}

	void CChatNetClient::onDisconnect(bool wasConnected, DWORD error) noexcept
	{
		m_loginState = ELoginState::Idle;
		m_accountNo = 0;
		m_sector.reset();
		m_requestedSector.reset();
		onChatDisconnected(wasConnected, error);
	}
}
