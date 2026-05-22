


#include "rwlock_api.h"
#include <string.h>

#ifdef DEBUG_TRACE
	#include <stdio.h>
#endif


/* ==================================== 辅助函数 ==================================== */

#ifdef DEBUG_TRACE
	// 获取当前线程ID
	static inline uint64 GetCurrentThreadID() {
		#if defined(_WIN32) || defined(_WIN64)
			return (uint64)GetCurrentThreadId();
		#else
			return (uint64)pthread_self();
		#endif
	}

	// 记录锁操作位置（用于调试）
	static inline void RecordLockPosition(xrwlock pRWLock, const char* file, int line) {
		if ( pRWLock ) {
			pRWLock->LastLockFileName = file;
			pRWLock->LastLockLineNumber = line;
		}
	}

	// 检查递归写锁
	static inline bool CheckRecursiveWriteLock(xrwlock pRWLock) {
		if ( !pRWLock ) return FALSE;
		return (pRWLock->WriterCount > 0) && (pRWLock->WriterThreadID == GetCurrentThreadID());
	}
#endif


/* ==================================== 读写锁创建和销毁 ==================================== */

XXAPI xrwlock xrtRWLockCreate()
{
	xrwlock pRWLock = xrtMalloc(sizeof(xrwlock_struct));
	if ( !pRWLock ) return NULL;
	
	xrtRWLockInit(pRWLock);
	
	#ifdef DEBUG_TRACE
		pRWLock->LockFileName = NULL;
		pRWLock->LockLineNumber = 0;
	#endif
	
	return pRWLock;
}



XXAPI void xrtRWLockDestroy(xrwlock pRWLock)
{
	if ( pRWLock ) {
		#ifdef DEBUG_TRACE
			// 检查是否仍有锁持有者
			if ( pRWLock->ReaderCount > 0 || pRWLock->WriterCount > 0 ) {
				fprintf(stderr, "[WARNING] RWLock destroyed while still locked!\n");
				fprintf(stderr, "  Readers: %u, Writers: %u\n", 
					pRWLock->ReaderCount, pRWLock->WriterCount);
			}
		#endif
		
		xrtRWLockUnit(pRWLock);
		xrtFree(pRWLock);
	}
}



XXAPI void xrtRWLockInit(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		InitializeSRWLock(&pRWLock->Lock);
	#else
		pthread_rwlockattr_t attr;
		pthread_rwlockattr_init(&attr);
		
		// 设置写者优先模式，防止写饥饿
		pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
		
		pthread_rwlock_init(&pRWLock->Lock, &attr);
		pthread_rwlockattr_destroy(&attr);
	#endif
	
	#ifdef DEBUG_TRACE
		memset((void*)&pRWLock->ReaderCount, 0, sizeof(xrwlock_struct) - offsetof(xrwlock_struct, ReaderCount));
	#endif
}



XXAPI void xrtRWLockUnit(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#if defined(_WIN32) || defined(_WIN64)
		// SRWLOCK 不需要显式销毁
	#else
		pthread_rwlock_destroy(&pRWLock->Lock);
	#endif
}


/* ==================================== 读锁操作 ==================================== */

XXAPI void xrtRWLockReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#ifdef DEBUG_TRACE
		pRWLock->WaitingReaders++;
	#endif
	
	#if defined(_WIN32) || defined(_WIN64)
		AcquireSRWLockShared(&pRWLock->Lock);
	#else
		pthread_rwlock_rdlock(&pRWLock->Lock);
	#endif
	
	#ifdef DEBUG_TRACE
		pRWLock->WaitingReaders--;
		pRWLock->ReaderCount++;
	#endif
}



XXAPI bool xrtRWLockTryReadLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	#ifdef DEBUG_TRACE
		pRWLock->WaitingReaders++;
	#endif
	
	BOOL bResult;
	#if defined(_WIN32) || defined(_WIN64)
		bResult = TryAcquireSRWLockShared(&pRWLock->Lock);
	#else
		bResult = pthread_rwlock_tryrdlock(&pRWLock->Lock) == 0;
	#endif
	
	#ifdef DEBUG_TRACE
		if ( bResult ) {
			pRWLock->ReaderCount++;
		}
		pRWLock->WaitingReaders--;
	#endif
	
	return bResult != FALSE;
}



XXAPI void xrtRWLockReadUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#ifdef DEBUG_TRACE
		if ( pRWLock->ReaderCount == 0 ) {
			fprintf(stderr, "[ERROR] RWLock read unlock without lock!\n");
		}
		pRWLock->ReaderCount--;
	#endif
	
	#if defined(_WIN32) || defined(_WIN64)
		ReleaseSRWLockShared(&pRWLock->Lock);
	#else
		pthread_rwlock_unlock(&pRWLock->Lock);
	#endif
}


/* ==================================== 写锁操作 ==================================== */

XXAPI void xrtRWLockWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#ifdef DEBUG_TRACE
		uint64 tid = GetCurrentThreadID();
		pRWLock->WaitingWriters++;
		
		// 检查递归写锁（仅警告，仍然允许）
		if ( CheckRecursiveWriteLock(pRWLock) ) {
			fprintf(stderr, "[WARNING] Recursive write lock detected! Thread: %llu\n", tid);
		}
	#endif
	
	#if defined(_WIN32) || defined(_WIN64)
		AcquireSRWLockExclusive(&pRWLock->Lock);
	#else
		pthread_rwlock_wrlock(&pRWLock->Lock);
	#endif
	
	#ifdef DEBUG_TRACE
		pRWLock->WaitingWriters--;
		pRWLock->WriterCount++;
		pRWLock->WriterThreadID = tid;
		pRWLock->WriterDepth++;
	#endif
}



XXAPI bool xrtRWLockTryWriteLock(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	#ifdef DEBUG_TRACE
		uint64 tid = GetCurrentThreadID();
		pRWLock->WaitingWriters++;
		
		// 检查递归写锁
		if ( CheckRecursiveWriteLock(pRWLock) ) {
			fprintf(stderr, "[WARNING] Recursive try write lock detected! Thread: %llu\n", tid);
		}
	#endif
	
	BOOL bResult;
	#if defined(_WIN32) || defined(_WIN64)
		bResult = TryAcquireSRWLockExclusive(&pRWLock->Lock);
	#else
		bResult = pthread_rwlock_trywrlock(&pRWLock->Lock) == 0;
	#endif
	
	#ifdef DEBUG_TRACE
		if ( bResult ) {
			pRWLock->WriterCount++;
			pRWLock->WriterThreadID = tid;
			pRWLock->WriterDepth++;
		}
		pRWLock->WaitingWriters--;
	#endif
	
	return bResult != FALSE;
}



XXAPI void xrtRWLockWriteUnlock(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#ifdef DEBUG_TRACE
		if ( pRWLock->WriterCount == 0 ) {
			fprintf(stderr, "[ERROR] RWLock write unlock without lock!\n");
			return;
		}
		
		uint64 tid = GetCurrentThreadID();
		if ( pRWLock->WriterThreadID != tid ) {
			fprintf(stderr, "[ERROR] RWLock write unlock from wrong thread!\n");
			fprintf(stderr, "  Holder: %llu, Unlocker: %llu\n", pRWLock->WriterThreadID, tid);
		}
		
		pRWLock->WriterDepth--;
		if ( pRWLock->WriterDepth == 0 ) {
			pRWLock->WriterCount--;
			pRWLock->WriterThreadID = 0;
		}
	#endif
	
	#if defined(_WIN32) || defined(_WIN64)
		ReleaseSRWLockExclusive(&pRWLock->Lock);
	#else
		pthread_rwlock_unlock(&pRWLock->Lock);
	#endif
}


/* ==================================== 锁降级和升级 ==================================== */

XXAPI void xrtRWLockDowngrade(xrwlock pRWLock)
{
	if ( !pRWLock ) return;
	
	#ifdef DEBUG_TRACE
		if ( pRWLock->WriterCount == 0 ) {
			fprintf(stderr, "[ERROR] RWLock downgrade without write lock!\n");
			return;
		}
		
		uint64 tid = GetCurrentThreadID();
		if ( pRWLock->WriterThreadID != tid ) {
			fprintf(stderr, "[ERROR] RWLock downgrade from wrong thread!\n");
			return;
		}
	#endif
	
	#if defined(_WIN32) || defined(_WIN64)
		// Windows SRWLOCK 不直接支持降级，需要先释放写锁再获取读锁
		ReleaseSRWLockExclusive(&pRWLock->Lock);
		AcquireSRWLockShared(&pRWLock->Lock);
	#else
		// Linux pthread_rwlock 支持降级
		pthread_rwlock_t* lock = &pRWLock->Lock;
		
		// 原子操作：写锁转读锁
		pthread_rwlock_unlock(lock);
		pthread_rwlock_rdlock(lock);
	#endif
	
	#ifdef DEBUG_TRACE
		pRWLock->WriterCount--;
		pRWLock->WriterDepth = 0;
		pRWLock->WriterThreadID = 0;
		pRWLock->ReaderCount++;
	#endif
}



XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	
	#ifdef DEBUG_TRACE
		if ( pRWLock->ReaderCount == 0 ) {
			fprintf(stderr, "[ERROR] RWLock upgrade without read lock!\n");
			return FALSE;
		}
		
		pRWLock->ReaderCount--;
	#endif
	
	// 注意：升级操作不是原子的，可能导致死锁！
	// 正确的做法是：释放读锁 -> 获取写锁
	// 如果在释放读锁和获取写锁之间，其他线程已经获取了写锁，则升级失败
	
	#if defined(_WIN32) || defined(_WIN64)
		// 释放读锁
		ReleaseSRWLockShared(&pRWLock->Lock);
		
		// 尝试获取写锁
		if ( TryAcquireSRWLockExclusive(&pRWLock->Lock) ) {
			#ifdef DEBUG_TRACE
				pRWLock->WriterCount++;
				pRWLock->WriterThreadID = GetCurrentThreadID();
				pRWLock->WriterDepth = 1;
			#endif
			return TRUE;
		} else {
			// 升级失败，重新获取读锁
			AcquireSRWLockShared(&pRWLock->Lock);
			#ifdef DEBUG_TRACE
				pRWLock->ReaderCount++;
			#endif
			return FALSE;
		}
	#else
		// Linux pthread_rwlock 也不直接支持升级
		pthread_rwlock_unlock(&pRWLock->Lock);
		
		if ( pthread_rwlock_trywrlock(&pRWLock->Lock) == 0 ) {
			#ifdef DEBUG_TRACE
				pRWLock->WriterCount++;
				pRWLock->WriterThreadID = GetCurrentThreadID();
				pRWLock->WriterDepth = 1;
			#endif
			return TRUE;
		} else {
			pthread_rwlock_rdlock(&pRWLock->Lock);
			#ifdef DEBUG_TRACE
				pRWLock->ReaderCount++;
			#endif
			return FALSE;
		}
	#endif
}


/* ==================================== 调试辅助函数 ==================================== */

#ifdef DEBUG_TRACE

XXAPI bool xrtRWLockIsReadLocked(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	return pRWLock->ReaderCount > 0;
}



XXAPI bool xrtRWLockIsWriteLocked(xrwlock pRWLock)
{
	if ( !pRWLock ) return FALSE;
	return pRWLock->WriterCount > 0;
}



XXAPI uint32 xrtRWLockGetReaderCount(xrwlock pRWLock)
{
	if ( !pRWLock ) return 0;
	return pRWLock->ReaderCount;
}



XXAPI uint32 xrtRWLockGetWaitingReaders(xrwlock pRWLock)
{
	if ( !pRWLock ) return 0;
	return pRWLock->WaitingReaders;
}



XXAPI uint32 xrtRWLockGetWaitingWriters(xrwlock pRWLock)
{
	if ( !pRWLock ) return 0;
	return pRWLock->WaitingWriters;
}

#endif // DEBUG_TRACE
