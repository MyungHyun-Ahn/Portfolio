#pragma once

#include <new>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <time.h>

#include "CProfileManager.h"

// Lock-Free
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CTLSSharedMemoryPool.h"
#include "CTLSMemoryPool.h"
#include "CLFStack.h"
#include "CLFQueue.h"

#include "CTLSPagePool.h"
#include "CTLSSharedPagePool.h"