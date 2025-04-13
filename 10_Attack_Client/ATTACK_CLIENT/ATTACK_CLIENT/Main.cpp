#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale>
#include <map>
#include <unordered_map>
#include <deque>

#include "CEncryption.h"

#define SERVER_IP L"106.245.38.108"
// #define SERVER_IP L"127.0.0.1"
constexpr int SERVER_PORT = 10611;
// constexpr int BUF_SIZE = 1000; // 100씩 보낼것

#pragma pack(push, 1)
struct NetHeader
{
	BYTE code = 119;
	USHORT len;
	BYTE randKey;
	BYTE checkSum;
};
#pragma pack(pop)

int main()
{
	int retVal;
	int errVal;

	std::unordered_map<INT64, SOCKET> clientMap;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	BOOL runningFlag = TRUE;
	while (runningFlag)
	{
		// 접속된 모든 클라이언트가 해당 행동을 함
		printf("메뉴\n");
		printf("1. connect\n");
		printf("2. login\n");
		printf("3. echo\n");
		printf("4. only send\n");
		printf("5. custom\n");
		printf("6. exit\n");
		int selVal;
		scanf_s("%d", &selVal);

		switch (selVal)
		{
			// connect
			// 입력한 수 만큼 서버에 connect 요청
		case 1:
		{
			int connectCount = 10;
			scanf_s("%d", &connectCount);

			for (int i = 0; i < connectCount; i++)
			{
				SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
				if (sock == INVALID_SOCKET)
				{
					errVal = WSAGetLastError();
					wprintf(L"socket() Error : %d\n", errVal);
					return 1;
				}

				// connect()
				SOCKADDR_IN serverAddr;
				ZeroMemory(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family = AF_INET;
				InetPton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
				serverAddr.sin_port = htons(SERVER_PORT);
				retVal = connect(sock, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
				if (retVal == SOCKET_ERROR)
				{
					errVal = WSAGetLastError();
					wprintf(L"connect() Error : %d\n", errVal);
					break;
				}

				// accountNo 8001번 부터 할당
				clientMap.insert(std::make_pair(8000 + i, sock));
			}
		}
		break;
		// login
		case 2:
		{
			std::deque<UINT64> disconnectQ;
			for (auto &[key, value] : clientMap)
			{
				{
					char sendBuf[1024];
					int front = sizeof(NetHeader);
					int rear = sizeof(NetHeader);
					// Type 1001
					// AccountNo key
					// SessionKey 대충 아무거나 64
					// int version 1
					WORD *enqueueType = (WORD *)(sendBuf + rear);
					*enqueueType = 1001;
					rear += sizeof(WORD);
					INT64 *enqueueAccountNo = (INT64 *)(sendBuf + rear);
					*enqueueAccountNo = key;
					rear += sizeof(INT64);
					// memset(sendBuf + rear, 0xFF, 64);
					rear += sizeof(char) * 64; // 64 쓰레기값 포함
					int *enqueueVersion = (int *)(sendBuf + rear);
					*enqueueVersion = 1;
					rear += sizeof(int);

					NetHeader *header = (NetHeader *)sendBuf;
					header->code = 119;
					header->len = rear - front;
					header->randKey = 0;
					header->checkSum = CEncryption::CalCheckSum(sendBuf + front, rear - front);
					CEncryption::Encoding(sendBuf + 4, rear - front + 1, header->randKey);

					retVal = send(value, sendBuf, rear, 0);
					if (retVal == SOCKET_ERROR)
					{
						printf("disconnect from server %lld\n", key);
						disconnectQ.push_back(key);
						continue;
					}
				}

				{
					char recvBuf[1024];
					char front = sizeof(NetHeader);
					char rear = 0;
					int received = recv(value, recvBuf, 1024, 0);
					if (received == SOCKET_ERROR)
					{
						printf("disconnect from server %lld\n", key);
						disconnectQ.push_back(key);
						continue;
					}
					else if (received == 0)
					{
						printf("disconnect from server %lld\n", key);
						disconnectQ.push_back(key);
						continue;
					}

					rear = received;
					NetHeader *recvHeader = (NetHeader *)recvBuf;
					CEncryption::Decoding(recvBuf + 4, rear - 4, recvHeader->randKey);
					// 대충 front 부터 꺼내서 확인

					// 성공 했겠지
				}
			}

			for (auto &key : disconnectQ)
			{
				closesocket(clientMap[key]);
				clientMap.erase(key);
			}
		}
		break;
		// echo
		case 3:
		{
			std::deque<UINT64> disconnectQ;
			for (auto &[key, value] : clientMap)
			{
				{
					char sendBuf[4096];
					memset(sendBuf, 0x44, 600);
					int front = sizeof(NetHeader);
					int rear = sizeof(NetHeader);
					// Type 5000
					// AccountNo key
					// SessionKey 대충 아무거나 64
					// int version 1
					WORD *enqueueType = (WORD *)(sendBuf + rear);
					*enqueueType = 5000;
					rear += sizeof(WORD);
					INT64 *enqueueAccountNo = (INT64 *)(sendBuf + rear);
					*enqueueAccountNo = key;
					rear += sizeof(INT64);
					LONGLONG *enqueueSendTick = (LONGLONG *)(sendBuf + rear);
					*enqueueSendTick = key;
					rear += sizeof(LONGLONG);

					NetHeader *header = (NetHeader *)sendBuf;
					header->code = 119;
					header->len = rear - front;
					header->randKey = 0;
					header->checkSum = CEncryption::CalCheckSum(sendBuf + front, header->len);
					CEncryption::Encoding(sendBuf + 4, header->len + 1, header->randKey);

					retVal = send(value, sendBuf, rear, 0);
					if (retVal == SOCKET_ERROR)
					{
						printf("disconnect from server %lld\n", key);
						disconnectQ.push_back(key);
						continue;
					}

					{
						char recvBuf[1024];
						char front = sizeof(NetHeader);
						char rear = 0;
						int received = recv(value, recvBuf, 1024, 0);
						if (received == SOCKET_ERROR)
						{
							printf("disconnect from server %lld\n", key);
							disconnectQ.push_back(key);
							continue;
						}
						else if (received == 0)
						{
							printf("disconnect from server %lld\n", key);
							disconnectQ.push_back(key);
							continue;
						}

						rear = received;
						NetHeader *recvHeader = (NetHeader *)recvBuf;
						CEncryption::Decoding(recvBuf + 4, rear - 4, recvHeader->randKey);
						// 대충 front 부터 꺼내서 확인

						// 성공 했겠지

						front += (sizeof(WORD) + sizeof(INT64));
						LONGLONG *echoVal = (LONGLONG *)(recvBuf + front);
						printf("recv echo %lld %lld\n", key, *echoVal);
					}
				}

				for (auto &key : disconnectQ)
				{
					closesocket(clientMap[key]);
					clientMap.erase(key);
				}
			}
		}
		break;
		// only send
		case 4:
		{
			int roopCount = 0;
			int onlySendRun = TRUE;
			while (onlySendRun)
			{
				roopCount++;
				printf("RoopCount %d\n", roopCount);
				std::deque<UINT64> disconnectQ;
				for (auto &[key, value] : clientMap)
				{
					{
						char sendBuf[1024];
						int front = sizeof(NetHeader);
						int rear = sizeof(NetHeader);
						// Type 5000
						// AccountNo key
						// SessionKey 대충 아무거나 64
						// int version 1
						WORD *enqueueType = (WORD *)(sendBuf + rear);
						*enqueueType = 5000;
						rear += sizeof(WORD);
						INT64 *enqueueAccountNo = (INT64 *)(sendBuf + rear);
						*enqueueAccountNo = key;
						rear += sizeof(INT64);
						LONGLONG *enqueueSendTick = (LONGLONG *)(sendBuf + rear);
						*enqueueSendTick = key;
						rear += sizeof(LONGLONG);

						NetHeader *header = (NetHeader *)sendBuf;
						header->code = 119;
						header->len = rear - front;
						header->randKey = 0;
						header->checkSum = CEncryption::CalCheckSum(sendBuf + front, rear - front);
						CEncryption::Encoding(sendBuf + 4, rear - front + 1, header->randKey);

						retVal = send(value, sendBuf, rear, 0);
						if (retVal == SOCKET_ERROR)
						{
							errVal = WSAGetLastError();
							printf("disconnect from server %lld ErrorCode : %d\n", key, errVal);
							disconnectQ.push_back(key);
						}

						Sleep(0);
					}

					for (auto &key : disconnectQ)
					{
						closesocket(clientMap[key]);
						clientMap.erase(key);
					}
				}

				if (clientMap.empty())
				{
					onlySendRun = FALSE;
				}
			}
		}
		break;
		// custom
		case 5:
			break;

			// exit
		case 6:
		{
			runningFlag = FALSE;
		}
		break;
		default:
			break;
		}
	}

	return 0;
}