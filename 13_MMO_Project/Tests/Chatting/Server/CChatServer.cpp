#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "CServer.h"
#include "IContents.h"
#include "ContentsHandle.h"
#include "CContentsRuntime.h"
#include "CommonProtocol.h"
#include "CSector.h"
#include "ServerPacketSend.g.h"
#include "IServerPacketHandler.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	CChatServer::CChatServer(std::size_t contentsWorkerCount, std::chrono::milliseconds frameInterval)
		: m_runtime(contentsWorkerCount, frameInterval)
	{
	}

	bool CChatServer::StartContents()
	{
		auto upContents = std::make_unique<CChatContents>(kChatContentsId, *this);
		m_pContents = upContents.get();
		if (!m_runtime.RegisterContents(std::move(upContents), true, 0))
		{
			m_pContents = nullptr;
			return false;
		}
		m_completionHandle = m_runtime.GetContentsHandle(kChatContentsId);
		return m_runtime.Start();
	}

	void CChatServer::StopContents()
	{
		m_runtime.Stop();
	}

	bool CChatServer::OnConnectionRequest(const WCHAR *pIp, USHORT /*port*/) noexcept
	{
		// 고정 테스트 키는 외부 접속에 개방하지 않는다.
		return wcscmp(pIp, L"127.0.0.1") == 0;
	}

	void CChatServer::OnAccept(UINT64 sessionId) noexcept
	{
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=session_accepted session_id=%llu | 세션의 접속을 수락했습니다.", sessionId);
		if (!m_runtime.PostCompletion(m_completionHandle, std::bind_front(&CChatContents::OnEnter, m_pContents, sessionId)))
			Disconnect(sessionId);
	}

	void CChatServer::OnClientLeave(UINT64 sessionId) noexcept
	{
		(void)m_runtime.PostCompletion(m_completionHandle, std::bind_front(&CChatContents::OnLeave, m_pContents, sessionId));
	}

	void CChatServer::OnError(int errorCode, WCHAR *pMessage) noexcept
	{
		g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", 0, std::source_location::current(),
			L"event=network_error error_code=%d | 네트워크 오류가 발생했습니다. %.160ls",
			errorCode, pMessage != nullptr ? pMessage : L"");
	}

	void CChatServer::OnServerReady(USHORT port) noexcept
	{
		g_pLogger->WriteLog(ELogLevel::Info, L"ChatServer", 0, std::source_location::current(),
			L"event=server_ready port=%u | 테스트 채팅 서버가 준비되었습니다.", static_cast<unsigned>(port));
	}
}
