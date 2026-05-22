

// 读写锁实现（高性能、高可靠性）
// 设计原则：
// 1. 跨平台支持（Windows SRWLOCK + Linux pthread_rwlock）
// 2. 高性能：使用原生锁机制
// 3. 高可靠性：死锁检测、递归锁检测
// 4. 公平性：防止写饥饿
// 5. 调试支持：锁持有者跟踪


/* ==================================== 读写锁数据结构 ==================================== */

// 读写锁数据结构
typedef struct {
	// 平台特定的锁句柄
	#if defined(_WIN32) || defined(_WIN64)
		SRWLOCK Lock;					// Windows SRWLOCK（最高性能）
	#else
		pthread_rwlock_t Lock;			// Linux pthread_rwlock
	#endif
	
	// 调试信息（仅在调试模式编译）
	#ifdef DEBUG_TRACE
		uint32 ReaderCount;				// 当前读者数量
		uint32 WriterCount;				// 当前写者数量（应该 <= 1）
		uint64 WriterThreadID;			// 写锁持有者线程ID
		uint32 WriterDepth;				// 写锁递归深度
		uint32 WaitingReaders;			// 等待读锁的线程数
		uint32 WaitingWriters;			// 等待写锁的线程数
		const char* LockFileName;		// 锁创建位置（文件名）
		int LockLineNumber;				// 锁创建位置（行号）
		const char* LastLockFileName;	// 最后一次锁操作位置
		int LastLockLineNumber;
	#endif
} xrwlock_struct, *xrwlock;


/* ==================================== 读写锁操作 ==================================== */

// 创建读写锁
XXAPI xrwlock xrtRWLockCreate();

// 销毁读写锁
XXAPI void xrtRWLockDestroy(xrwlock pRWLock);

// 初始化读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockInit(xrwlock pRWLock);

// 释放读写锁（对自维护结构体指针使用）
XXAPI void xrtRWLockUnit(xrwlock pRWLock);

// ========== 读锁操作 ==========

// 获取读锁（阻塞）
XXAPI void xrtRWLockReadLock(xrwlock pRWLock);

// 尝试获取读锁（非阻塞）
XXAPI bool xrtRWLockTryReadLock(xrwlock pRWLock);

// 释放读锁
XXAPI void xrtRWLockReadUnlock(xrwlock pRWLock);

// ========== 写锁操作 ==========

// 获取写锁（阻塞）
XXAPI void xrtRWLockWriteLock(xrwlock pRWLock);

// 尝试获取写锁（非阻塞）
XXAPI bool xrtRWLockTryWriteLock(xrwlock pRWLock);

// 释放写锁
XXAPI void xrtRWLockWriteUnlock(xrwlock pRWLock);

// ========== 降级和升级操作 ==========

// 写锁降级为读锁（保持锁状态）
XXAPI void xrtRWLockDowngrade(xrwlock pRWLock);

// 读锁升级为写锁（可能失败，需要释放后重新获取）
XXAPI bool xrtRWLockUpgrade(xrwlock pRWLock);

// ========== 调试辅助函数 ==========

#ifdef DEBUG_TRACE
	// 检查锁状态（用于调试）
	XXAPI bool xrtRWLockIsReadLocked(xrwlock pRWLock);
	XXAPI bool xrtRWLockIsWriteLocked(xrwlock pRWLock);
	XXAPI uint32 xrtRWLockGetReaderCount(xrwlock pRWLock);
	XXAPI uint32 xrtRWLockGetWaitingReaders(xrwlock pRWLock);
	XXAPI uint32 xrtRWLockGetWaitingWriters(xrwlock pRWLock);
#endif
