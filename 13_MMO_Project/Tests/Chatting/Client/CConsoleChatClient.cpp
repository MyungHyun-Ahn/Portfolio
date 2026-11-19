#include "pch.h"
#include <memory>
#include <condition_variable>
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "CNetClientManager.h"
#include "CNetClient.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "ClientPacketSend.g.h"
#include "CConsoleChatClient.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CConsoleChatClient::onConnect() noexcept
	{
		return login();
	}

	bool CConsoleChatClient::login() noexcept
	{
		std::array<WCHAR, kNameLength> id{};
		std::array<WCHAR, kNameLength> nickname{};
		std::array<char, kSessionKeyBytes> sessionKey{};
		if (m_config.m_accountNo <= 0 || _i64tow_s(m_config.m_accountNo, id.data(), id.size(), 10) != 0
			|| m_config.m_name.size() >= nickname.size()
			|| wcscpy_s(nickname.data(), nickname.size(), m_config.m_name.c_str()) != 0)
			return false;
		std::copy_n(kTestSessionKey, kSessionKeyBytes, sessionKey.begin());
		return SendReqLogin(m_config.m_accountNo, id, nickname, sessionKey);
	}

	bool CConsoleChatClient::HandleResLogin(const SResLogin &packet) noexcept
	{
		if (m_isLogin || packet.m_status != TRUE || packet.m_accountNo != m_config.m_accountNo)
			return false;
		m_isLogin = true;
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"LOGIN OK account=%lld", packet.m_accountNo);
		// 로그인 응답을 받은 다음 섹터에 등록한다.
		return SendReqSectorMove(packet.m_accountNo, m_config.m_sectorX, m_config.m_sectorY);
	}

	bool CConsoleChatClient::HandleResSectorMove(const SResSectorMove &packet) noexcept
	{
		if (!m_isLogin || packet.m_accountNo != m_config.m_accountNo
			|| packet.m_sectorX != m_config.m_sectorX || packet.m_sectorY != m_config.m_sectorY)
			return false;
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"READY account=%lld sector=%u,%u",
			packet.m_accountNo, packet.m_sectorX, packet.m_sectorY);
		m_isReady = true;
		return true;
	}

	bool CConsoleChatClient::HandleResMessage(const SResMessage &packet) noexcept
	{
		if (!m_isReady || packet.m_message.empty() || packet.m_message.size() > kMaxMessageBytes / sizeof(WCHAR)
			|| wmemchr(packet.m_id.data(), L'\0', packet.m_id.size()) == nullptr
			|| wmemchr(packet.m_nickname.data(), L'\0', packet.m_nickname.size()) == nullptr)
			return false;
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"CHAT account=%lld name=%ls: %ls",
			packet.m_accountNo, packet.m_nickname.data(), packet.m_message.c_str());
		return true;
	}

	void CConsoleChatClient::onDisconnect(bool wasConnected, DWORD error) noexcept
	{
		if (!wasConnected)
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"ConnectEx completion failed: error=%lu", error);
		m_isReady = false;
		m_isLogin = false;
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"DISCONNECTED");
	}
}
