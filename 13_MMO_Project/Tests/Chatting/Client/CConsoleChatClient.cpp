#include "pch.h"
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "Config.h"
#include "CNetClient.h"
#include "CConsoleChatClient.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CConsoleChatClient::sendOwned(NetworkLib::CSerializableBuffer<FALSE> *pMessage) noexcept
	{
		pMessage->IncreaseRef();
		bool isSent = SendPacket(pMessage);
		if (pMessage->DecreaseRef() == 0)
			NetworkLib::CSerializableBuffer<FALSE>::Free(pMessage);
		return isSent;
	}

	bool CConsoleChatClient::Login() noexcept
	{
		auto *pMessage = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		WCHAR id[kNameLength]{};
		WCHAR nickname[kNameLength]{};
		wcscpy_s(id, std::to_wstring(m_config.m_accountNo).c_str());
		wcscpy_s(nickname, m_config.m_name.c_str());
		*pMessage << static_cast<WORD>(EPacketType::ReqLogin) << m_config.m_accountNo;
		pMessage->Enqueue((char *)id, sizeof(id));
		pMessage->Enqueue((char *)nickname, sizeof(nickname));
		pMessage->Enqueue((char *)kTestSessionKey, sizeof(kTestSessionKey));
		return sendOwned(pMessage);
	}

	bool CConsoleChatClient::SendMessage(const std::wstring &text) noexcept
	{
		if (!m_isReady || text.empty() || text.size() * sizeof(WCHAR) > kMaxMessageBytes)
			return false;
		auto *pMessage = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		WORD messageBytes = static_cast<WORD>(text.size() * sizeof(WCHAR));
		*pMessage << static_cast<WORD>(EPacketType::ReqMessage) << m_config.m_accountNo << messageBytes;
		pMessage->Enqueue((char *)text.data(), messageBytes);
		return sendOwned(pMessage);
	}

	bool CConsoleChatClient::SendHeartbeat() noexcept
	{
		if (!m_isReady)
			return false;
		auto *pMessage = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		*pMessage << static_cast<WORD>(EPacketType::ReqHeartbeat) << m_config.m_accountNo;
		return sendOwned(pMessage);
	}

	bool CConsoleChatClient::OnRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		if (message.GetDataSize() < sizeof(WORD))
			return false;
		WORD type;
		message >> type;
		if (static_cast<EPacketType>(type) == EPacketType::ResLogin)
		{
			if (m_isLogin || message.GetDataSize() != 9)
				return false;
			BYTE status;
			INT64 accountNo;
			message >> status >> accountNo;
			if (status != TRUE || accountNo != m_config.m_accountNo)
				return false;
			m_isLogin = true;
			g_pLogger->WriteLogConsole(ELogLevel::System, L"LOGIN OK account=%lld", accountNo);
			// 로그인 응답을 받은 다음 섹터에 등록한다.
			auto *pRequest = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
			*pRequest << static_cast<WORD>(EPacketType::ReqSectorMove) << accountNo << m_config.m_sectorX << m_config.m_sectorY;
			return sendOwned(pRequest);
		}
		if (static_cast<EPacketType>(type) == EPacketType::ResSectorMove)
		{
			if (!m_isLogin || message.GetDataSize() != 12)
				return false;
			INT64 accountNo;
			WORD sectorX;
			WORD sectorY;
			message >> accountNo >> sectorX >> sectorY;
			if (accountNo != m_config.m_accountNo || sectorX != m_config.m_sectorX || sectorY != m_config.m_sectorY)
				return false;
			g_pLogger->WriteLogConsole(ELogLevel::System, L"READY account=%lld sector=%u,%u", accountNo, sectorX, sectorY);
			m_isReady = true;
			return true;
		}
		if (static_cast<EPacketType>(type) != EPacketType::ResMessage || !m_isReady || message.GetDataSize() < 90)
			return false;

		INT64 accountNo;
		WCHAR id[kNameLength];
		WCHAR nickname[kNameLength];
		WORD messageBytes;
		message >> accountNo;
		message.Dequeue((char *)id, sizeof(id));
		message.Dequeue((char *)nickname, sizeof(nickname));
		message >> messageBytes;
		if (messageBytes == 0 || messageBytes > kMaxMessageBytes || messageBytes % sizeof(WCHAR) != 0
			|| message.GetDataSize() != messageBytes || wmemchr(id, L'\0', kNameLength) == nullptr
			|| wmemchr(nickname, L'\0', kNameLength) == nullptr)
			return false;
		std::wstring text(messageBytes / sizeof(WCHAR), L'\0');
		message.Dequeue((char *)text.data(), messageBytes);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"CHAT account=%lld name=%ls: %ls", accountNo, nickname, text.c_str());
		return true;
	}

	void CConsoleChatClient::OnDisconnect() noexcept
	{
		m_isReady = false;
		m_isLogin = false;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"DISCONNECTED");
	}
}
