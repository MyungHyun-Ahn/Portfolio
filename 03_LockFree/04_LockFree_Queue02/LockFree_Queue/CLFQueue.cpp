#include <new>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CLFQueue.h"


QueueDebug logging[200000];
LONG64 logIndex = 0;