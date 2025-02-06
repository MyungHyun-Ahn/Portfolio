#include "pch.h"
#include "CLanServer.h"
#include "CLanSession.h"
#include "CEchoServer.h"


bool CEchoServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
    return true;
}

void CEchoServer::OnAccept(const UINT64 sessionID) noexcept
{
    CSerializableBuffer<TRUE> *buffer = CSerializableBuffer<TRUE>::Alloc();
    *buffer << (UINT64)LOGIN_PACKET;
    SendPacket(sessionID, buffer);
}

void CEchoServer::OnClientLeave(const UINT64 sessionID) noexcept
{
}

void CEchoServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept
{
    __int64 num;
    *message >> num;
    CSerializableBufferView<TRUE>::Free(message);

    CSerializableBuffer<TRUE> *buffer = CSerializableBuffer<TRUE>::Alloc();
    *buffer << num;
    SendPacket(sessionID, buffer);
}
 
void CEchoServer::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView<TRUE>> message) noexcept
{
	__int64 num;
	(*message) >> num;

    CSmartPtr<CSerializableBuffer<TRUE>> buffer = CSmartPtr<CSerializableBuffer<TRUE>>::MakeShared();
	*buffer << num;
    // 네트워크 코드로 보낼 때는 진짜 PTR 버전으로 보냄
	SendPacket(sessionID, buffer.GetRealPtr());
}

void CEchoServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

void CEchoServer::RegisterContentTimerEvent() noexcept
{
}
