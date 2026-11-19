#include "pch.h"
#include <memory>
#include <condition_variable>
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "CNetClientManager.h"
#include "CClientMailbox.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "ClientPacketSend.g.h"
#include "ClientPacketDispatch.g.h"
#include "CConsoleChatClient.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CConsoleChatClient::Connect() noexcept
	{
		if (IsActive())
			return false;
		try
		{
			auto spMailbox = std::make_shared<NetworkLib::CClientMailbox>();
			m_sessionId = m_network.Connect(m_config.m_ip.c_str(), m_config.m_port, kPacketCode, spMailbox);
			if (m_sessionId == 0)
				return false;
			m_spMailbox = std::move(spMailbox);
			return true;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

	void CConsoleChatClient::Disconnect() noexcept
	{
		m_isConnected = false;
		if (IsActive())
			m_network.Disconnect(m_sessionId, 2000);
	}

	// 클라이언트는 Tick을 가지고 매 프레임마다 이 함수를 호출한다.
	void CConsoleChatClient::Tick() noexcept
	{
		if (m_spMailbox == nullptr)
			return;
		NetworkLib::SClientEvent event;
		for (int count = 0; count < 64 && m_spMailbox->TryPop(event); ++count)
		{
			switch (event.m_kind)
			{
			case NetworkLib::EClientEvent::Connected:
				onConnect();
				break;
			case NetworkLib::EClientEvent::Packet:
				if (m_isConnected && !onRecv(*event.m_spPacket))
				{
					m_isConnected = false;
					m_network.Disconnect(m_sessionId);
				}
				break;
			case NetworkLib::EClientEvent::ConnectFailed:
				g_pLogger->WriteLogConsole(ELogLevel::Error, L"ConnectEx completion failed: error=%lu", event.m_error);
				[[fallthrough]];
			case NetworkLib::EClientEvent::Disconnected:
				onDisconnect();
				return;
			}
		}
	}

	void CConsoleChatClient::onConnect() noexcept
	{
		m_isConnected = true;
		if (!login())
		{
			m_isConnected = false;
			m_network.Disconnect(m_sessionId);
		}
	}

	bool CConsoleChatClient::login() noexcept
	{
		SReqLogin packet;
		packet.m_accountNo = m_config.m_accountNo;
		WCHAR id[kNameLength]{};
		if (m_config.m_accountNo <= 0 || _i64tow_s(m_config.m_accountNo, id, kNameLength, 10) != 0
			|| m_config.m_name.size() >= packet.m_nickname.size()
			|| wcscpy_s(packet.m_id.data(), packet.m_id.size(), id) != 0
			|| wcscpy_s(packet.m_nickname.data(), packet.m_nickname.size(), m_config.m_name.c_str()) != 0)
			return false;
		std::copy_n(kTestSessionKey, kSessionKeyBytes, packet.m_sessionKey.begin());
		return m_network.SendPacket(m_sessionId, MakePacketReqLogin(packet));
	}

	bool CConsoleChatClient::SendReqMessage(const std::wstring &text) noexcept
	{
		if (!m_isReady || text.empty() || text.size() > kMaxMessageBytes / sizeof(WCHAR))
			return false;
		try
		{
			SReqMessage packet;
			packet.m_accountNo = m_config.m_accountNo;
			packet.m_message = text;
			return m_network.SendPacket(m_sessionId, MakePacketReqMessage(packet));
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

	bool CConsoleChatClient::SendNotiHeartbeat() noexcept
	{
		if (!m_isReady)
			return false;
		SHeartbeatNoti packet;
		packet.m_accountNo = m_config.m_accountNo;
		return m_network.SendPacket(m_sessionId, MakePacketHeartbeatNoti(packet));
	}

	bool CConsoleChatClient::onRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		return DispatchClientPacket(message, *this);
	}

	bool CConsoleChatClient::HandleResLogin(const SResLogin &packet) noexcept
	{
		if (m_isLogin || packet.m_status != TRUE || packet.m_accountNo != m_config.m_accountNo)
			return false;
		m_isLogin = true;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"LOGIN OK account=%lld", packet.m_accountNo);
		// 로그인 응답을 받은 다음 섹터에 등록한다.
		SReqSectorMove request;
		request.m_accountNo = packet.m_accountNo;
		request.m_sectorX = m_config.m_sectorX;
		request.m_sectorY = m_config.m_sectorY;
		return m_network.SendPacket(m_sessionId, MakePacketReqSectorMove(request));
	}

	bool CConsoleChatClient::HandleResSectorMove(const SResSectorMove &packet) noexcept
	{
		if (!m_isLogin || packet.m_accountNo != m_config.m_accountNo
			|| packet.m_sectorX != m_config.m_sectorX || packet.m_sectorY != m_config.m_sectorY)
			return false;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"READY account=%lld sector=%u,%u",
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
		g_pLogger->WriteLogConsole(ELogLevel::System, L"CHAT account=%lld name=%ls: %ls",
			packet.m_accountNo, packet.m_nickname.data(), packet.m_message.c_str());
		return true;
	}

	void CConsoleChatClient::onDisconnect() noexcept
	{
		m_sessionId = 0;
		m_spMailbox.reset();
		m_isConnected = false;
		m_isReady = false;
		m_isLogin = false;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"DISCONNECTED");
	}
}
