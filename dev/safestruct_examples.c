

// ====================================
// 线程安全的数据结构封装示例
// ====================================

#include "rwlock_api.h"
#include "xrt.h"


/* ==================================== 线程安全的 List ==================================== */

typedef struct {
	xlist List;
	xrwlock Lock;
} SafeList;

// 创建
SafeList* SafeList_Create(uint32 iItemLength) {
	SafeList* p = xrtMalloc(sizeof(SafeList));
	if ( !p ) return NULL;
	
	p->List = xrtListCreate(iItemLength, XRT_OBJMODE_LOCAL);
	p->Lock = xrtRWLockCreate();
	
	return p;
}

// 销毁
void SafeList_Destroy(SafeList* p) {
	if ( p ) {
		xrtRWLockWriteLock(p->Lock);
		xrtListDestroy(p->List);
		xrtRWLockWriteUnlock(p->Lock);
		
		xrtRWLockDestroy(p->Lock);
		xrtFree(p);
	}
}

// 获取值（读操作）
xvalue SafeList_GetValue(SafeList* p, int64 index) {
	xrtRWLockReadLock(p->Lock);
	xvalue val = xvoListGetValue(p->List, index);
	xrtRWLockReadUnlock(p->Lock);
	return val;
}

// 设置值（写操作）
void SafeList_SetValue(SafeList* p, int64 index, xvalue val, bool bColloc) {
	xrtRWLockWriteLock(p->Lock);
	xvoListSetValue(p->List, index, val, bColloc);
	xrtRWLockWriteUnlock(p->Lock);
}

// 获取元素数量（读操作）
uint32 SafeList_GetCount(SafeList* p) {
	xrtRWLockReadLock(p->Lock);
	uint32 count = xvoListItemCount(p->List);
	xrtRWLockReadUnlock(p->Lock);
	return count;
}

// 遍历（读操作）
void SafeList_Foreach(SafeList* p, List_EachProc proc, ptr arg) {
	xrtRWLockReadLock(p->Lock);
	xrtListWalk(p->List, proc, arg);
	xrtRWLockReadUnlock(p->Lock);
}


/* ==================================== 线程安全的 Dict ==================================== */

typedef struct {
	xdict Dict;
	xrwlock Lock;
} SafeDict;

// 创建
SafeDict* SafeDict_Create(uint32 iItemLength) {
	SafeDict* p = xrtMalloc(sizeof(SafeDict));
	if ( !p ) return NULL;
	
	p->Dict = xrtDictCreate(iItemLength, XRT_OBJMODE_LOCAL);
	p->Lock = xrtRWLockCreate();
	
	return p;
}

// 销毁
void SafeDict_Destroy(SafeDict* p) {
	if ( p ) {
		xrtRWLockWriteLock(p->Lock);
		xrtDictDestroy(p->Dict);
		xrtRWLockWriteUnlock(p->Lock);
		
		xrtRWLockDestroy(p->Lock);
		xrtFree(p);
	}
}

// 获取值（读操作）
xvalue SafeDict_GetValue(SafeDict* p, str key, uint32 kl) {
	xrtRWLockReadLock(p->Lock);
	xvalue val = xvoTableGetValue(p->Dict, key, kl);
	xrtRWLockReadUnlock(p->Lock);
	return val;
}

// 设置值（写操作）
void SafeDict_SetValue(SafeDict* p, str key, uint32 kl, xvalue val, bool bColloc) {
	xrtRWLockWriteLock(p->Lock);
	xvoTableSetValue(p->Dict, key, kl, val, bColloc);
	xrtRWLockWriteUnlock(p->Lock);
}

// 获取元素数量（读操作）
uint32 SafeDict_GetCount(SafeDict* p) {
	xrtRWLockReadLock(p->Lock);
	uint32 count = xvoTableItemCount(p->Dict);
	xrtRWLockReadUnlock(p->Lock);
	return count;
}


/* ==================================== 线程安全的 Coll ==================================== */

typedef struct {
	xvalue Coll;
	xrwlock Lock;
} SafeColl;

// 创建
SafeColl* SafeColl_Create() {
	SafeColl* p = xrtMalloc(sizeof(SafeColl));
	if ( !p ) return NULL;
	
	p->Coll = xvoCreateColl();
	p->Lock = xrtRWLockCreate();
	
	return p;
}

// 销毁
void SafeColl_Destroy(SafeColl* p) {
	if ( p ) {
		xrtRWLockWriteLock(p->Lock);
		xvoUnref(p->Coll);
		xrtRWLockWriteUnlock(p->Lock);
		
		xrtRWLockDestroy(p->Lock);
		xrtFree(p);
	}
}

// 检查是否存在（读操作）
bool SafeColl_Exists(SafeColl* p, xvalue val) {
	xrtRWLockReadLock(p->Lock);
	bool exists = xvoCollExists(p->Coll, val);
	xrtRWLockReadUnlock(p->Lock);
	return exists;
}

// 添加值（写操作）
bool SafeColl_SetValue(SafeColl* p, xvalue val, bool bColloc) {
	xrtRWLockWriteLock(p->Lock);
	bool result = xvoCollSetValue(p->Coll, val, bColloc);
	xrtRWLockWriteUnlock(p->Lock);
	return result;
}

// 移除值（写操作）
bool SafeColl_Remove(SafeColl* p, xvalue val) {
	xrtRWLockWriteLock(p->Lock);
	bool result = xvoCollRemove(p->Coll, val);
	xrtRWLockWriteUnlock(p->Lock);
	return result;
}

// 获取元素数量（读操作）
uint32 SafeColl_GetCount(SafeColl* p) {
	xrtRWLockReadLock(p->Lock);
	uint32 count = xvoCollItemCount(p->Coll);
	xrtRWLockReadUnlock(p->Lock);
	return count;
}


/* ==================================== 使用示例 ==================================== */

void Example_Usage()
{
	// ========== 线程安全的 List ==========
	SafeList* list = SafeList_Create(sizeof(int));
	
	// 多个线程可以同时读取
	xvalue val1 = SafeList_GetValue(list, 0);
	xvalue val2 = SafeList_GetValue(list, 1);
	
	// 写操作会独占锁
	SafeList_SetValue(list, 0, xvoCreateInt(100), FALSE);
	
	// 遍历操作
	uint32 count = SafeList_GetCount(list);
	
	SafeList_Destroy(list);
	
	
	
	// ========== 线程安全的 Dict ==========
	SafeDict* dict = SafeDict_Create(sizeof(int));
	
	// 读取
	xvalue value = SafeDict_GetValue(dict, "key", 3);
	
	// 写入
	SafeDict_SetValue(dict, "key", 3, xvoCreateInt(123), FALSE);
	
	// 获取大小
	count = SafeDict_GetCount(dict);
	
	SafeDict_Destroy(dict);
	
	
	
	// ========== 线程安全的 Coll ==========
	SafeColl* coll = SafeColl_Create();
	
	// 检查是否存在
	bool exists = SafeColl_Exists(coll, xvoCreateInt(10));
	
	// 添加
	SafeColl_SetValue(coll, xvoCreateInt(20), FALSE);
	
	// 移除
	SafeColl_Remove(coll, xvoCreateInt(10));
	
	// 获取数量
	count = SafeColl_GetCount(coll);
	
	SafeColl_Destroy(coll);
}
