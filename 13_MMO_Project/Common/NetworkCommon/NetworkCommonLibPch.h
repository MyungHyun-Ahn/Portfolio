#pragma once

// 공용 네트워크 PCH용 집계. 구체 네트워크 타입은 CPP에서 공급한다.
#include "MHLib/MHLibPch.h"
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "NetHeaders.h"
#include "NetworkLimits.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "dbghelp")
#pragma comment(lib, "synchronization")
#pragma comment(lib, "user32")

namespace NetworkCommonLib
{
	using namespace MHLib::utils;
	using namespace MHLib::containers;
	using namespace MHLib::memory;
	using namespace MHLib::security;
	using namespace MHLib::diagnostics;
}
