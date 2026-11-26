#include "pch.h"

// 게임 서버를 시작하고 종료 명령 또는 입력 종료를 기다린다.
int main()
{
	std::cout << "event=game_server_started | 게임 서버가 시작됐습니다." << std::endl;
	std::string command;
	while (std::getline(std::cin, command))
	{
		if (command == "/exit")
		{
			break;
		}
	}
	std::cout << "event=game_server_stopped | 게임 서버가 종료됐습니다." << std::endl;
	return 0;
}
