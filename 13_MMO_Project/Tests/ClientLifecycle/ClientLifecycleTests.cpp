#include "ClientLifecyclePch.h"
#include "CSerializableBuffer.h"
#include "NetHeaders.h"
#include "NetPacket.h"
#include "CClientMailbox.h"
#include "CNetClientManager.h"
#include "CNetSession.h"
#include "NetworkLimits.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "CLanClient.h"

namespace NetworkLib
{
	class CClientLifecycleProbe
	{
		static_assert(std::is_same_v<decltype(CNetClientManager::m_sessions),
			std::unordered_map<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>>>);

	public:
		static auto FindSession(CNetClientManager &client, SessionId sessionId)
		{
			return client.findSession(sessionId);
		}

		static auto GetFirstSessionEntry(CNetClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			const auto first = client.m_sessions.begin();
			return first != client.m_sessions.end() ? std::make_pair(first->first, first->second)
				: std::pair<SessionId, std::shared_ptr<ClientNetworkLib::CNetSession>>{};
		}

		static auto LockSession(ClientNetworkLib::CNetSession &session)
		{
			return std::unique_lock(session.m_sessionStateLock);
		}

		static bool IsAccepting(CNetClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return client.m_isAccepting;
		}

		static LPFN_CONNECTEX GetCachedConnectEx(CNetClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return client.m_pConnectEx;
		}

		static bool IsConnectPending(ClientNetworkLib::CNetSession &session)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			if (session.m_state != ClientNetworkLib::CNetSession::EState::Connecting || session.m_pendingIo != 1)
				return false;
			DWORD bytes = 0;
			DWORD flags = 0;
			return !WSAGetOverlappedResult(session.m_socket, &session.m_connectOverlapped, &bytes, FALSE, &flags)
				&& WSAGetLastError() == WSA_IO_INCOMPLETE;
		}

		static HANDLE GetLanPort(CLanClientManager &manager)
		{
			return manager.m_iocpHandle;
		}
	};
}

namespace
{
	using namespace NetworkLib;

	void Check(bool isPassed, const wchar_t *pCase)
	{
		if (!isPassed)
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"FAIL: %ls", pCase);
			fflush(nullptr);
			ExitProcess(1);
		}
	}

	SOCKET BindLoopback(USHORT &outPort)
	{
		SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		int addressBytes = sizeof(address);
		Check(socketHandle != INVALID_SOCKET && bind(socketHandle, reinterpret_cast<sockaddr *>(&address), addressBytes) == 0
			&& getsockname(socketHandle, reinterpret_cast<sockaddr *>(&address), &addressBytes) == 0, L"bind loopback");
		outPort = ntohs(address.sin_port);
		return socketHandle;
	}

	SClientEvent WaitEvent(CClientMailbox &mailbox)
	{
		const auto deadline = GetTickCount64() + 7000;
		SClientEvent event{};
		while (!mailbox.TryPop(event))
		{
			Check(GetTickCount64() < deadline, L"mailbox deadline");
			Sleep(1);
		}
		return event;
	}

	SOCKET AcceptPeer(SOCKET listener)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(listener, &readable);
		timeval timeout{ 7, 0 };
		Check(select(0, &readable, nullptr, nullptr, &timeout) == 1, L"accept deadline");
		SOCKET peer = accept(listener, nullptr, nullptr);
		Check(peer != INVALID_SOCKET, L"accept peer");
		DWORD timeoutMilliseconds = 2000;
		Check(setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMilliseconds),
			sizeof(timeoutMilliseconds)) == 0, L"bound fixture receive wait");
		return peer;
	}

	void ExpectOnlyTerminal(CClientMailbox &mailbox, EClientEvent kind, DWORD error)
	{
		SClientEvent event = WaitEvent(mailbox);
		Check(event.m_kind == kind && event.m_error == error, L"expected terminal event and reason");
		Check(!mailbox.TryPop(event), L"terminal event was delivered only once");
	}

	void CheckConnectPending(CNetClientManager &client, SessionId sessionId, const wchar_t *pCase)
	{
		auto spSession = CClientLifecycleProbe::FindSession(client, sessionId);
		Check(spSession != nullptr && CClientLifecycleProbe::IsConnectPending(*spSession), pCase);
	}

	class CLanLoopbackClient : public CLanClient
	{
	public:
		void OnConnect(UINT64 sessionId) noexcept override
		{
			m_connectedId.store(sessionId);
		}

		void OnDisconnect(UINT64 sessionId) noexcept override
		{
			m_disconnectedId.store(sessionId);
		}

		void OnRecv(UINT64 sessionId, std::shared_ptr<CSerializableBufferView<TRUE>> spMessage) noexcept override
		{
			Check(sessionId == m_connectedId.load() && m_spReceived.load() == nullptr, L"LAN delivery on active session");
			m_spReceived.store(std::move(spMessage));
		}

		std::atomic<UINT64> m_connectedId = 0;
		std::atomic<UINT64> m_disconnectedId = 0;
		std::atomic<std::shared_ptr<CSerializableBufferView<TRUE>>> m_spReceived;
	};

	void VerifyLanLoopback()
	{
		CLanClientManager manager;
		CLanLoopbackClient client;
		g_pLanClientManager = &manager;
		Check(manager.Init() && client.Init(1), L"LAN manager and one slot initialized");
		manager.RegisterClient(&client);
		std::thread worker([&manager] { manager.WorkerThread(); });
		USHORT port = 0;
		SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"LAN listen");
		UINT64 oldId = 0;
		for (int attempt = 0; attempt < 2; ++attempt)
		{
			const auto deadline = GetTickCount64() + 7000;
			while (!client.PostConnectEx("127.0.0.1", port))
			{
				Check(GetTickCount64() < deadline, L"LAN opens and reuses the sole slot");
				Sleep(1);
			}
			SOCKET peer = AcceptPeer(listener);
			while (client.m_connectedId.load() == oldId)
			{
				Check(GetTickCount64() < deadline, L"LAN connect installs a fresh session ID");
				Sleep(1);
			}
			const auto sessionId = client.m_connectedId.load();
			if (oldId != 0)
				Check(!client.Disconnect(oldId), L"LAN rejects a stale ID after slot reuse");

			using CLanPacket = CSerializableBuffer<TRUE>;
			auto spMessage = CLanPacket::MakeShared();
			Check(spMessage != nullptr, L"LAN allocates a shared send packet");
			*spMessage << BYTE{42};
			std::weak_ptr wpQueued = spMessage;
			client.EnqueuePacket(sessionId, std::move(spMessage));
			Check(!wpQueued.expired(), L"LAN queue retains a packet without a caller owner");
			spMessage = CLanPacket::MakeShared();
			Check(spMessage != nullptr, L"LAN allocates a second shared send packet");
			*spMessage << BYTE{43};
			client.SendPacket(sessionId, std::move(spMessage));
			constexpr std::array<char, 6> kExpected{ 1, 0, 42, 1, 0, 43 };
			std::array<char, kExpected.size()> received{};
			const auto bytes = static_cast<int>(received.size());
			Check(recv(peer, received.data(), bytes, MSG_WAITALL) == bytes && received == kExpected, L"LAN FIFO wire bytes");
			Check(send(peer, kExpected.data(), 3, 0) == 3, L"LAN peer sends framed data");
			while (client.m_spReceived.load() == nullptr)
			{
				Check(GetTickCount64() < deadline, L"LAN callback receives the peer frame");
				Sleep(1);
			}
			Check(client.Disconnect(sessionId), L"LAN cancels its pending recv");
			while (client.m_disconnectedId.load() != sessionId)
			{
				Check(GetTickCount64() < deadline, L"LAN drains before disconnect callback");
				Sleep(1);
			}
			auto spView = client.m_spReceived.exchange(nullptr);
			std::weak_ptr wpView = spView;
			BYTE value = 0;
			*spView >> value;
			Check(value == 42, L"LAN view retains worker-owned receive storage after disconnect");
			spView.reset();
			Check(wpView.expired() && wpQueued.expired(), L"LAN releases the retained view and queued packet");
			closesocket(peer);
			oldId = sessionId;
		}
		closesocket(listener);
		const HANDLE lanPort = CClientLifecycleProbe::GetLanPort(manager);
		Check(PostQueuedCompletionStatus(lanPort, 0, 0, nullptr), L"stop test LAN worker");
		worker.join();
		CloseHandle(lanPort);
		WSACleanup();
		g_pLanClientManager = nullptr;
		g_pLogger->WriteLogConsole(ELogLevel::System, L"PASS: LAN FIFO wire, cross-thread view return, slot reuse");
	}

	void VerifyLifecycle(DWORD workers)
	{
		constexpr BYTE kPacketCode = 0x77;
		CNetClientManager client;
		Check(!client.Init(0) && CClientLifecycleProbe::GetCachedConnectEx(client) == nullptr,
			L"invalid worker count leaves no cache");
		Check(client.Init(workers), L"manager initialization succeeds");
		const auto pConnectEx = CClientLifecycleProbe::GetCachedConnectEx(client);
		Check(pConnectEx != nullptr, L"initialization caches ConnectEx before any connection");
		Check(!client.Init(workers) && CClientLifecycleProbe::GetCachedConnectEx(client) == pConnectEx,
			L"duplicate initialization preserves cache");
		USHORT port = 0;
		SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"listen");
		auto spFirst = std::make_shared<CClientMailbox>();
		auto spSecond = std::make_shared<CClientMailbox>();
		const auto first = client.Connect("127.0.0.1", port, kPacketCode, spFirst);
		const auto second = client.Connect("127.0.0.1", port, kPacketCode, spSecond);
		Check(first != 0 && second != 0 && first != second, L"two accepted attempts with distinct IDs");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) == pConnectEx, L"multiple attempts share the initialized cache");
		Check(client.Connect("127.0.0.1", port, kPacketCode, spFirst) == 0, L"mailbox is connection-specific");
		SOCKET firstPeer = AcceptPeer(listener);
		SOCKET secondPeer = AcceptPeer(listener);
		Check(WaitEvent(*spFirst).m_kind == EClientEvent::Connected && WaitEvent(*spSecond).m_kind == EClientEvent::Connected,
			L"both attempts complete");
		Check(client.Disconnect(first), L"close one session");
		ExpectOnlyTerminal(*spFirst, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		Check(!client.Disconnect(first), L"old ID cannot disconnect");
		Check(client.Connect("127.0.0.1", port, kPacketCode, spFirst) == 0, L"retired mailbox cannot be rebound");

		auto spNext = std::make_shared<CClientMailbox>();
		const auto next = client.Connect("127.0.0.1", port, kPacketCode, spNext);
		Check(next != 0 && next != first && next != second, L"reconnect returns a new ID");
		SOCKET nextPeer = AcceptPeer(listener);

		auto spMessage = CSerializableBuffer<FALSE>::MakeShared();
		Check(spMessage != nullptr, L"allocate test packet");
		*spMessage << static_cast<BYTE>(42);
		PrepareNetPacket(*spMessage, kPacketCode);
		const int frameBytes = spMessage->GetFullSize();
		Check(send(nextPeer, spMessage->GetBufferPtr(), frameBytes, 0) == frameBytes, L"peer sends a frame before app polls");
		Check(WaitEvent(*spNext).m_kind == EClientEvent::Connected, L"Connected precedes the first packet");
		SClientEvent packet = WaitEvent(*spNext);
		BYTE value = 0;
		Check(packet.m_kind == EClientEvent::Packet && packet.m_spPacket != nullptr && packet.m_spPacket->IsHeapAllocated(),
			L"new mailbox owns its first heap packet");
		auto spReceived = packet.m_spPacket;
		packet = {};
		*spReceived >> value;
		Check(value == 42 && !spFirst->TryPop(packet) && !spSecond->TryPop(packet), L"receive stays in its own mailbox");

		std::weak_ptr wpRejected = spMessage;
		Check(!client.SendPacket(first, std::move(spMessage)) && wpRejected.expired(), L"stale ID does not retain a shared packet");
		auto makePacket = []
		{
			auto spPacket = CSerializableBuffer<FALSE>::MakeShared();
			Check(spPacket != nullptr, L"allocate a temporary send packet");
			*spPacket << BYTE{42};
			return spPacket;
		};
		Check(!client.SendPacket(second, nullptr) && client.SendPacket(second, makePacket()), L"survivor sends a temporary owner");
		SNetHeader header{};
		const auto deadline = GetTickCount64() + 7000;
		std::array<WSAPOLLFD, 2> peers{{ { firstPeer, POLLRDNORM, 0 }, { secondPeer, POLLRDNORM, 0 } }};
		bool hasPacket = false;
		while (!hasPacket && GetTickCount64() < deadline)
		{
			Check(WSAPoll(peers.data(), static_cast<ULONG>(peers.size()), 50) != SOCKET_ERROR, L"poll survivor packet");
			for (auto &peer : peers)
			{
				if (peer.fd == INVALID_SOCKET || !(peer.revents & (POLLRDNORM | POLLHUP | POLLERR)))
					continue;
				int received = recv(peer.fd, reinterpret_cast<char *>(&header), sizeof(header), MSG_WAITALL);
				if (received == 0)
					peer.fd = INVALID_SOCKET;
				else
					hasPacket = received == sizeof(header) && header.code == kPacketCode && header.len == 1;
			}
		}
		Check(hasPacket, L"survivor sends real framed bytes");

		USHORT refusedPort = 0;
		SOCKET nonListener = BindLoopback(refusedPort);
		auto spRefused = std::make_shared<CClientMailbox>();
		Check(client.Connect("127.0.0.1", refusedPort, kPacketCode, spRefused) != 0, L"refused attempt accepted for async I/O");
		SClientEvent refused = WaitEvent(*spRefused);
		Check(refused.m_kind == EClientEvent::ConnectFailed
			&& (refused.m_error == ERROR_CONNECTION_REFUSED || refused.m_error == WSAECONNREFUSED), L"asynchronous refusal");
		Check(!spRefused->TryPop(refused), L"refusal reported once");

		auto spDeadline = std::make_shared<CClientMailbox>();
		Check(client.Connect("127.0.0.1", port, kPacketCode, spDeadline, 0) != 0, L"accept a ConnectEx with an immediate deadline");
		ExpectOnlyTerminal(*spDeadline, EClientEvent::ConnectFailed, WSAETIMEDOUT);

		// accept하지 않아 대기열을 채우면 후속 연결을 Connecting 상태로 유지할 수 있다.
		USHORT saturatedPort = 0;
		SOCKET saturatedListener = BindLoopback(saturatedPort);
		Check(listen(saturatedListener, 0) == 0, L"listen with a minimal backlog");
		auto spFiller = std::make_shared<CClientMailbox>();
		Check(client.Connect("127.0.0.1", saturatedPort, kPacketCode, spFiller) != 0
			&& WaitEvent(*spFiller).m_kind == EClientEvent::Connected, L"fill listener backlog");

		auto spTimedOut = std::make_shared<CClientMailbox>();
		const auto timedOut = client.Connect("127.0.0.1", saturatedPort, kPacketCode, spTimedOut, 1000);
		CheckConnectPending(client, timedOut, L"timeout fixture has outstanding ConnectEx");
		Check(timedOut != 0 && !client.SendPacket(timedOut, makePacket()), L"send rejected while Connecting");
		ExpectOnlyTerminal(*spTimedOut, EClientEvent::ConnectFailed, WSAETIMEDOUT);

		auto spCanceled = std::make_shared<CClientMailbox>();
		const auto canceled = client.Connect("127.0.0.1", saturatedPort, kPacketCode, spCanceled);
		CheckConnectPending(client, canceled, L"ConnectEx still outstanding before Disconnect");
		Check(client.Disconnect(canceled), L"cancel a pending connection");
		ExpectOnlyTerminal(*spCanceled, EClientEvent::ConnectFailed, ERROR_OPERATION_ABORTED);
		const auto retireDeadline = GetTickCount64() + 3000;
		while (CClientLifecycleProbe::FindSession(client, canceled) || CClientLifecycleProbe::FindSession(client, timedOut))
		{
			Check(GetTickCount64() < retireDeadline, L"previous failed connections are retired before the Stop fixture");
			Sleep(1);
		}

		auto spStopPending = std::make_shared<CClientMailbox>();
		const auto stopPending = client.Connect("127.0.0.1", saturatedPort, kPacketCode, spStopPending);
		CheckConnectPending(client, stopPending, L"ConnectEx still outstanding before Stop");
		auto [blockerId, spStopBlocker] = CClientLifecycleProbe::GetFirstSessionEntry(client);
		const auto sendTarget = blockerId == next ? second : next;
		Check(spStopBlocker != nullptr && blockerId != sendTarget, L"Stop has a distinct blocker and send target");
		auto spRejected = CSerializableBuffer<FALSE>::MakeShared();
		Check(spRejected != nullptr, L"allocate unframed shutdown packet");
		*spRejected << static_cast<BYTE>(73);
		std::array<char, sizeof(SNetHeader) + 1> originalFrame{};
		memset(spRejected->GetBufferPtr(), 0, spRejected->GetHeaderSize());
		memcpy(originalFrame.data(), spRejected->GetBufferPtr(), originalFrame.size());
		{
			// 스냅샷의 첫 세션에서 Stop을 멈춰 다른 세션이 닫히기 전에 송신 접수 경계를 확인한다.
			auto blockerSessionStateLock = CClientLifecycleProbe::LockSession(*spStopBlocker);
			std::thread stopper([&client] { client.Stop(); });
			const auto stopDeadline = GetTickCount64() + 3000;
			while (CClientLifecycleProbe::IsAccepting(client))
			{
				Check(GetTickCount64() < stopDeadline, L"Stop closes admission while a session lock is held");
				Sleep(1);
			}
			if (blockerId != stopPending)
				CheckConnectPending(client, stopPending, L"ConnectEx still outstanding after Stop closes admission");
			Check(!client.SendPacket(sendTarget, spRejected), L"Stop must reject send to a not-yet-closed session");
			Check(memcmp(originalFrame.data(), spRejected->GetBufferPtr(), originalFrame.size()) == 0,
				L"shutdown rejection leaves caller packet unframed");
			blockerSessionStateLock.unlock();
			stopper.join();
		}
		spStopBlocker.reset();
		ExpectOnlyTerminal(*spStopPending, EClientEvent::ConnectFailed, ERROR_OPERATION_ABORTED);
		ExpectOnlyTerminal(*spFiller, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		ExpectOnlyTerminal(*spSecond, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		ExpectOnlyTerminal(*spNext, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		spNext.reset();
		std::weak_ptr wpReceived = spReceived;
		Check(spReceived->GetDataSize() == 0 && spReceived.use_count() == 1, L"Net packet outlives mailbox and Stop");
		spReceived.reset();
		Check(wpReceived.expired(), L"Net receive packet releases its last owner");
		Check(!client.SendPacket(second, makePacket()) && !client.Disconnect(next), L"no sessions remain after Stop");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) == nullptr, L"Stop clears the ConnectEx cache");
		Check(client.Connect("127.0.0.1", port, kPacketCode, std::make_shared<CClientMailbox>()) == 0, L"Stop rejects new attempts");
		Check(client.Init(workers), L"manager restarts after draining pending receives");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) != nullptr, L"reinitialization restores the ConnectEx cache");
		auto spRestarted = std::make_shared<CClientMailbox>();
		const auto restarted = client.Connect("127.0.0.1", port, kPacketCode, spRestarted);
		Check(restarted > next && WaitEvent(*spRestarted).m_kind == EClientEvent::Connected, L"IDs remain fresh across Stop/Init");
		client.Stop();
		ExpectOnlyTerminal(*spRestarted, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		closesocket(firstPeer);
		closesocket(secondPeer);
		closesocket(nextPeer);
		closesocket(nonListener);
		closesocket(saturatedListener);
		closesocket(listener);
		g_pLogger->WriteLogConsole(ELogLevel::System,
			L"PASS: %lu worker(s): simultaneous sessions, mailbox order/isolation, reconnect, refusal, timeout, cancel, Stop", workers);
	}
}

int main(void)
{
	g_pLogger = CLogger::GetInstance();
	WSADATA wsaData{};
	Check(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0, L"fixture Winsock startup");
	VerifyLifecycle(1);
	VerifyLifecycle(2);
	VerifyLanLoopback();
	WSACleanup();
	fflush(nullptr);
	// 기존 정적 풀 소멸은 이 테스트 범위가 아니다.
	ExitProcess(0);
}
