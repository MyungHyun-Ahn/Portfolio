#include "pch.h"
#include "CLanServer.h"
#include "CSession.h"
#include "CEchoServer.h"


bool CEchoServer::OnConnectionRequest(const WCHAR *ip, USHORT port)
{
    return true;
}

void CEchoServer::OnAccept(const UINT64 sessionID)
{
    CSerializableBuffer *buffer = CSerializableBuffer::Alloc();
    *buffer << (UINT64)LOGIN_PACKET;
    SendPacket(sessionID, buffer);
}

void CEchoServer::OnClientLeave(const UINT64 sessionID)
{
}

void CEchoServer::OnRecv(const UINT64 sessionID, CSerializableBufferView *message)
{
    __int64 num;
    *message >> num;
    CSerializableBufferView::Free(message);

    // g_Logger->WriteLogConsole(LOG_LEVEL::DEBUG, L"%d", num);

    CSerializableBuffer *buffer = CSerializableBuffer::Alloc();
    *buffer << num;
    SendPacket(sessionID, buffer);
}

void CEchoServer::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView> message)
{
	__int64 num;
	(*message) >> num;

    CSmartPtr<CSerializableBuffer> buffer = CSmartPtr<CSerializableBuffer>::MakeShared();
	*buffer << num;
    // 네트워크 코드로 보낼 때는 진짜 PTR 버전으로 보냄
	SendPacket(sessionID, buffer.GetRealPtr());
}

void CEchoServer::OnError(int errorcode, WCHAR *errMsg)
{
}
