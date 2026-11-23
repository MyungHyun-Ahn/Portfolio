#pragma once

#include "NetworkCommonPch.h"
#include <pdh.h>
#include <psapi.h>
#include <optional>

#pragma comment(lib, "pdh")
#pragma comment(lib, "psapi")

#include "COverlappedAllocator.h"

#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CNetServer.h"
#include "SystemEvent.h"
#include "CBaseContents.h"
#include "CContentsThread.h"
