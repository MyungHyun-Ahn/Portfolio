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

void CEchoServer::OnError(int errorcode, WCHAR *errMsg)
{
}
