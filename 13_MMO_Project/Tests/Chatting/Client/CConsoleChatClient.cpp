#include "pch.h"
#include <new>
#include <memory>
#include <condition_variable>
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "ClientConfig.h"
#include "CClientManager.h"
#include "CClient.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ClientPacketDispatch.g.h"
#include "CChatNetClient.g.h"
#include "CConsoleChatClient.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CConsoleChatClient::onConnect() noexcept
	{
		std::array<char, kSessionKeyBytes> sessionKey{};
		std::copy_n(kTestSessionKey, kSessionKeyBytes, sessionKey.begin());
		try
		{
			return ReqLogin(m_config.m_accountNo, std::to_wstring(m_config.m_accountNo), m_config.m_name, sessionKey);
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

	void CConsoleChatClient::onLoginAccepted(const SResLogin &packet) noexcept
	{
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"LOGIN OK account=%lld", packet.m_accountNo);
		if (!ReqSectorMove(m_config.m_sectorX, m_config.m_sectorY))
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"최초 섹터 진입 요청 실패: account=%lld", packet.m_accountNo);
			Disconnect();
		}
	}

	void CConsoleChatClient::onSectorEntered(const SResSectorMove &packet) noexcept
	{
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"READY account=%lld sector=%u,%u",
			packet.m_accountNo, packet.m_sectorX, packet.m_sectorY);
	}

	namespace
	{
		void PrintChatMessage(std::int64_t accountNo, const std::wstring &nickname, const std::wstring &message) noexcept
		{
			g_pLogger->WriteLogConsole(ELogLevel::Info, L"CHAT account=%lld name=%ls: %ls",
				accountNo, nickname.c_str(), message.c_str());
		}
	}

	void CConsoleChatClient::onChatMessageEcho(const SResMessage &packet) noexcept
	{
		PrintChatMessage(packet.m_accountNo, packet.m_nickname, packet.m_message);
	}

	void CConsoleChatClient::onChatMessageNoti(const SMessageNoti &packet) noexcept
	{
		PrintChatMessage(packet.m_accountNo, packet.m_nickname, packet.m_message);
	}

	void CConsoleChatClient::onChatDisconnected(bool wasConnected, DWORD error) noexcept
	{
		if (!wasConnected)
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"ConnectEx completion failed: error=%lu", error);

		g_pLogger->WriteLogConsole(ELogLevel::Info, L"DISCONNECTED");
	}
}
