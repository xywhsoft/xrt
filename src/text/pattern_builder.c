#include "../internal/xrt_pattern.h"



#if defined(XRT_FEATURE_PATTERN)

/* 将槽下标与非零代际编码为公开稳定 ID。 */
static xpatternid __xrtPatternBuilderId(size_t iIndex, uint32 iGeneration)
{
	return
		(((uint64)iGeneration) << 32u) |
		((uint64)((uint32)iIndex + 1u));
}



/* 解析 ID 并验证其仍指向活动槽。 */
static __xrt_pattern_builder_slot* __xrtPatternBuilderSlot(
	xpatternbuilder* pBuilder,
	xpatternid Id,
	size_t* pIndex
)
{
	uint32 iEncoded = (uint32)(Id & UINT64_C(0xffffffff));
	uint32 iGeneration = (uint32)(Id >> 32u);
	size_t iIndex;
	__xrt_pattern_builder_slot* pSlot;

	if ( (iEncoded == 0) || (iGeneration == 0) ) {
		return NULL;
	}
	iIndex = (size_t)(iEncoded - 1u);
	if ( iIndex >= pBuilder->SlotCount ) {
		return NULL;
	}
	pSlot = &pBuilder->Slots[iIndex];
	if ( (pSlot->Source == NULL) || (pSlot->Generation != iGeneration) ) {
		return NULL;
	}
	if ( pIndex != NULL ) {
		*pIndex = iIndex;
	}
	return pSlot;
}



/* 在修改前保留版本单调性，避免溢出后 Dirty 状态产生歧义。 */
static bool __xrtPatternBuilderCanModify(xpatternbuilder* pBuilder)
{
	if ( pBuilder->Version == UINT64_MAX ) {
		__xrtPatternSetInvalidState();
		return false;
	}
	return true;
}



/* 扩展槽数组，新增槽从代际 1 开始且尚未进入空闲链。 */
static bool __xrtPatternBuilderReserveValid(
	xpatternbuilder* pBuilder,
	size_t iCapacity
)
{
	__xrt_pattern_builder_slot* pSlots;
	size_t iNewCapacity;
	size_t iOldCapacity;

	if ( iCapacity <= pBuilder->SlotCapacity ) {
		return true;
	}
	if ( iCapacity > pBuilder->Options.MaxPatterns ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			"builder_reserve",
			"builder capacity exceeds its pattern limit",
			false,
			0,
			false,
			0
		);
		return false;
	}
	iNewCapacity = pBuilder->SlotCapacity != 0 ? pBuilder->SlotCapacity :
		(pBuilder->Options.MaxPatterns < 8u ?
		 pBuilder->Options.MaxPatterns : 8u);
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > (pBuilder->Options.MaxPatterns / 2u) ) {
			iNewCapacity = pBuilder->Options.MaxPatterns;
			break;
		}
		iNewCapacity *= 2u;
	}
	if ( iNewCapacity > (SIZE_MAX / sizeof(*pSlots)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	iOldCapacity = pBuilder->SlotCapacity;
	pSlots = (__xrt_pattern_builder_slot*)xrtRealloc(
		pBuilder->Slots,
		iNewCapacity * sizeof(*pSlots)
	);
	if ( pSlots == NULL ) {
		return false;
	}
	memset(
		pSlots + iOldCapacity,
		0,
		(iNewCapacity - iOldCapacity) * sizeof(*pSlots)
	);
	for ( size_t i = iOldCapacity; i < iNewCapacity; i++ ) {
		pSlots[i].Generation = 1u;
		pSlots[i].NextFree = __XRT_PATTERN_SLOT_NONE;
	}
	pBuilder->Slots = pSlots;
	pBuilder->SlotCapacity = iNewCapacity;
	return true;
}



/* 取一个已预留槽；本函数不分配，因此可用于批量事务提交。 */
static size_t __xrtPatternBuilderTakeSlot(xpatternbuilder* pBuilder)
{
	size_t iIndex;
	__xrt_pattern_builder_slot* pSlot;

	if ( pBuilder->FreeSlot != __XRT_PATTERN_SLOT_NONE ) {
		iIndex = pBuilder->FreeSlot;
		pSlot = &pBuilder->Slots[iIndex];
		pBuilder->FreeSlot = pSlot->NextFree;
		pSlot->NextFree = __XRT_PATTERN_SLOT_NONE;
		return iIndex;
	}
	iIndex = pBuilder->SlotCount++;
	return iIndex;
}



/* 创建自包含 Builder。 */
XRT_API xpatternbuilder* xrtPatternBuilderCreateConfig(
	const xpatternconfig* pConfig
)
{
	__xrt_pattern_options Options;
	xpatternbuilder* pBuilder;

	if ( !__xrtPatternOptionsInit(pConfig, &Options, "builder_create") ) {
		return NULL;
	}
	pBuilder = (xpatternbuilder*)xrtCalloc(1u, sizeof(*pBuilder));
	if ( pBuilder == NULL ) {
		return NULL;
	}
	pBuilder->Options = Options;
	pBuilder->FreeSlot = __XRT_PATTERN_SLOT_NONE;
	pBuilder->NextOrder = 1u;
	pBuilder->Version = 1u;
	return pBuilder;
}



XRT_API xpatternbuilder* xrtPatternBuilderCreate(void)
{
	xpatternconfig Config;

	xrtPatternConfigInit(&Config);
	return xrtPatternBuilderCreateConfig(&Config);
}



XRT_API void xrtPatternBuilderFree(xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		return;
	}
	for ( size_t i = 0; i < pBuilder->SlotCount; i++ ) {
		__xrtPatternSourceFree(pBuilder->Slots[i].Source);
	}
	xrtPatternRelease(pBuilder->Cached);
	xrtFree(pBuilder->Slots);
	xrtFree(pBuilder);
}



XRT_API void xrtPatternBuilderClear(xpatternbuilder* pBuilder)
{
	uint32 iFree = __XRT_PATTERN_SLOT_NONE;

	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return;
	}
	if ( pBuilder->Count == 0 ) {
		return;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return;
	}
	for ( size_t i = pBuilder->SlotCount; i != 0; i-- ) {
		__xrt_pattern_builder_slot* pSlot = &pBuilder->Slots[i - 1u];

		__xrtPatternSourceFree(pSlot->Source);
		pSlot->Source = NULL;
		pSlot->Generation++;
		if ( pSlot->Generation == 0 ) {
			pSlot->Generation = 1u;
		}
		pSlot->NextFree = iFree;
		iFree = (uint32)(i - 1u);
	}
	pBuilder->FreeSlot = iFree;
	pBuilder->Count = 0;
	pBuilder->Version++;
}



XRT_API bool xrtPatternBuilderReserve(
	xpatternbuilder* pBuilder,
	size_t iCapacity
)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	return __xrtPatternBuilderReserveValid(pBuilder, iCapacity);
}



XRT_API size_t xrtPatternBuilderCount(const xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pBuilder->Count;
}



XRT_API uint64 xrtPatternBuilderVersion(const xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pBuilder->Version;
}



XRT_API bool xrtPatternBuilderDirty(const xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	return (pBuilder->Cached == NULL) ||
		(pBuilder->CompiledVersion != pBuilder->Version);
}



XRT_API xpatternid xrtPatternBuilderAdd(
	xpatternbuilder* pBuilder,
	const xpatternspec* pSpec
)
{
	__xrt_pattern_source* pSource;
	__xrt_pattern_builder_slot* pSlot;
	xpatternid Id;
	size_t iIndex;

	if ( (pBuilder == NULL) || (pSpec == NULL) ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ID_INVALID;
	}
	if ( (pBuilder->Count >= pBuilder->Options.MaxPatterns) ||
		 (pBuilder->NextOrder == UINT64_MAX) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			"builder_add",
			"builder cannot accept another pattern",
			false,
			0,
			false,
			0
		);
		return XPATTERN_ID_INVALID;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return XPATTERN_ID_INVALID;
	}
	pSource = __xrtPatternSourceCreate(
		pSpec,
		&pBuilder->Options,
		"builder_add",
		XPATTERN_ID_INVALID,
		pBuilder->NextOrder,
		false,
		0
	);
	if ( pSource == NULL ) {
		return XPATTERN_ID_INVALID;
	}
	if ( !__xrtPatternBuilderReserveValid(pBuilder, pBuilder->Count + 1u) ) {
		__xrtPatternSourceFree(pSource);
		return XPATTERN_ID_INVALID;
	}
	iIndex = __xrtPatternBuilderTakeSlot(pBuilder);
	pSlot = &pBuilder->Slots[iIndex];
	Id = __xrtPatternBuilderId(iIndex, pSlot->Generation);
	pSource->Id = Id;
	pSlot->Source = pSource;
	pBuilder->Count++;
	pBuilder->NextOrder++;
	pBuilder->Version++;
	return Id;
}



XRT_API bool xrtPatternBuilderAddMany(
	xpatternbuilder* pBuilder,
	const xpatternspec* arrSpec,
	size_t iCount,
	xpatternid* arrId
)
{
	__xrt_pattern_source** arrSource = NULL;
	size_t iParsed = 0;

	if ( (pBuilder == NULL) || ((arrSpec == NULL) && (iCount != 0)) ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if ( (iCount > (pBuilder->Options.MaxPatterns - pBuilder->Count)) ||
		 (iCount > (UINT64_MAX - pBuilder->NextOrder)) ) {
		__xrtPatternError(
			XERR_RANGE,
			XPATTERN_ERROR_LIMIT,
			"builder_add_many",
			"builder batch exceeds its pattern limit",
			false,
			0,
			false,
			0
		);
		return false;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return false;
	}
	if ( iCount > (SIZE_MAX / sizeof(*arrSource)) ) {
		__xrtPatternSetSizeOverflow();
		return false;
	}
	arrSource = (__xrt_pattern_source**)xrtCalloc(iCount, sizeof(*arrSource));
	if ( arrSource == NULL ) {
		return false;
	}
	for ( ; iParsed < iCount; iParsed++ ) {
		arrSource[iParsed] = __xrtPatternSourceCreate(
			&arrSpec[iParsed],
			&pBuilder->Options,
			"builder_add_many",
			XPATTERN_ID_INVALID,
			pBuilder->NextOrder + iParsed,
			true,
			iParsed
		);
		if ( arrSource[iParsed] == NULL ) {
			break;
		}
	}
	if ( (iParsed != iCount) ||
		 !__xrtPatternBuilderReserveValid(pBuilder, pBuilder->Count + iCount) ) {
		for ( size_t i = 0; i < iCount; i++ ) {
			__xrtPatternSourceFree(arrSource[i]);
		}
		xrtFree(arrSource);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iIndex = __xrtPatternBuilderTakeSlot(pBuilder);
		__xrt_pattern_builder_slot* pSlot = &pBuilder->Slots[iIndex];
		xpatternid Id = __xrtPatternBuilderId(iIndex, pSlot->Generation);

		arrSource[i]->Id = Id;
		pSlot->Source = arrSource[i];
		if ( arrId != NULL ) {
			arrId[i] = Id;
		}
	}
	pBuilder->Count += iCount;
	pBuilder->NextOrder += iCount;
	pBuilder->Version++;
	xrtFree(arrSource);
	return true;
}



XRT_API bool xrtPatternBuilderSet(
	xpatternbuilder* pBuilder,
	xpatternid Id,
	const xpatternspec* pSpec
)
{
	__xrt_pattern_builder_slot* pSlot;
	__xrt_pattern_source* pSource;

	if ( (pBuilder == NULL) || (pSpec == NULL) ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	pSlot = __xrtPatternBuilderSlot(pBuilder, Id, NULL);
	if ( pSlot == NULL ) {
		return false;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return false;
	}
	pSource = __xrtPatternSourceCreate(
		pSpec,
		&pBuilder->Options,
		"builder_set",
		Id,
		pSlot->Source->Order,
		false,
		0
	);
	if ( pSource == NULL ) {
		return false;
	}
	__xrtPatternSourceFree(pSlot->Source);
	pSlot->Source = pSource;
	pBuilder->Version++;
	return true;
}



XRT_API bool xrtPatternBuilderRemove(
	xpatternbuilder* pBuilder,
	xpatternid Id
)
{
	__xrt_pattern_builder_slot* pSlot;
	size_t iIndex;

	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	pSlot = __xrtPatternBuilderSlot(pBuilder, Id, &iIndex);
	if ( pSlot == NULL ) {
		return false;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return false;
	}
	__xrtPatternSourceFree(pSlot->Source);
	pSlot->Source = NULL;
	pSlot->Generation++;
	if ( pSlot->Generation == 0 ) {
		pSlot->Generation = 1u;
	}
	pSlot->NextFree = pBuilder->FreeSlot;
	pBuilder->FreeSlot = (uint32)iIndex;
	pBuilder->Count--;
	pBuilder->Version++;
	return true;
}



/* qsort 比较器只用于恢复稳定注册顺序，不进入匹配热路径。 */
static int __xrtPatternBuilderSourceCompare(
	const void* pLeft,
	const void* pRight
)
{
	const __xrt_pattern_source* pA =
		*(const __xrt_pattern_source* const*)pLeft;
	const __xrt_pattern_source* pB =
		*(const __xrt_pattern_source* const*)pRight;

	if ( pA->Order < pB->Order ) {
		return -1;
	}
	if ( pA->Order > pB->Order ) {
		return 1;
	}
	return 0;
}



XRT_API xpattern* xrtPatternBuilderCompile(xpatternbuilder* pBuilder)
{
	__xrt_pattern_source** arrSource = NULL;
	xpattern* pPattern;
	size_t iWrite = 0;

	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	if ( (pBuilder->Cached != NULL) &&
		 (pBuilder->CompiledVersion == pBuilder->Version) ) {
		return xrtPatternRef(pBuilder->Cached);
	}
	if ( pBuilder->Count > (SIZE_MAX / sizeof(*arrSource)) ) {
		__xrtPatternSetSizeOverflow();
		return NULL;
	}
	if ( pBuilder->Count != 0 ) {
		arrSource = (__xrt_pattern_source**)xrtMalloc(
			pBuilder->Count * sizeof(*arrSource)
		);
		if ( arrSource == NULL ) {
			return NULL;
		}
	}
	for ( size_t i = 0; i < pBuilder->SlotCount; i++ ) {
		if ( pBuilder->Slots[i].Source != NULL ) {
			if ( (arrSource == NULL) || (iWrite >= pBuilder->Count) ) {
				xrtFree(arrSource);
				__xrtPatternSetInternal();
				return NULL;
			}
			arrSource[iWrite++] = pBuilder->Slots[i].Source;
		}
	}
	if ( iWrite != pBuilder->Count ) {
		xrtFree(arrSource);
		__xrtPatternSetInternal();
		return NULL;
	}
	if ( pBuilder->Count > 1u ) {
		qsort(
			arrSource,
			pBuilder->Count,
			sizeof(*arrSource),
			__xrtPatternBuilderSourceCompare
		);
	}
	pPattern = __xrtPatternCompileSources(
		arrSource,
		pBuilder->Count,
		&pBuilder->Options,
		"builder_compile"
	);
	xrtFree(arrSource);
	if ( pPattern == NULL ) {
		return NULL;
	}
	xrtPatternRelease(pBuilder->Cached);
	pBuilder->Cached = xrtPatternRef(pPattern);
	if ( pBuilder->Cached == NULL ) {
		xrtPatternRelease(pPattern);
		return NULL;
	}
	pBuilder->CompiledVersion = pBuilder->Version;
	return pPattern;
}

#endif
