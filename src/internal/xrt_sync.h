#ifndef XRT_INTERNAL_SYNC_H
#define XRT_INTERNAL_SYNC_H

#include "xrt_wait.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
#endif



#if defined(XRT_FEATURE_SYNC)

#define XRT_MUTEX_MAGIC UINT32_C(0x58544D58)
#define XRT_COND_MAGIC UINT32_C(0x58544344)
#define XRT_SEM_MAGIC UINT32_C(0x5854534D)
#define XRT_RWLOCK_MAGIC UINT32_C(0x58545257)
#define XRT_EVENT_MAGIC UINT32_C(0x58544556)



#if defined(XRT_FEATURE_MUTEX)
/* Mutex 的平台内部布局。 */
typedef struct xrt_mutex_impl {
	uint32 Magic;
	#if defined(_WIN32) || defined(_WIN64)
		SRWLOCK Lock;
		volatile LONG Owner;
	#else
		pthread_mutex_t Lock;
		uint64 Owner;
	#endif
} xrt_mutex_impl;

typedef char xrt_mutex_storage_check[
	(sizeof(xrt_mutex_impl) <= XRT_MUTEX_STORAGE_SIZE) ? 1 : -1
];



/* 读取互斥锁内部布局。 */
static inline xrt_mutex_impl* __xrtMutexImpl(xmutex* pMutex)
{
	return (xrt_mutex_impl*)pMutex;
}



/* 原子读取互斥锁当前所有者。 */
static inline uint64 __xrtMutexOwnerLoad(const xrt_mutex_impl* pImpl)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)(uint32)InterlockedCompareExchange(
			(volatile LONG*)&pImpl->Owner,
			0,
			0
		);
	#elif defined(__GNUC__) || defined(__clang__)
		return __atomic_load_n(&pImpl->Owner, __ATOMIC_ACQUIRE);
	#else
		return pImpl->Owner;
	#endif
}



/* 原子发布互斥锁当前所有者。 */
static inline void __xrtMutexOwnerStore(xrt_mutex_impl* pImpl, uint64 iOwner)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange(&pImpl->Owner, (LONG)iOwner);
	#elif defined(__GNUC__) || defined(__clang__)
		__atomic_store_n(&pImpl->Owner, iOwner, __ATOMIC_RELEASE);
	#else
		pImpl->Owner = iOwner;
	#endif
}
#endif



#if defined(XRT_FEATURE_COND)
/* 条件变量的平台内部布局。 */
typedef struct xrt_cond_impl {
	uint32 Magic;
	#if defined(_WIN32) || defined(_WIN64)
		CONDITION_VARIABLE Condition;
	#else
		pthread_cond_t Condition;
		bool Monotonic;
	#endif
} xrt_cond_impl;

typedef char xrt_cond_storage_check[
	(sizeof(xrt_cond_impl) <= XRT_COND_STORAGE_SIZE) ? 1 : -1
];



/* 读取条件变量内部布局。 */
static inline xrt_cond_impl* __xrtCondImpl(xcond* pCond)
{
	return (xrt_cond_impl*)pCond;
}
#endif



#if defined(XRT_FEATURE_SEM)
/* 信号量的平台内部布局。 */
typedef struct xrt_sem_impl {
	uint32 Magic;
	uint32 Maximum;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
	#else
		pthread_mutex_t Lock;
		pthread_cond_t Condition;
		uint32 Value;
		bool Monotonic;
	#endif
} xrt_sem_impl;

typedef char xrt_sem_storage_check[
	(sizeof(xrt_sem_impl) <= XRT_SEM_STORAGE_SIZE) ? 1 : -1
];



/* 读取信号量内部布局。 */
static inline xrt_sem_impl* __xrtSemImpl(xsem* pSem)
{
	return (xrt_sem_impl*)pSem;
}
#endif



#if defined(XRT_FEATURE_RWLOCK)
/* 写者优先读写锁的平台内部布局。 */
typedef struct xrt_rwlock_impl {
	uint32 Magic;
	uint32 Readers;
	uint32 WaitingReaders;
	uint32 WaitingWriters;
	uint64 WriterId;
	bool Writer;
	#if defined(_WIN32) || defined(_WIN64)
		CRITICAL_SECTION Lock;
		CONDITION_VARIABLE ReadCondition;
		CONDITION_VARIABLE WriteCondition;
	#else
		pthread_mutex_t Lock;
		pthread_cond_t ReadCondition;
		pthread_cond_t WriteCondition;
	#endif
} xrt_rwlock_impl;

typedef char xrt_rwlock_storage_check[
	(sizeof(xrt_rwlock_impl) <= XRT_RWLOCK_STORAGE_SIZE) ? 1 : -1
];



/* 读取读写锁内部布局。 */
static inline xrt_rwlock_impl* __xrtRWLockImpl(xrwlock* pLock)
{
	return (xrt_rwlock_impl*)pLock;
}
#endif



#if defined(XRT_FEATURE_EVENT)
/* 事件的平台内部布局。 */
typedef struct xrt_event_impl {
	uint32 Magic;
	bool ManualReset;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
	#else
		pthread_mutex_t Lock;
		pthread_cond_t Condition;
		bool Signaled;
		bool Monotonic;
	#endif
} xrt_event_impl;

typedef char xrt_event_storage_check[
	(sizeof(xrt_event_impl) <= XRT_EVENT_STORAGE_SIZE) ? 1 : -1
];



/* 读取事件内部布局。 */
static inline xrt_event_impl* __xrtEventImpl(xevent* pEvent)
{
	return (xrt_event_impl*)pEvent;
}
#endif



/* 设置同步原语的平台错误。 */
void __xrtSyncSetSystemError(cstr sOperation, int iCode, cstr sMessage);



#if !defined(_WIN32) && !defined(_WIN64) && defined(XRT_FEATURE_WAIT)
/* 把 XRT 截止时间转换为条件变量实际使用的绝对时钟。 */
bool __xrtSyncDeadlineTime(
	xdeadline iDeadline,
	bool bMonotonic,
	struct timespec* pTime
);
#endif

#endif

#endif
