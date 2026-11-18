#pragma once

// NetworkLib PCH용 집계. 구체 네트워크 타입은 CPP에서 공급한다.
#include "MHLib/MHLibPch.h"
#include <mmsystem.h>
#include <pdh.h>
#include <psapi.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "pdh")
#pragma comment(lib, "psapi")
#pragma comment(lib, "dbghelp")
#pragma comment(lib, "synchronization")
#pragma comment(lib, "user32")

namespace NetworkLib
{
	using namespace MHLib::utils;
	using namespace MHLib::containers;
	using namespace MHLib::memory;
	using namespace MHLib::security;
	using namespace MHLib::diagnostics;
}
