#include "../internal/xrt_memory.h"
#include "../internal/xrt_temp.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <pthread.h>
#endif



#if defined(XRT_FEATURE_TEMP_MEMORY)

#define XRT_TEMP_FLAG_INITIALIZED 0x0001u



/* 块头与用户区分配在同一段原始内存中。 */
struct xtempblock {
	xtempblock* Next;
	size_t Capacity;
	size_t Used;
};



/* 每个原生线程拥有默认 arena，并可临时绑定协程 arena。 */
typedef struct xrt_temp_thread_state {
	xtemparena ThreadArena;
	xtemparena* BoundArena;
} xrt_temp_thread_state;



/* 将大小向全局内存对齐单位上取整。 */
static bool __xrtTempAlign(size_t iSize, size_t* pAligned)
{
	if ( iSize == 0 ) {
		iSize = 1;
	}
	if ( iSize > (SIZE_MAX - (XRT_HEAP_ALIGNMENT - 1)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pAligned = (iSize + (XRT_HEAP_ALIGNMENT - 1)) & ~((size_t)XRT_HEAP_ALIGNMENT - 1);
	return true;
}



/* 返回对齐后的临时块头大小。 */
static size_t __xrtTempBlockHeaderSize(void)
{
	return (sizeof(xtempblock) + (XRT_HEAP_ALIGNMENT - 1)) & ~((size_t)XRT_HEAP_ALIGNMENT - 1);
}



/* 返回临时块的用户区起点。 */
static ptr __xrtTempBlockData(xtempblock* pBlock)
{
	return (unsigned char*)pBlock + __xrtTempBlockHeaderSize();
}



/* 通过底层分配器申请一个临时块。 */
static xtempblock* __xrtTempBlockAlloc(size_t iCapacity)
{
	size_t iHeaderSize = __xrtTempBlockHeaderSize();
	xtempblock* pBlock;

	if ( iCapacity > (SIZE_MAX - iHeaderSize) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pBlock = (xtempblock*)__xrtBackingAlloc(iHeaderSize + iCapacity);
	if ( pBlock == NULL ) {
		__xrtErrorSetOutOfMemory();
		return NULL;
	}
	memset(pBlock, 0, iHeaderSize);
	pBlock->Capacity = iCapacity;
	return pBlock;
}



/* 释放一条临时块链。 */
static void __xrtTempBlockFreeChain(xtempblock* pBlock)
{
	while ( pBlock != NULL ) {
		xtempblock* pNext = pBlock->Next;

		__xrtBackingFree(pBlock);
		pBlock = pNext;
	}
}



/* 安全擦除一条块链的完整用户区，包括普通重置后仍保留的旧内容。 */
static void __xrtTempBlockSecureChain(xtempblock* pBlock)
{
	while ( pBlock != NULL ) {
		xrtSecureZero(__xrtTempBlockData(pBlock), pBlock->Capacity);
		pBlock = pBlock->Next;
	}
}



/* 为零初始化 arena 填入默认配置。 */
static void __xrtTempSetDefaults(xtemparena* pArena)
{
	pArena->BlockSize = XRT_TEMP_BLOCK_SIZE_DEFAULT;
	pArena->SpillLimit = XRT_TEMP_SPILL_LIMIT_DEFAULT;
	pArena->RetainLimit = XRT_TEMP_RETAIN_LIMIT_DEFAULT;
	pArena->Flags = XRT_TEMP_FLAG_INITIALIZED;
}



/* 确保可惰性使用的 arena 已经初始化。 */
static bool __xrtTempEnsureArena(xtemparena* pArena)
{
	if ( pArena == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pArena->Flags & XRT_TEMP_FLAG_INITIALIZED) == 0 ) {
		memset(pArena, 0, sizeof(xtemparena));
		__xrtTempSetDefaults(pArena);
	}
	return true;
}



/* 追加常规块，不覆盖已经存在的复用链。 */
static bool __xrtTempAppendBlock(xtemparena* pArena, size_t iNeed)
{
	size_t iCapacity = pArena->BlockSize > iNeed ? pArena->BlockSize : iNeed;
	xtempblock* pBlock;

	if ( iCapacity > (SIZE_MAX - pArena->RetainedBytes) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBlock = __xrtTempBlockAlloc(iCapacity);
	if ( pBlock == NULL ) {
		return false;
	}
	if ( pArena->Tail != NULL ) {
		pArena->Tail->Next = pBlock;
	} else {
		pArena->Blocks = pBlock;
	}
	pArena->Tail = pBlock;
	pArena->Current = pBlock;
	pArena->RetainedBytes += iCapacity;
	return true;
}



/* 找到可复用常规块，没有时追加新块。 */
static bool __xrtTempEnsureCurrent(xtemparena* pArena, size_t iNeed)
{
	xtempblock* pBlock = pArena->Current != NULL ? pArena->Current : pArena->Blocks;

	while ( pBlock != NULL ) {
		if ( iNeed <= (pBlock->Capacity - pBlock->Used) ) {
			pArena->Current = pBlock;
			return true;
		}
		pBlock = pBlock->Next;
	}
	return __xrtTempAppendBlock(pArena, iNeed);
}



/* 在 arena 空闲时释放超过保留上限的常规块。 */
static void __xrtTempTrimIdle(xtemparena* pArena, size_t iRetainBytes)
{
	xtempblock* pBlock = pArena->Blocks;
	xtempblock* pPrevious = NULL;
	size_t iKept = 0;

	while ( pBlock != NULL ) {
		if ( (iKept + pBlock->Capacity) > iRetainBytes ) {
			if ( pPrevious != NULL ) {
				pPrevious->Next = NULL;
			} else {
				pArena->Blocks = NULL;
			}
			__xrtTempBlockFreeChain(pBlock);
			break;
		}
		iKept += pBlock->Capacity;
		pPrevious = pBlock;
		pBlock = pBlock->Next;
	}
	pArena->Tail = pPrevious;
	pArena->Current = pArena->Blocks;
	pArena->RetainedBytes = iKept;
}



/* 使用默认或指定配置初始化一个空 arena。 */
XRT_API bool xrtTempInit(xtemparena* pArena, const xtempconfig* pConfig)
{
	if ( pArena == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pArena, 0, sizeof(xtemparena));
	__xrtTempSetDefaults(pArena);
	if ( pConfig != NULL ) {
		if ( (pConfig->BlockSize == 0) || (pConfig->SpillLimit == 0) ) {
			memset(pArena, 0, sizeof(xtemparena));
			__xrtErrorSetInvalidArgument();
			return false;
		}
		pArena->BlockSize = pConfig->BlockSize;
		pArena->SpillLimit = pConfig->SpillLimit;
		pArena->RetainLimit = pConfig->RetainLimit;
	}
	return true;
}



/* 释放 arena 的全部内存并回到零状态。 */
XRT_API void xrtTempUnit(xtemparena* pArena)
{
	if ( pArena == NULL ) {
		return;
	}
	if ( ((pArena->Flags & XRT_TEMP_FLAG_INITIALIZED) != 0) && (pArena->CurrentBytes != 0) ) {
		__xrtMemDebugTempRelease(pArena->CurrentBytes, XMEMDEBUG_TEMP_RESET, __FILE__, (uint32)__LINE__);
	}
	__xrtTempBlockFreeChain(pArena->Blocks);
	__xrtTempBlockFreeChain(pArena->Spill);
	memset(pArena, 0, sizeof(xtemparena));
}



/* 从显式 arena 分配临时内存。 */
XRT_API ptr xrtTempAlloc(xtemparena* pArena, size_t iSize)
{
	size_t iNeed;
	xtempblock* pBlock;
	ptr pMemory;

	if ( !__xrtTempEnsureArena(pArena) || !__xrtTempAlign(iSize, &iNeed) ) {
		return NULL;
	}
	if ( iNeed > (SIZE_MAX - pArena->CurrentBytes) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( iNeed > pArena->SpillLimit ) {
		pBlock = __xrtTempBlockAlloc(iNeed);
		if ( pBlock == NULL ) {
			return NULL;
		}
		pBlock->Used = iNeed;
		pBlock->Next = pArena->Spill;
		pArena->Spill = pBlock;
		pMemory = __xrtTempBlockData(pBlock);
	} else {
		if ( !__xrtTempEnsureCurrent(pArena, iNeed) ) {
			return NULL;
		}
		pBlock = pArena->Current;
		pMemory = (unsigned char*)__xrtTempBlockData(pBlock) + pBlock->Used;
		pBlock->Used += iNeed;
	}
	pArena->CurrentBytes += iNeed;
	if ( pArena->CurrentBytes > pArena->PeakBytes ) {
		pArena->PeakBytes = pArena->CurrentBytes;
	}
	__xrtMemStatsTemp(iSize);
	__xrtMemDebugTempAlloc(iNeed, __FILE__, (uint32)__LINE__);
	return pMemory;
}



/* 把一段二进制数据复制到指定 arena。 */
XRT_API ptr xrtTempDup(xtemparena* pArena, const void* pData, size_t iSize)
{
	ptr pCopy;

	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pCopy = xrtTempAlloc(pArena, iSize);
	if ( (pCopy != NULL) && (iSize != 0) ) {
		memcpy(pCopy, pData, iSize);
	}
	return pCopy;
}



/* 把字符串视图复制为零结尾临时字符串。 */
XRT_API str xrtTempStr(xtemparena* pArena, xstrview Text)
{
	str sCopy;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sCopy = (str)xrtTempAlloc(pArena, Text.Size + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sCopy, Text.Data, Text.Size);
	}
	sCopy[Text.Size] = 0;
	return sCopy;
}



/* 重置全部临时分配并执行有界保留。 */
XRT_API bool xrtTempReset(xtemparena* pArena)
{
	if ( !__xrtTempEnsureArena(pArena) ) {
		return false;
	}
	if ( pArena->ScopeDepth != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pArena->CurrentBytes != 0 ) {
		__xrtMemDebugTempRelease(pArena->CurrentBytes, XMEMDEBUG_TEMP_RESET, __FILE__, (uint32)__LINE__);
	}
	for ( xtempblock* pBlock = pArena->Blocks; pBlock != NULL; pBlock = pBlock->Next ) {
		pBlock->Used = 0;
	}
	__xrtTempBlockFreeChain(pArena->Spill);
	pArena->Spill = NULL;
	pArena->Current = pArena->Blocks;
	pArena->CurrentBytes = 0;
	pArena->ResetCount++;
	__xrtTempTrimIdle(pArena, pArena->RetainLimit);
	return true;
}



/* 安全擦除全部常规块和 spill 块，再复用普通重置的统计与保留逻辑。 */
XRT_API bool xrtTempSecureReset(xtemparena* pArena)
{
	if ( !__xrtTempEnsureArena(pArena) ) {
		return false;
	}
	if ( pArena->ScopeDepth != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtTempBlockSecureChain(pArena->Blocks);
	__xrtTempBlockSecureChain(pArena->Spill);
	return xrtTempReset(pArena);
}



/* 安全擦除全部块容量后释放 arena。 */
XRT_API void xrtTempSecureUnit(xtemparena* pArena)
{
	if ( pArena == NULL ) {
		return;
	}
	if ( (pArena->Flags & XRT_TEMP_FLAG_INITIALIZED) != 0 ) {
		__xrtTempBlockSecureChain(pArena->Blocks);
		__xrtTempBlockSecureChain(pArena->Spill);
	}
	xrtTempUnit(pArena);
}



/* 在 arena 空闲时调整实际保留量。 */
XRT_API bool xrtTempTrim(xtemparena* pArena, size_t iRetainBytes)
{
	if ( !__xrtTempEnsureArena(pArena) ) {
		return false;
	}
	if ( (pArena->ScopeDepth != 0) || (pArena->CurrentBytes != 0) || (pArena->Spill != NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtTempTrimIdle(pArena, iRetainBytes);
	return true;
}



/* 建立严格后进先出的临时作用域。 */
XRT_API xtempmark xrtTempBegin(xtemparena* pArena)
{
	xtempmark tMark;

	memset(&tMark, 0, sizeof(tMark));
	if ( !__xrtTempEnsureArena(pArena) ) {
		return tMark;
	}
	if ( (pArena->ScopeDepth == UINT32_MAX) || (pArena->ScopeSerial == UINT64_MAX) ) {
		__xrtErrorSetSizeOverflow();
		return tMark;
	}
	tMark.Arena = pArena;
	tMark.Current = pArena->Current;
	tMark.Spill = pArena->Spill;
	tMark.Used = pArena->Current != NULL ? pArena->Current->Used : 0;
	tMark.CurrentBytes = pArena->CurrentBytes;
	tMark.ParentId = pArena->ActiveScopeId;
	tMark.Id = ++pArena->ScopeSerial;
	tMark.Depth = ++pArena->ScopeDepth;
	tMark.Active = true;
	pArena->ActiveScopeId = tMark.Id;
	return tMark;
}



/* 回退一个作用域内的常规块游标和 spill 链。 */
XRT_API bool xrtTempEnd(xtempmark* pMark)
{
	xtemparena* pArena;
	xtempblock* pBlock;

	if ( pMark == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !pMark->Active ) {
		return true;
	}
	pArena = pMark->Arena;
	if ( (pArena == NULL) ||
		 (pArena->ScopeDepth != pMark->Depth) ||
		 (pArena->ActiveScopeId != pMark->Id) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	while ( pArena->Spill != pMark->Spill ) {
		xtempblock* pRelease = pArena->Spill;

		if ( pRelease == NULL ) {
			__xrtErrorSetInvalidState();
			return false;
		}
		pArena->Spill = pRelease->Next;
		__xrtBackingFree(pRelease);
	}
	if ( pMark->Current != NULL ) {
		pMark->Current->Used = pMark->Used;
		pBlock = pMark->Current->Next;
	} else {
		pBlock = pArena->Blocks;
	}
	while ( pBlock != NULL ) {
		pBlock->Used = 0;
		pBlock = pBlock->Next;
	}
	if ( pArena->CurrentBytes > pMark->CurrentBytes ) {
		__xrtMemDebugTempRelease(pArena->CurrentBytes - pMark->CurrentBytes,
			XMEMDEBUG_TEMP_REWIND, __FILE__, (uint32)__LINE__);
	}
	pArena->Current = pMark->Current != NULL ? pMark->Current : pArena->Blocks;
	pArena->CurrentBytes = pMark->CurrentBytes;
	pArena->ScopeDepth--;
	pArena->ActiveScopeId = pMark->ParentId;
	pMark->Active = false;
	return true;
}



/* 结束作用域并把二进制数据提升到父作用域。 */
XRT_API ptr xrtTempEndDup(xtempmark* pMark, const void* pData, size_t iSize)
{
	xtemparena* pArena;
	ptr pOwned;
	ptr pResult;

	if ( (pMark == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !pMark->Active || (pMark->Arena == NULL) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pArena = pMark->Arena;
	pOwned = xrtMemDup(pData, iSize);
	if ( pOwned == NULL ) {
		return NULL;
	}
	if ( !xrtTempEnd(pMark) ) {
		xrtFree(pOwned);
		return NULL;
	}
	pResult = xrtTempDup(pArena, pOwned, iSize);
	xrtFree(pOwned);
	return pResult;
}



/* 结束作用域并把字符串提升到父作用域。 */
XRT_API str xrtTempEndStr(xtempmark* pMark, xstrview Text)
{
	xtemparena* pArena;
	str sOwned;
	str sResult;

	if ( (pMark == NULL) || ((Text.Data == NULL) && (Text.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !pMark->Active || (pMark->Arena == NULL) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pArena = pMark->Arena;
	sOwned = (str)xrtMalloc(Text.Size + 1u);
	if ( sOwned == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sOwned, Text.Data, Text.Size);
	}
	sOwned[Text.Size] = 0;
	if ( !xrtTempEnd(pMark) ) {
		xrtFree(sOwned);
		return NULL;
	}
	sResult = xrtTempStr(pArena, (xstrview){ sOwned, Text.Size });
	xrtFree(sOwned);
	return sResult;
}



/* 获取 arena 当前的块数量和用量。 */
XRT_API void xrtTempGet(const xtemparena* pArena, xtempinfo* pInfo)
{
	const xtempblock* pBlock;

	if ( (pArena == NULL) || (pInfo == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pInfo, 0, sizeof(xtempinfo));
	for ( pBlock = pArena->Blocks; pBlock != NULL; pBlock = pBlock->Next ) {
		pInfo->BlockCount++;
	}
	for ( pBlock = pArena->Spill; pBlock != NULL; pBlock = pBlock->Next ) {
		pInfo->SpillCount++;
	}
	pInfo->RetainedBytes = pArena->RetainedBytes;
	pInfo->CurrentBytes = pArena->CurrentBytes;
	pInfo->PeakBytes = pArena->PeakBytes;
	pInfo->ResetCount = pArena->ResetCount;
	pInfo->ScopeDepth = pArena->ScopeDepth;
}



#if defined(_WIN32) || defined(_WIN64)

static DWORD __xrtTempTlsIndex = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtTempTlsState;



/* 在线程退出时释放默认 arena 和 TLS 状态。 */
static VOID CALLBACK __xrtTempTlsDestroy(PVOID pValue)
{
	xrt_temp_thread_state* pState = (xrt_temp_thread_state*)pValue;

	if ( pState != NULL ) {
		xrtTempUnit(&pState->ThreadArena);
		__xrtBackingFree(pState);
	}
}



/* 初始化 Windows FLS 槽。 */
static bool __xrtTempTlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtTempTlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtTempTlsIndex = FlsAlloc(__xrtTempTlsDestroy);
		InterlockedExchange(&__xrtTempTlsState,
			__xrtTempTlsIndex != FLS_OUT_OF_INDEXES ? 2 : 3);
		return __xrtTempTlsIndex != FLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtTempTlsState, 0, 0)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 读取当前 Windows 线程状态。 */
static xrt_temp_thread_state* __xrtTempTlsGet(void)
{
	return __xrtTempTlsEnsure() ? (xrt_temp_thread_state*)FlsGetValue(__xrtTempTlsIndex) : NULL;
}



/* 写入当前 Windows 线程状态。 */
static bool __xrtTempTlsSet(xrt_temp_thread_state* pState)
{
	return __xrtTempTlsEnsure() && (FlsSetValue(__xrtTempTlsIndex, pState) != 0);
}

#else

static pthread_key_t __xrtTempTlsKey;
static pthread_once_t __xrtTempTlsOnce = PTHREAD_ONCE_INIT;
static bool __xrtTempTlsReady;



/* 在线程退出时释放默认 arena 和 TLS 状态。 */
static void __xrtTempTlsDestroy(void* pValue)
{
	xrt_temp_thread_state* pState = (xrt_temp_thread_state*)pValue;

	if ( pState != NULL ) {
		xrtTempUnit(&pState->ThreadArena);
		__xrtBackingFree(pState);
	}
}



/* 初始化 POSIX TLS key。 */
static void __xrtTempTlsInit(void)
{
	__xrtTempTlsReady = pthread_key_create(&__xrtTempTlsKey, __xrtTempTlsDestroy) == 0;
}



/* 确保 POSIX TLS key 已经初始化。 */
static bool __xrtTempTlsEnsure(void)
{
	(void)pthread_once(&__xrtTempTlsOnce, __xrtTempTlsInit);
	return __xrtTempTlsReady;
}



/* 读取当前 POSIX 线程状态。 */
static xrt_temp_thread_state* __xrtTempTlsGet(void)
{
	return __xrtTempTlsEnsure() ? (xrt_temp_thread_state*)pthread_getspecific(__xrtTempTlsKey) : NULL;
}



/* 写入当前 POSIX 线程状态。 */
static bool __xrtTempTlsSet(xrt_temp_thread_state* pState)
{
	return __xrtTempTlsEnsure() && (pthread_setspecific(__xrtTempTlsKey, pState) == 0);
}

#endif



/* 获取或创建当前原生线程状态。 */
static xrt_temp_thread_state* __xrtTempThreadState(bool bCreate)
{
	xrt_temp_thread_state* pState = __xrtTempTlsGet();

	if ( (pState == NULL) && bCreate ) {
		pState = (xrt_temp_thread_state*)__xrtBackingAlloc(sizeof(xrt_temp_thread_state));
		if ( pState == NULL ) {
			__xrtErrorSetOutOfMemory();
			return NULL;
		}
		memset(pState, 0, sizeof(xrt_temp_thread_state));
		if ( !__xrtTempTlsSet(pState) ) {
			__xrtBackingFree(pState);
			__xrtErrorSetInvalidState();
			return NULL;
		}
	}
	return pState;
}



/* 返回当前线程或绑定执行上下文的 arena。 */
XRT_API xtemparena* xrtTempCurrent(void)
{
	xrt_temp_thread_state* pState = __xrtTempThreadState(true);
	xtemparena* pArena;

	if ( pState == NULL ) {
		return NULL;
	}
	pArena = pState->BoundArena != NULL ? pState->BoundArena : &pState->ThreadArena;
	return __xrtTempEnsureArena(pArena) ? pArena : NULL;
}



/* 从当前执行上下文分配临时内存。 */
XRT_API ptr xrtTemp(size_t iSize)
{
	xtemparena* pArena = xrtTempCurrent();

	return pArena != NULL ? xrtTempAlloc(pArena, iSize) : NULL;
}



/* 重置当前执行上下文的 arena。 */
XRT_API bool xrtTempClear(void)
{
	xtemparena* pArena = xrtTempCurrent();

	return pArena != NULL && xrtTempReset(pArena);
}



/* 切换协程或任务绑定的 arena，并返回先前显式绑定。 */
xtemparena* __xrtTempContextSwap(xtemparena* pArena)
{
	xrt_temp_thread_state* pState = __xrtTempThreadState(true);
	xtemparena* pPrevious;

	if ( pState == NULL ) {
		return NULL;
	}
	pPrevious = pState->BoundArena;
	pState->BoundArena = pArena;
	return pPrevious;
}

#endif
