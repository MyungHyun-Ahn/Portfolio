#include "MHLib/MHLibPch.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Server/GameDataAll.g.h"
#include "Client/GameDataAll.g.h"

namespace Server = GameData::Server;
namespace Client = GameData::Client;

// 정상 저작 파일은 두 대상 묶음의 실제 정적 조회로 확인한다.
static int CheckAuthored(const std::filesystem::path& data)
{
	auto server = Server::CGameDataTables::Load(data / "Server");
	auto client = Client::CGameDataTables::Load(data / "Client");
	if (!server || !client)
	{
		std::fprintf(stderr, "authored bundle: server=%d client=%d\n", !!server, !!client);
		return 1;
	}
	const auto* s = Server::World::CMapTable::Find(1);
	const auto* c = Client::World::CMapTable::Find(1);
	if (!s || !c || s->GetMapId() != 1 || c->GetMapId() != 1 || s->GetMapType() != GameData::MapType::Town ||
		s->GetMinXCm() != -5000 || s->GetMinYCm() != -5000 || s->GetMaxXCm() != 5000 || s->GetMaxYCm() != 5000 ||
		s->GetSpawnXCm() != 0 || s->GetSpawnYCm() != 0 || c->GetLevelAssetPath() != "/Game/TopDown/Lvl_TopDown")
	{
		return 2;
	}
	return Server::World::CMapTable::Find(2) || Client::World::CMapTable::Find(2) ? 3 : 0;
}

template<class Map, class Player, class Bundle>
static int CheckCandidate(const std::filesystem::path& root, const char* file, bool valid)
{
	Map map;
	if (!map.Load(root / file) || !map.Init() || map.Validate() != valid)
	{
		return 4;
	}
	auto bundle = Bundle::Load(root);
	if (valid)
	{
		return bundle && Map::Find(1) && Map::Find(2) && Player::Find(1) ? 0 : 5;
	}
	return !bundle && !Map::Find(1) && !Map::Find(2) && !Player::Find(1) ? 0 : 6;
}

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		return CheckAuthored(argv[1]);
	}
	if (argc != 4)
	{
		return 7;
	}
	const bool valid = std::string_view(argv[3]) == "valid";
	if (std::string_view(argv[2]) == "Server")
	{
		return CheckCandidate<Server::World::CMapTable, Server::Player::CPlayerMovementTable, Server::CGameDataTables>(
			argv[1], "World/Map.sdat", valid);
	}
	return CheckCandidate<Client::World::CMapTable, Client::Player::CPlayerMovementTable, Client::CGameDataTables>(
		argv[1], "World/Map.cdat", valid);
}
