#include "pch.h"
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "CNetClient.h"
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

	// SendPacket()이 송신 참조를 잡기 전에 실패할 수 있으므로 호출 중에는 버퍼 참조를 유지한다.
	// 호출이 끝나면 자기 참조만 내리고, 마지막 참조였다면 버퍼를 풀에 반환한다
	bool CConsoleChatClient::sendOwned(NetworkLib::CSerializableBuffer<FALSE> *pMessage) noexcept
	{
		if (pMessage == nullptr)
			return false;
		pMessage->IncreaseRef();
		bool isSent = SendPacket(pMessage);
		if (pMessage->DecreaseRef() == 0)
			NetworkLib::CSerializableBuffer<FALSE>::Free(pMessage);
		return isSent;
	}

	bool CConsoleChatClient::Login() noexcept
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
		return sendOwned(MakePacketReqLogin(packet));
	}

	bool CConsoleChatClient::SendMessage(const std::wstring &text) noexcept
	{
		if (!m_isReady || text.empty() || text.size() > kMaxMessageBytes / sizeof(WCHAR))
			return false;
		try
		{
			SReqMessage packet;
			packet.m_accountNo = m_config.m_accountNo;
			packet.m_message = text;
			return sendOwned(MakePacketReqMessage(packet));
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
	}

	bool CConsoleChatClient::SendHeartbeat() noexcept
	{
		if (!m_isReady)
			return false;
		SHeartbeatNoti packet;
		packet.m_accountNo = m_config.m_accountNo;
		return sendOwned(MakePacketHeartbeatNoti(packet));
	}

	bool CConsoleChatClient::OnRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
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
		return sendOwned(MakePacketReqSectorMove(request));
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

	void CConsoleChatClient::OnDisconnect() noexcept
	{
		m_isReady = false;
		m_isLogin = false;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"DISCONNECTED");
	}
}
