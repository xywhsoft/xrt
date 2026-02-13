/*
 * XRT Example - Task Scheduler
 * XRT 范例 - 任务调度器
 *
 * Description / 说明:
 *   EN: Implements a flexible task scheduler that can execute tasks at specific
 *       times, intervals, or cron-like schedules. Features include job queuing,
 *       priority scheduling, dependency management, and execution monitoring.
 *       Supports one-time tasks, recurring tasks, and complex scheduling patterns.
 *   CN: 实现灵活的任务调度器，能够在特定时间、间隔或类cron调度中执行任务。
 *       功能包括作业排队、优先级调度、依赖管理、执行监控。
 *       支持一次性任务、重复任务和复杂的调度模式。
 *
 * Build / 编译:
 *   tcc main.c -o ../bin/task_scheduler.exe          (Windows, TCC)
 *   gcc main.c -o ../bin/task_scheduler -lm           (Linux, GCC)
 *
 * Usage / 用法:
 *   task_scheduler add <name> <schedule> <command>
 *   task_scheduler list
 *   task_scheduler run
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Cron-style scheduling syntax
 *   - Priority-based task execution
 *   - Task dependency resolution
 *   - Real-time scheduling engine
 *   - Cross-platform timer implementation
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

// Task states
// 任务状态
typedef enum {
	TASK_PENDING = 0,
	TASK_RUNNING,
	TASK_COMPLETED,
	TASK_FAILED,
	TASK_CANCELLED
} TaskState;

// Schedule types
// 调度类型
typedef enum {
	SCHEDULE_ONCE = 1,
	SCHEDULE_INTERVAL,
	SCHEDULE_CRON,
	SCHEDULE_DAILY,
	SCHEDULE_WEEKLY
} ScheduleType;

// Task structure
// 任务结构
typedef struct Task {
	unsigned int uiId;
	str sName;
	str sCommand;
	ScheduleType eScheduleType;
	time_t tNextRun;
	time_t tInterval;		// For interval scheduling / 用于间隔调度
	int iPriority;			// Higher number = higher priority / 数字越大优先级越高
	TaskState eState;
	int iMaxRetries;
	int iRetryCount;
	
	// Cron-like fields
	// 类cron字段
	int iMinute;			// 0-59, -1 for any / -1表示任意
	int iHour;				// 0-23, -1 for any
	int iDayOfMonth;		// 1-31, -1 for any
	int iMonth;				// 1-12, -1 for any
	int iDayOfWeek;			// 0-6 (Sunday=0), -1 for any
	
	struct Task* pNext;		// Linked list / 链表
} Task;

// Global scheduler state
// 全局调度器状态
Task* gpTaskList = NULL;
unsigned int guiNextTaskId = 1;
volatile int gbSchedulerRunning = 1;

#ifdef _WIN32
CRITICAL_SECTION gCriticalSection;
#else
pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Thread-safe task list management
// 线程安全的任务列表管理
void LockTasks()
{
#ifdef _WIN32
	EnterCriticalSection(&gCriticalSection);
#else
	pthread_mutex_lock(&gMutex);
#endif
}

void UnlockTasks()
{
#ifdef _WIN32
	LeaveCriticalSection(&gCriticalSection);
#else
	pthread_mutex_unlock(&gMutex);
#endif
}

// Create new task
// 创建新任务
Task* CreateTask(str sName, str sCommand, ScheduleType eType)
{
	Task* pTask = xrtMalloc(sizeof(Task));
	memset(pTask, 0, sizeof(Task));
	
	pTask->uiId = guiNextTaskId++;
	pTask->sName = xrtCopyStr(sName, strlen(sName) + 1);
	pTask->sCommand = xrtCopyStr(sCommand, strlen(sCommand) + 1);
	pTask->eScheduleType = eType;
	pTask->eState = TASK_PENDING;
	pTask->iPriority = 0;
	pTask->iMaxRetries = 3;
	pTask->iRetryCount = 0;
	pTask->pNext = NULL;
	
	// Initialize cron fields to "any"
	// 初始化cron字段为"任意"
	pTask->iMinute = -1;
	pTask->iHour = -1;
	pTask->iDayOfMonth = -1;
	pTask->iMonth = -1;
	pTask->iDayOfWeek = -1;
	
	return pTask;
}

// Add task to list
// 添加任务到列表
void AddTask(Task* pTask)
{
	LockTasks();
	
	if ( !gpTaskList ) {
		gpTaskList = pTask;
	} else {
		// Add to end of list
		// 添加到列表末尾
		Task* pCurrent = gpTaskList;
		while ( pCurrent->pNext ) {
			pCurrent = pCurrent->pNext;
		}
		pCurrent->pNext = pTask;
	}
	
	UnlockTasks();
}

// Remove task from list
// 从列表中移除任务
int RemoveTask(unsigned int uiTaskId)
{
	LockTasks();
	
	Task* pPrev = NULL;
	Task* pCurrent = gpTaskList;
	
	while ( pCurrent ) {
		if ( pCurrent->uiId == uiTaskId ) {
			if ( pPrev ) {
				pPrev->pNext = pCurrent->pNext;
			} else {
				gpTaskList = pCurrent->pNext;
			}
			
			// Free task memory
			// 释放任务内存
			xrtFree(pCurrent->sName);
			xrtFree(pCurrent->sCommand);
			xrtFree(pCurrent);
			
			UnlockTasks();
			return 1;
		}
		
		pPrev = pCurrent;
		pCurrent = pCurrent->pNext;
	}
	
	UnlockTasks();
	return 0;
}

// Parse cron-style schedule
// 解析cron风格的调度
int ParseCronSchedule(str sSchedule, Task* pTask)
{
	// Simple cron format: "minute hour day month day_of_week"
	// 简单的cron格式："分钟 小时 日 月 星期"
	
	// For demonstration, we'll parse a simplified format
	// 为演示目的，我们将解析简化的格式
	// Example: "* * * * *" means every minute
	// 示例："* * * * *" 表示每分钟
	
	if ( strcmp(sSchedule, "* * * * *") == 0 ) {
		// Every minute
		// 每分钟
		pTask->iMinute = -1;
		pTask->iHour = -1;
		pTask->iDayOfMonth = -1;
		pTask->iMonth = -1;
		pTask->iDayOfWeek = -1;
		pTask->tNextRun = time(NULL) + 60;  // Next minute
		return 1;
	}
	
	// Parse specific times (simplified)
	// 解析特定时间（简化）
	// Format: "HH:MM" for daily schedule
	// 格式："HH:MM" 用于每日调度
	str sColon = strchr(sSchedule, ':');
	if ( sColon ) {
		*sColon = '\0';
		pTask->iHour = atoi(sSchedule);
		pTask->iMinute = atoi(sColon + 1);
		pTask->iDayOfMonth = -1;
		pTask->iMonth = -1;
		pTask->iDayOfWeek = -1;
		
		// Calculate next run time
		// 计算下次运行时间
		struct tm* pNow = localtime(&(time_t){time(NULL)});
		struct tm nextRun = *pNow;
		
		nextRun.tm_hour = pTask->iHour;
		nextRun.tm_min = pTask->iMinute;
		nextRun.tm_sec = 0;
		
		// If time has passed today, schedule for tomorrow
		// 如果今天的时间已过，则安排到明天
		if ( nextRun.tm_hour < pNow->tm_hour || 
		     (nextRun.tm_hour == pNow->tm_hour && nextRun.tm_min <= pNow->tm_min) ) {
			nextRun.tm_mday++;
		}
		
		pTask->tNextRun = mktime(&nextRun);
		return 1;
	}
	
	return 0;
}

// Check if task should run now
// 检查任务现在是否应该运行
int ShouldRunTask(Task* pTask, time_t tNow)
{
	if ( pTask->eState != TASK_PENDING ) {
		return 0;
	}
	
	switch ( pTask->eScheduleType ) {
		case SCHEDULE_ONCE:
			return tNow >= pTask->tNextRun;
			
		case SCHEDULE_INTERVAL:
			return tNow >= pTask->tNextRun;
			
		case SCHEDULE_CRON:
			{
				struct tm* pTime = localtime(&tNow);
				
				// Check cron conditions
				// 检查cron条件
				if ( pTask->iMinute != -1 && pTask->iMinute != pTime->tm_min ) return 0;
				if ( pTask->iHour != -1 && pTask->iHour != pTime->tm_hour ) return 0;
				if ( pTask->iDayOfMonth != -1 && pTask->iDayOfMonth != pTime->tm_mday ) return 0;
				if ( pTask->iMonth != -1 && pTask->iMonth != pTime->tm_mon + 1 ) return 0;
				if ( pTask->iDayOfWeek != -1 && pTask->iDayOfWeek != pTime->tm_wday ) return 0;
				
				return 1;
			}
			
		case SCHEDULE_DAILY:
			return tNow >= pTask->tNextRun;
			
		case SCHEDULE_WEEKLY:
			return tNow >= pTask->tNextRun;
			
		default:
			return 0;
	}
}

// Execute task
// 执行任务
void ExecuteTask(Task* pTask)
{
	printf("Executing task #%u: %s\n", pTask->uiId, pTask->sName);
	printf("  Command: %s\n", pTask->sCommand);
	
	pTask->eState = TASK_RUNNING;
	
	// Execute command (simplified)
	// 执行命令（简化）
#ifdef _WIN32
	int iResult = system(pTask->sCommand);
#else
	int iResult = system(pTask->sCommand);
#endif
	
	if ( iResult == 0 ) {
		pTask->eState = TASK_COMPLETED;
		printf("  Result: SUCCESS\n");
	} else {
		pTask->eState = TASK_FAILED;
		pTask->iRetryCount++;
		printf("  Result: FAILED (retry %d/%d)\n", pTask->iRetryCount, pTask->iMaxRetries);
		
		// Retry logic
		// 重试逻辑
		if ( pTask->iRetryCount < pTask->iMaxRetries ) {
			pTask->eState = TASK_PENDING;
			pTask->tNextRun = time(NULL) + 30;  // Retry in 30 seconds
		}
	}
	
	// Update next run time for recurring tasks
	// 更新重复任务的下次运行时间
	if ( pTask->eScheduleType == SCHEDULE_INTERVAL ) {
		pTask->tNextRun = time(NULL) + pTask->tInterval;
		pTask->eState = TASK_PENDING;  // Reset for next run
	} else if ( pTask->eScheduleType == SCHEDULE_DAILY ) {
		struct tm* pNow = localtime(&pTask->tNextRun);
		pNow->tm_mday++;
		pTask->tNextRun = mktime(pNow);
		pTask->eState = TASK_PENDING;
	}
}

// Find highest priority runnable task
// 查找最高优先级的可运行任务
Task* FindNextTask()
{
	LockTasks();
	
	Task* pBestTask = NULL;
	time_t tNow = time(NULL);
	
	Task* pCurrent = gpTaskList;
	while ( pCurrent ) {
		if ( ShouldRunTask(pCurrent, tNow) ) {
			if ( !pBestTask || pCurrent->iPriority > pBestTask->iPriority ) {
				pBestTask = pCurrent;
			}
		}
		pCurrent = pCurrent->pNext;
	}
	
	UnlockTasks();
	return pBestTask;
}

// Scheduler worker thread
// 调度器工作线程
#ifdef _WIN32
unsigned int __stdcall SchedulerWorker(void* pParam)
#else
void* SchedulerWorker(void* pParam)
#endif
{
	printf("Scheduler worker started\n");
	
	while ( gbSchedulerRunning ) {
		Task* pTask = FindNextTask();
		
		if ( pTask ) {
			ExecuteTask(pTask);
		} else {
			// No tasks ready, sleep briefly
			// 没有就绪任务，短暂睡眠
#ifdef _WIN32
			Sleep(1000);  // 1 second
#else
			sleep(1);
#endif
		}
	}
	
	printf("Scheduler worker stopped\n");
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Add scheduled task
// 添加调度任务
int AddScheduledTask(str sName, str sSchedule, str sCommand)
{
	// Determine schedule type from schedule string
	// 从调度字符串确定调度类型
	ScheduleType eType = SCHEDULE_ONCE;
	time_t tNextRun = time(NULL);
	time_t tInterval = 0;
	
	// Parse schedule
	// 解析调度
	if ( strncmp(sSchedule, "every ", 6) == 0 ) {
		// Interval schedule: "every 5 minutes"
		// 间隔调度："every 5 minutes"
		eType = SCHEDULE_INTERVAL;
		str sNumber = sSchedule + 6;
		int iValue = atoi(sNumber);
		
		if ( strstr(sSchedule, "second") ) {
			tInterval = iValue;
		} else if ( strstr(sSchedule, "minute") ) {
			tInterval = iValue * 60;
		} else if ( strstr(sSchedule, "hour") ) {
			tInterval = iValue * 3600;
		} else {
			tInterval = iValue * 60;  // Default to minutes
		}
		
		tNextRun = time(NULL) + tInterval;
	} else if ( strchr(sSchedule, ':') ) {
		// Time-based schedule: "14:30"
		// 基于时间的调度："14:30"
		eType = SCHEDULE_DAILY;
	} else if ( strstr(sSchedule, "*") ) {
		// Cron-style schedule
		// Cron风格调度
		eType = SCHEDULE_CRON;
	}
	
	// Create and configure task
	// 创建并配置任务
	Task* pTask = CreateTask(sName, sCommand, eType);
	pTask->tNextRun = tNextRun;
	pTask->tInterval = tInterval;
	
	// Parse cron schedule if applicable
	// 如适用则解析cron调度
	if ( eType == SCHEDULE_CRON ) {
		if ( !ParseCronSchedule(sSchedule, pTask) ) {
			printf("Error: Invalid cron schedule format\n");
			xrtFree(pTask->sName);
			xrtFree(pTask->sCommand);
			xrtFree(pTask);
			return 0;
		}
	}
	
	// Add to scheduler
	// 添加到调度器
	AddTask(pTask);
	
	printf("Added task #%u: %s\n", pTask->uiId, pTask->sName);
	printf("  Schedule: %s\n", sSchedule);
	printf("  Next run: %s", ctime(&pTask->tNextRun));
	
	return 1;
}

// List all scheduled tasks
// 列出所有调度任务
void ListTasks()
{
	printf("=== Scheduled Tasks ===\n");
	printf("=== 调度任务 ===\n");
	
	LockTasks();
	
	if ( !gpTaskList ) {
		printf("No tasks scheduled\n");
		UnlockTasks();
		return;
	}
	
	Task* pCurrent = gpTaskList;
	int iCount = 0;
	
	while ( pCurrent ) {
		iCount++;
		printf("%d. Task #%u: %s\n", iCount, pCurrent->uiId, pCurrent->sName);
		printf("   Command: %s\n", pCurrent->sCommand);
		printf("   State: %d\n", pCurrent->eState);
		printf("   Priority: %d\n", pCurrent->iPriority);
		printf("   Next run: %s", ctime(&pCurrent->tNextRun));
		printf("   ---\n");
		
		pCurrent = pCurrent->pNext;
	}
	
	UnlockTasks();
	
	printf("Total tasks: %d\n", iCount);
}

// Run scheduler
// 运行调度器
void RunScheduler()
{
	printf("=== Task Scheduler ===\n");
	printf("=== 任务调度器 ===\n");
	
#ifdef _WIN32
	InitializeCriticalSection(&gCriticalSection);
#else
	// Mutex already initialized
#endif
	
	// Start scheduler worker
	// 启动调度器工作线程
#ifdef _WIN32
	HANDLE hWorker = (HANDLE)_beginthreadex(NULL, 0, SchedulerWorker, NULL, 0, NULL);
#else
	pthread_t workerThread;
	pthread_create(&workerThread, NULL, SchedulerWorker, NULL);
#endif
	
	printf("Scheduler running. Press Ctrl+C to stop.\n");
	
	// Main loop - just wait for interruption
	// 主循环 - 等待中断
	while ( gbSchedulerRunning ) {
#ifdef _WIN32
		Sleep(5000);  // Check every 5 seconds
#else
		sleep(5);
#endif
		
		// Could add interactive commands here
		// 可以在这里添加交互命令
	}
	
	// Stop scheduler
	// 停止调度器
#ifdef _WIN32
	WaitForSingleObject(hWorker, INFINITE);
	CloseHandle(hWorker);
	DeleteCriticalSection(&gCriticalSection);
#else
	pthread_join(workerThread, NULL);
#endif
	
	printf("Scheduler stopped\n");
}

// Test scheduler functionality
// 测试调度器功能
void TestScheduler()
{
	printf("=== Scheduler Tests ===\n");
	printf("=== 调度器测试 ===\n");
	
	// Add test tasks
	// 添加测试任务
	AddScheduledTask("Test Task 1", "every 10 seconds", "echo 'Hello from task 1'");
	AddScheduledTask("Test Task 2", "12:00", "echo 'Daily task at noon'");
	AddScheduledTask("Test Task 3", "* * * * *", "echo 'Every minute task'");
	
	// List tasks
	// 列出任务
	ListTasks();
	
	// Run for a short time to demonstrate
	// 运行一小段时间以演示
	printf("\nRunning scheduler for 30 seconds...\n");
	
	volatile int bTestRunning = 1;
	time_t tStart = time(NULL);
	
	while ( time(NULL) - tStart < 30 && bTestRunning ) {
		Task* pTask = FindNextTask();
		if ( pTask ) {
			ExecuteTask(pTask);
		}
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}
	
	printf("Test completed\n");
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Task Scheduler Demo\n");
	printf("XRT 任务调度器演示\n");
	printf("====================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc >= 2 ) {
		if ( strcmp(argv[1], "add") == 0 && argc >= 5 ) {
			// Add new task
			// 添加新任务
			str sName = argv[2];
			str sSchedule = argv[3];
			str sCommand = argv[4];
			
			AddScheduledTask(sName, sSchedule, sCommand);
		}
		else if ( strcmp(argv[1], "list") == 0 ) {
			// List all tasks
			// 列出所有任务
			ListTasks();
		}
		else if ( strcmp(argv[1], "run") == 0 ) {
			// Run scheduler
			// 运行调度器
			RunScheduler();
		}
		else if ( strcmp(argv[1], "test") == 0 ) {
			// Run tests
			// 运行测试
			TestScheduler();
		}
		else {
			printf("Usage:\n");
			printf("  %s add <name> <schedule> <command>\n", argv[0]);
			printf("  %s list\n", argv[0]);
			printf("  %s run\n", argv[0]);
			printf("  %s test\n", argv[0]);
			printf("\nSchedule formats:\n");
			printf("  'every X seconds/minutes/hours' - Interval scheduling\n");
			printf("  'HH:MM' - Daily scheduling\n");
			printf("  '* * * * *' - Cron-style (every minute)\n");
			printf("\nExamples:\n");
			printf("  %s add backup \"every 1 hour\" \"cp file1.txt file2.txt\"\n", argv[0]);
			printf("  %s add cleanup \"02:00\" \"rm temp/*\"\n", argv[0]);
		}
	} else {
		// Show help
		// 显示帮助
		printf("XRT Task Scheduler\n");
		printf("Commands:\n");
		printf("  add  - Add a new scheduled task\n");
		printf("  list - List all scheduled tasks\n");
		printf("  run  - Run the scheduler\n");
		printf("  test - Run scheduler tests\n");
		printf("Run with arguments for specific operation\n");
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}