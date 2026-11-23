#pragma once

#include "NetworkCommonLibPch.h"
#include <pdh.h>
#include <psapi.h>
#include <optional>

#pragma comment(lib, "pdh")
#pragma comment(lib, "psapi")

#include "COverlappedAllocator.h"

namespace ServerNetworkLib
{
	using namespace NetworkCommonLib;
}

#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CServer.h"
#include "SystemEvent.h"
#include "CBaseContents.h"
#include "CContentsThread.h"
