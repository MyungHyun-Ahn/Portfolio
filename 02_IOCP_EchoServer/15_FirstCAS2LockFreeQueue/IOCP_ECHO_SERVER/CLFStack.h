#pragma once

struct StackDebug
{
	LONG64		index;
	DWORD		threadId;
	USHORT		pushOrPop;		// push : 0							
								// pop : 1
	USHORT		data;			// 데이터
	void *tPtr;			// 읽어온 Top
	void *newTPtr;		// 새로 Top 되어야할 것
};

#define USE_LFSTACK_DEBUG

#define LOG_MAX 200000
#define PUSH 0
#define POP 1

extern StackDebug StackLogging[LOG_MAX];
extern LONG64 StackLogIndex;

template<typename Data>
struct StackNode
{
	Data data;
	ULONG_PTR next;
};

template<typename T>
class CLFStack
{
public:
	using Node = StackNode<T>;

	void Push(T data)
	{
		// 새로 Push 할 때만 식별자 발급
		ULONG_PTR ident = InterlockedIncrement(&m_ullCurrentIdentifier);
		ULONG_PTR readTop;
		Node *newTop = s_StackNodePool.Alloc();
		newTop->data = data;
		ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, (ULONG_PTR)newTop);

		do
		{
			// readTop 또한 ident와 합성된 주소
			readTop = m_pTop;
			newTop->next = readTop;
		} while (InterlockedCompareExchange(&m_pTop, combinedNewTop, readTop) != readTop);

		// Push 성공
		InterlockedIncrement(&m_iUseCount);
	}

	bool Pop(T *data)
	{
		ULONG_PTR readTop;
		ULONG_PTR newTop;
		Node *readTopAddr;

		do
		{
			readTop = m_pTop;
			readTopAddr = (Node *)GetAddress(readTop);
			if (readTop == NULL)
			{
				break;
			}
			newTop = readTopAddr->next;
		} while (InterlockedCompareExchange(&m_pTop, newTop, readTop) != readTop);

		if (readTopAddr == NULL)
			return false;

		// Pop 성공
		InterlockedDecrement(&m_iUseCount);

		*data = readTopAddr->data;
		s_StackNodePool.Free(readTopAddr);
		return true;
	}

public:
	ULONG_PTR				m_pTop = NULL;
	ULONG_PTR				m_ullCurrentIdentifier = 0; // ABA 문제를 해결하기 위한 식별자
	inline static CLFMemoryPool<Node>		s_StackNodePool = CLFMemoryPool<Node>(0, false);

	LONG					m_iUseCount = 0;
};

