/*
 * XRT Example - Event Loop
 * XRT 范例 - 事件循环
 *
 * Description / 说明:
 *   EN: Demonstrates event-driven programming model with custom event loop
 *       implementation. Shows timer events, I/O events, signal handling,
 *       and callback registration. Implements reactor pattern for asynchronous
 *       event processing. Suitable for network servers and GUI applications.
 *   CN: 演示事件驱动编程模型和自定义事件循环实现。
 *       展示定时器事件、I/O事件、信号处理和回调注册。
 *       实现反应器模式用于异步事件处理。
 *       适用于网络服务器和GUI应用程序。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/advanced_event_loop.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/advanced_event_loop -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Reactor pattern implementation
 *   - Timer event handling
 *   - I/O event multiplexing
 *   - Callback-based architecture
 *   - Asynchronous event processing
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <sys/select.h>
	#include <unistd.h>
	#include <signal.h>
#endif

// Event types
// 事件类型
typedef enum {
	EVENT_TIMER = 1,
	EVENT_IO_READ,
	EVENT_IO_WRITE,
	EVENT_SIGNAL,
	EVENT_CUSTOM
} EventType;

// Forward declarations
// 前向声明
typedef struct EventLoop EventLoop;
typedef struct Event Event;
typedef void (*EventHandler)(EventLoop* pLoop, Event* pEvent, void* pUserData);

// Event structure
// 事件结构
struct Event {
	EventType eType;
	int iDescriptor;			// File descriptor or handle / 文件描述符或句柄
	uint64_t uiTimeoutMs;		// Timeout for timer events / 定时器事件超时时间
	uint64_t uiExpireTime;		// Expiration time (milliseconds) / 到期时间（毫秒）
	EventHandler pfnHandler;	// Event handler callback / 事件处理回调
	void* pUserData;			// User data for callback / 回调的用户数据
	Event* pNext;				// Next event in list / 列表中的下一个事件
};

// Event loop structure
// 事件循环结构
struct EventLoop {
	Event* pEvents;				// Linked list of events / 事件链表
	int bRunning;				// Loop running flag / 循环运行标志
	uint64_t uiStartTime;		// Loop start time / 循环开始时间
};

// Get current time in milliseconds
// 获取当前时间（毫秒）
uint64_t GetCurrentTimeMs()
{
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

// Helper function for minimum
// 最小值辅助函数
uint64_t xrtMin(uint64_t a, uint64_t b)
{
	return (a < b) ? a : b;
}

// Create event loop
// 创建事件循环
EventLoop* EventLoopCreate()
{
	EventLoop* pLoop = xrtMalloc(sizeof(EventLoop));
	pLoop->pEvents = NULL;
	pLoop->bRunning = 0;
	pLoop->uiStartTime = GetCurrentTimeMs();
	return pLoop;
}

// Destroy event loop
// 销毁事件循环
void EventLoopDestroy(EventLoop* pLoop)
{
	if ( pLoop ) {
		Event* pCurrent = pLoop->pEvents;
		while ( pCurrent ) {
			Event* pNext = pCurrent->pNext;
			xrtFree(pCurrent);
			pCurrent = pNext;
		}
		xrtFree(pLoop);
	}
}

// Add event to loop
// 向循环添加事件
Event* EventLoopAddEvent(EventLoop* pLoop, EventType eType, int iDescriptor,
                        uint64_t uiTimeoutMs, EventHandler pfnHandler, void* pUserData)
{
	Event* pEvent = xrtMalloc(sizeof(Event));
	pEvent->eType = eType;
	pEvent->iDescriptor = iDescriptor;
	pEvent->uiTimeoutMs = uiTimeoutMs;
	pEvent->uiExpireTime = GetCurrentTimeMs() + uiTimeoutMs;
	pEvent->pfnHandler = pfnHandler;
	pEvent->pUserData = pUserData;
	pEvent->pNext = pLoop->pEvents;
	pLoop->pEvents = pEvent;
	
	return pEvent;
}

// Remove event from loop
// 从循环移除事件
void EventLoopRemoveEvent(EventLoop* pLoop, Event* pEventToRemove)
{
	Event* pCurrent = pLoop->pEvents;
	Event* pPrev = NULL;
	
	while ( pCurrent ) {
		if ( pCurrent == pEventToRemove ) {
			if ( pPrev ) {
				pPrev->pNext = pCurrent->pNext;
			} else {
				pLoop->pEvents = pCurrent->pNext;
			}
			xrtFree(pCurrent);
			return;
		}
		pPrev = pCurrent;
		pCurrent = pCurrent->pNext;
	}
}

// Find next timeout
// 查找下一个超时时间
uint64_t EventLoopFindNextTimeout(EventLoop* pLoop)
{
	uint64_t uiNow = GetCurrentTimeMs();
	uint64_t uiMinTimeout = UINT64_MAX;
	
	Event* pCurrent = pLoop->pEvents;
	while ( pCurrent ) {
		if ( pCurrent->eType == EVENT_TIMER ) {
			if ( pCurrent->uiExpireTime > uiNow ) {
				uint64_t uiTimeout = pCurrent->uiExpireTime - uiNow;
				if ( uiTimeout < uiMinTimeout ) {
					uiMinTimeout = uiTimeout;
				}
			} else {
				// Already expired
				// 已经过期
				uiMinTimeout = 0;
			}
		}
		pCurrent = pCurrent->pNext;
	}
	
	return (uiMinTimeout == UINT64_MAX) ? 1000 : uiMinTimeout;  // Default 1 second / 默认1秒
}

// Process expired timers
// 处理过期的定时器
void EventLoopProcessTimers(EventLoop* pLoop)
{
	uint64_t uiNow = GetCurrentTimeMs();
	Event* pCurrent = pLoop->pEvents;
	
	while ( pCurrent ) {
		if ( pCurrent->eType == EVENT_TIMER && pCurrent->uiExpireTime <= uiNow ) {
			// Timer expired
			// 定时器到期
			if ( pCurrent->pfnHandler ) {
				pCurrent->pfnHandler(pLoop, pCurrent, pCurrent->pUserData);
			}
			
			// Reschedule periodic timer
			// 重新安排周期性定时器
			if ( pCurrent->uiTimeoutMs > 0 ) {
				pCurrent->uiExpireTime = uiNow + pCurrent->uiTimeoutMs;
			}
		}
		pCurrent = pCurrent->pNext;
	}
}

// Process I/O events (simplified)
// 处理I/O事件（简化版）
void EventLoopProcessIOEvents(EventLoop* pLoop)
{
#ifndef _WIN32
	fd_set readfds, writefds;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	int iMaxFd = -1;
	
	// Prepare file descriptor sets
	// 准备文件描述符集合
	Event* pCurrent = pLoop->pEvents;
	while ( pCurrent ) {
		if ( pCurrent->eType == EVENT_IO_READ ) {
			FD_SET(pCurrent->iDescriptor, &readfds);
			if ( pCurrent->iDescriptor > iMaxFd ) {
				iMaxFd = pCurrent->iDescriptor;
			}
		} else if ( pCurrent->eType == EVENT_IO_WRITE ) {
			FD_SET(pCurrent->iDescriptor, &writefds);
			if ( pCurrent->iDescriptor > iMaxFd ) {
				iMaxFd = pCurrent->iDescriptor;
			}
		}
		pCurrent = pCurrent->pNext;
	}
	
	if ( iMaxFd >= 0 ) {
		struct timeval timeout = {0, 0};  // Non-blocking / 非阻塞
		int iResult = select(iMaxFd + 1, &readfds, &writefds, NULL, &timeout);
		
		if ( iResult > 0 ) {
			// Check which descriptors are ready
			// 检查哪些描述符已就绪
			pCurrent = pLoop->pEvents;
			while ( pCurrent ) {
				if ( (pCurrent->eType == EVENT_IO_READ && 
				      FD_ISSET(pCurrent->iDescriptor, &readfds)) ||
				     (pCurrent->eType == EVENT_IO_WRITE && 
				      FD_ISSET(pCurrent->iDescriptor, &writefds)) ) {
					if ( pCurrent->pfnHandler ) {
						pCurrent->pfnHandler(pLoop, pCurrent, pCurrent->pUserData);
					}
				}
				pCurrent = pCurrent->pNext;
			}
		}
	}
#endif
}

// Run event loop
// 运行事件循环
void EventLoopRun(EventLoop* pLoop)
{
	pLoop->bRunning = 1;
	
	printf("Event loop started\n");
	
	while ( pLoop->bRunning ) {
		// Process timers
		// 处理定时器
		EventLoopProcessTimers(pLoop);
		
		// Process I/O events
		// 处理I/O事件
		EventLoopProcessIOEvents(pLoop);
		
		// Find next timeout and sleep
		// 查找下一个超时时间并休眠
		uint64_t uiTimeout = EventLoopFindNextTimeout(pLoop);
		if ( uiTimeout > 0 ) {
#ifdef _WIN32
			Sleep((DWORD)xrtMin(uiTimeout, 100));  // Max 100ms sleep / 最多休眠100ms
#else
			usleep((useconds_t)xrtMin(uiTimeout * 1000, 100000));  // Max 100ms sleep / 最多休眠100ms
#endif
		}
	}
	
	printf("Event loop stopped\n");
}

// Stop event loop
// 停止事件循环
void EventLoopStop(EventLoop* pLoop)
{
	pLoop->bRunning = 0;
}

// Timer event handler
// 定时器事件处理程序
void TimerEventHandler(EventLoop* pLoop, Event* pEvent, void* pUserData)
{
	int* piCounter = (int*)pUserData;
	(*piCounter)++;
	
	uint64_t uiElapsed = GetCurrentTimeMs() - pLoop->uiStartTime;
	printf("Timer event #%d at %llu ms\n", *piCounter, uiElapsed);
	
	// Stop after 5 events
	// 5次事件后停止
	if ( *piCounter >= 5 ) {
		EventLoopStop(pLoop);
	}
}

// Custom event handler
// 自定义事件处理程序
void CustomEventHandler(EventLoop* pLoop, Event* pEvent, void* pUserData)
{
	str sMessage = (str)pUserData;
	uint64_t uiElapsed = GetCurrentTimeMs() - pLoop->uiStartTime;
	printf("Custom event: %s at %llu ms\n", sMessage, uiElapsed);
}

// Signal handler (Unix only)
// 信号处理程序（仅Unix）
#ifndef _WIN32
void SignalHandler(int iSignal)
{
	printf("Received signal %d\n", iSignal);
}
#endif

// Test timer events
// 测试定时器事件
void TestTimerEvents()
{
	printf("=== Timer Events Test ===\n");
	printf("=== 定时器事件测试 ===\n");
	
	EventLoop* pLoop = EventLoopCreate();
	int iCounter = 0;
	
	// Add periodic timer (every 500ms)
	// 添加周期性定时器（每500ms）
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 500, TimerEventHandler, &iCounter);
	
	// Add one-shot timer (after 2 seconds)
	// 添加一次性定时器（2秒后）
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 2000, CustomEventHandler, "One-shot timer");
	
	// Run loop
	// 运行循环
	EventLoopRun(pLoop);
	
	EventLoopDestroy(pLoop);
}

// Test custom events
// 测试自定义事件
void TestCustomEvents()
{
	printf("\n=== Custom Events Test ===\n");
	printf("=== 自定义事件测试 ===\n");
	
	EventLoop* pLoop = EventLoopCreate();
	
	// Add several custom events with different timeouts
	// 添加几个具有不同超时时间的自定义事件
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 300, CustomEventHandler, "First event");
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 700, CustomEventHandler, "Second event");
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 1200, CustomEventHandler, "Third event");
	
	// Add stop event
	// 添加停止事件
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 2000, 
	                 (EventHandler)EventLoopStop, pLoop);
	
	EventLoopRun(pLoop);
	EventLoopDestroy(pLoop);
}

// Test signal handling (Unix only)
// 测试信号处理（仅Unix）
void TestSignalHandling()
{
#ifndef _WIN32
	printf("\n=== Signal Handling Test ===\n");
	printf("=== 信号处理测试 ===\n");
	
	// Register signal handler
	// 注册信号处理程序
	signal(SIGINT, SignalHandler);
	
	EventLoop* pLoop = EventLoopCreate();
	
	printf("Press Ctrl+C to send SIGINT signal...\n");
	printf("Loop will run for 5 seconds or until signal received\n");
	
	// Add timeout to stop loop
	// 添加超时以停止循环
	EventLoopAddEvent(pLoop, EVENT_TIMER, -1, 5000, 
	                 (EventHandler)EventLoopStop, pLoop);
	
	EventLoopRun(pLoop);
	EventLoopDestroy(pLoop);
#endif
}

int main()
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Event Loop Demo\n");
	printf("XRT 事件循环演示\n");
	printf("==================\n\n");
	
	// Run tests
	// 运行测试
	TestTimerEvents();
	TestCustomEvents();
	TestSignalHandling();
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}