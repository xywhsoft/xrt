#include "../internal/xrt_pattern.h"



#if defined(XRT_FEATURE_PATTERN)

static xpatternedit* __xrtPatternBuilderPrepareAdd(
	xpatternbuilder* pBuilder, const xpatternspec* arrSpec, size_t iCount, cstr sOperation
);

/* 将槽下标与非零代际编码为公开稳定 ID。 */
static xpatternid __xrtPatternBuilderId(size_t iIndex, uint32 iGeneration)
{
	return
		(((uint64)iGeneration) << 32u) |
		((uint64)((uint32)iIndex + 1u));
}



/* 解析 ID 并验证其仍指向活动槽。 */
static __xrt_pattern_builder_slot* __xrtPatternBuilderSlot(
	const xpatternbuilder* pBuilder,
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
	if ( (pBuilder->Pending != NULL) || (pBuilder->Version == UINT64_MAX) ) {
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
	if ( pBuilder->Pending != NULL ) {
		pBuilder->Pending->Builder = NULL;
		pBuilder->Pending = NULL;
	}
	for ( size_t i = 0; i < pBuilder->SlotCount; i++ ) {
		__xrtPatternSourceFree(pBuilder->Slots[i].Source);
	}
	xrtPatternRelease(pBuilder->Cached);
	xrtFree(pBuilder->Slots);
	xrtFree(pBuilder);
}



static bool __xrtPatternBuilderClearAllowed(xpatternbuilder* pBuilder)
{
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return false;
	}
	/* 代际不能回绕，清空必须在释放任何源之前完成全量预检。 */
	for ( size_t i = 0; i < pBuilder->SlotCount; i++ ) {
		if ( pBuilder->Slots[i].Generation == UINT32_MAX ) {
			__xrtPatternSetInvalidState();
			return false;
		}
	}
	return true;
}



static void __xrtPatternBuilderClearValid(xpatternbuilder* pBuilder)
{
	uint32 iFree = __XRT_PATTERN_SLOT_NONE;
	for ( size_t i = pBuilder->SlotCount; i != 0; i-- ) {
		__xrt_pattern_builder_slot* pSlot = &pBuilder->Slots[i - 1u];

		__xrtPatternSourceFree(pSlot->Source);
		pSlot->Source = NULL;
		pSlot->Generation++;
		pSlot->NextFree = iFree;
		iFree = (uint32)(i - 1u);
	}
	pBuilder->FreeSlot = iFree;
	pBuilder->Count = 0;
	pBuilder->Version++;
}



XRT_API void xrtPatternBuilderClear(xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return;
	}
	if ( pBuilder->Pending != NULL ) {
		__xrtPatternSetInvalidState();
		return;
	}
	if ( pBuilder->Count == 0 ) {
		return;
	}
	if ( __xrtPatternBuilderClearAllowed(pBuilder) ) {
		__xrtPatternBuilderClearValid(pBuilder);
	}
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
	if ( pBuilder->Pending != NULL ) {
		__xrtPatternSetInvalidState();
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
	xpatternedit* pEdit;
	bool bResult;
	if ( (pBuilder == NULL) || ((arrSpec == NULL) && (iCount != 0)) ) {
		__xrtPatternSetInvalidArgument(); return false;
	}
	if ( pBuilder->Pending != NULL ) { __xrtPatternSetInvalidState(); return false; }
	if ( iCount == 0 ) return true;
	pEdit = __xrtPatternBuilderPrepareAdd(pBuilder, arrSpec, iCount, "builder_add_many");
	if ( pEdit == NULL ) return false;
	bResult = xrtPatternEditCommit(pEdit);
	if ( bResult && (arrId != NULL) ) {
		for ( size_t i = 0; i < iCount; i++ ) arrId[i] = pEdit->Items[i].Id;
	}
	xrtPatternEditFree(pEdit);
	return bResult;
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
	if ( pBuilder->Pending != NULL ) {
		__xrtPatternSetInvalidState();
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



static void __xrtPatternBuilderRemoveValid(xpatternbuilder* pBuilder, size_t iIndex)
{
	__xrt_pattern_builder_slot* pSlot = &pBuilder->Slots[iIndex];
	__xrtPatternSourceFree(pSlot->Source);
	pSlot->Source = NULL;
	pSlot->Generation++;
	pSlot->NextFree = pBuilder->FreeSlot;
	pBuilder->FreeSlot = (uint32)iIndex;
	pBuilder->Count--;
	pBuilder->Version++;
}



XRT_API bool xrtPatternBuilderRemove(xpatternbuilder* pBuilder, xpatternid Id)
{
	__xrt_pattern_builder_slot* pSlot;
	size_t iIndex;

	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	if ( pBuilder->Pending != NULL ) {
		__xrtPatternSetInvalidState();
		return false;
	}
	pSlot = __xrtPatternBuilderSlot(pBuilder, Id, &iIndex);
	if ( pSlot == NULL ) {
		return false;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) {
		return false;
	}
	if ( pSlot->Generation == UINT32_MAX ) {
		__xrtPatternSetInvalidState();
		return false;
	}
	__xrtPatternBuilderRemoveValid(pBuilder, iIndex);
	return true;
}



XRT_API bool xrtPatternBuilderContains(const xpatternbuilder* pBuilder, xpatternid Id)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	return __xrtPatternBuilderSlot(pBuilder, Id, NULL) != NULL;
}



/* 只检查独占权；零项编辑不要求增加版本。 */
static bool __xrtPatternBuilderPrepareAllowed(xpatternbuilder* pBuilder)
{
	if ( pBuilder == NULL ) {
		__xrtPatternSetInvalidArgument();
		return false;
	}
	if ( pBuilder->Pending != NULL ) {
		__xrtPatternSetInvalidState();
		return false;
	}
	return true;
}



static xpatternedit* __xrtPatternEditCreate(__xrt_pattern_edit_kind Kind, size_t iCount)
{
	xpatternedit* pEdit;
	if ( iCount > ((SIZE_MAX - sizeof(*pEdit)) / sizeof(pEdit->Items[0])) ) {
		__xrtPatternSetSizeOverflow();
		return NULL;
	}
	pEdit = (xpatternedit*)xrtCalloc(1u, sizeof(*pEdit) + iCount * sizeof(pEdit->Items[0]));
	if ( pEdit != NULL ) {
		pEdit->Kind = Kind;
		pEdit->Count = iCount;
	}
	return pEdit;
}



XRT_API void xrtPatternEditFree(xpatternedit* pEdit)
{
	if ( pEdit == NULL ) {
		return;
	}
	if ( pEdit->Builder != NULL ) {
		pEdit->Builder->Pending = NULL;
		pEdit->Builder = NULL;
	}
	for ( size_t i = 0; i < pEdit->Count; i++ ) {
		__xrtPatternSourceFree(pEdit->Items[i].Source);
	}
	xrtFree(pEdit);
}



XRT_API bool xrtPatternEditReady(const xpatternedit* pEdit)
{
	return (pEdit != NULL) && (pEdit->Builder != NULL) &&
		(pEdit->Builder->Pending == pEdit);
}



XRT_API size_t xrtPatternEditCount(const xpatternedit* pEdit)
{
	if ( pEdit == NULL ) {
		__xrtPatternSetInvalidArgument();
		return 0;
	}
	return pEdit->Count;
}



XRT_API xpatternid xrtPatternEditId(const xpatternedit* pEdit, size_t iIndex)
{
	if ( pEdit == NULL ) {
		__xrtPatternSetInvalidArgument();
		return XPATTERN_ID_INVALID;
	}
	if ( iIndex >= pEdit->Count ) {
		__xrtPatternSetRange();
		return XPATTERN_ID_INVALID;
	}
	return pEdit->Items[iIndex].Id;
}



static xpatternedit* __xrtPatternBuilderPrepareAdd(
	xpatternbuilder* pBuilder, const xpatternspec* arrSpec, size_t iCount, cstr sOperation
)
{
	xpatternedit* pEdit;
	uint32 iFree;
	size_t iFresh;
	if ( !__xrtPatternBuilderPrepareAllowed(pBuilder) ) {
		return NULL;
	}
	if ( (arrSpec == NULL) && (iCount != 0) ) {
		__xrtPatternSetInvalidArgument();
		return NULL;
	}
	if ( (iCount > (pBuilder->Options.MaxPatterns - pBuilder->Count)) ||
		 (iCount > (UINT64_MAX - pBuilder->NextOrder)) ) {
		__xrtPatternError(XERR_RANGE, XPATTERN_ERROR_LIMIT, sOperation,
			"builder batch exceeds its pattern limit", false, 0, false, 0);
		return NULL;
	}
	if ( (iCount != 0) && !__xrtPatternBuilderCanModify(pBuilder) ) {
		return NULL;
	}
	pEdit = __xrtPatternEditCreate(__XRT_PATTERN_EDIT_ADD, iCount);
	if ( pEdit == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		pEdit->Items[i].Source = __xrtPatternSourceCreate(&arrSpec[i], &pBuilder->Options,
			sOperation, XPATTERN_ID_INVALID, pBuilder->NextOrder + i, true, i);
		if ( pEdit->Items[i].Source == NULL ) {
			xrtPatternEditFree(pEdit);
			return NULL;
		}
	}
	if ( !__xrtPatternBuilderReserveValid(pBuilder, pBuilder->Count + iCount) ) {
		xrtPatternEditFree(pEdit);
		return NULL;
	}
	/* 只读取空闲链，取消不会消费 ID、注册顺序或槽代际。 */
	iFree = pBuilder->FreeSlot;
	iFresh = pBuilder->SlotCount;
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iSlot;
		if ( iFree != __XRT_PATTERN_SLOT_NONE ) {
			iSlot = iFree;
			iFree = pBuilder->Slots[iSlot].NextFree;
		} else {
			iSlot = iFresh++;
		}
		pEdit->Items[i].Id = __xrtPatternBuilderId(iSlot, pBuilder->Slots[iSlot].Generation);
		pEdit->Items[i].Source->Id = pEdit->Items[i].Id;
	}
	pEdit->Builder = pBuilder;
	pBuilder->Pending = pEdit;
	return pEdit;
}



XRT_API xpatternedit* xrtPatternBuilderPrepareAdd(
	xpatternbuilder* pBuilder, const xpatternspec* arrSpec, size_t iCount
)
{
	return __xrtPatternBuilderPrepareAdd(pBuilder, arrSpec, iCount, "builder_prepare_add");
}



XRT_API xpatternedit* xrtPatternBuilderPrepareSet(
	xpatternbuilder* pBuilder, xpatternid Id, const xpatternspec* pSpec
)
{
	__xrt_pattern_builder_slot* pSlot;
	xpatternedit* pEdit;
	if ( !__xrtPatternBuilderPrepareAllowed(pBuilder) ) return NULL;
	if ( pSpec == NULL ) { __xrtPatternSetInvalidArgument(); return NULL; }
	pSlot = __xrtPatternBuilderSlot(pBuilder, Id, NULL);
	if ( pSlot == NULL ) { __xrtPatternSetInvalidState(); return NULL; }
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) return NULL;
	pEdit = __xrtPatternEditCreate(__XRT_PATTERN_EDIT_SET, 1u);
	if ( pEdit == NULL ) return NULL;
	pEdit->Items[0].Id = Id;
	pEdit->Items[0].Source = __xrtPatternSourceCreate(pSpec, &pBuilder->Options,
		"builder_prepare_set", Id, pSlot->Source->Order, false, 0);
	if ( pEdit->Items[0].Source == NULL ) { xrtPatternEditFree(pEdit); return NULL; }
	pEdit->Builder = pBuilder; pBuilder->Pending = pEdit;
	return pEdit;
}



XRT_API xpatternedit* xrtPatternBuilderPrepareRemove(xpatternbuilder* pBuilder, xpatternid Id)
{
	__xrt_pattern_builder_slot* pSlot;
	xpatternedit* pEdit;
	if ( !__xrtPatternBuilderPrepareAllowed(pBuilder) ) return NULL;
	pSlot = __xrtPatternBuilderSlot(pBuilder, Id, NULL);
	if ( (pSlot == NULL) || (pSlot->Generation == UINT32_MAX) ) {
		__xrtPatternSetInvalidState(); return NULL;
	}
	if ( !__xrtPatternBuilderCanModify(pBuilder) ) return NULL;
	pEdit = __xrtPatternEditCreate(__XRT_PATTERN_EDIT_REMOVE, 1u);
	if ( pEdit == NULL ) return NULL;
	pEdit->Items[0].Id = Id; pEdit->Builder = pBuilder; pBuilder->Pending = pEdit;
	return pEdit;
}



XRT_API xpatternedit* xrtPatternBuilderPrepareClear(xpatternbuilder* pBuilder)
{
	xpatternedit* pEdit;
	if ( !__xrtPatternBuilderPrepareAllowed(pBuilder) ) return NULL;
	if ( (pBuilder->Count != 0) && !__xrtPatternBuilderClearAllowed(pBuilder) ) return NULL;
	pEdit = __xrtPatternEditCreate(__XRT_PATTERN_EDIT_CLEAR, 0);
	if ( pEdit == NULL ) return NULL;
	pEdit->Builder = pBuilder; pBuilder->Pending = pEdit;
	return pEdit;
}



XRT_API bool xrtPatternEditCommit(xpatternedit* pEdit)
{
	xpatternbuilder* pBuilder;
	if ( !xrtPatternEditReady(pEdit) ) { __xrtPatternSetInvalidState(); return false; }
	pBuilder = pEdit->Builder;
	switch ( pEdit->Kind ) {
	case __XRT_PATTERN_EDIT_ADD:
		for ( size_t i = 0; i < pEdit->Count; i++ ) {
			size_t iSlot = __xrtPatternBuilderTakeSlot(pBuilder);
			pBuilder->Slots[iSlot].Source = pEdit->Items[i].Source;
			pEdit->Items[i].Source = NULL;
		}
		if ( pEdit->Count != 0 ) {
			pBuilder->Count += pEdit->Count;
			pBuilder->NextOrder += pEdit->Count;
			pBuilder->Version++;
		}
		break;
	case __XRT_PATTERN_EDIT_SET: {
		__xrt_pattern_builder_slot* pSlot = &pBuilder->Slots[(uint32)pEdit->Items[0].Id - 1u];
		__xrtPatternSourceFree(pSlot->Source);
		pSlot->Source = pEdit->Items[0].Source; pEdit->Items[0].Source = NULL;
		pBuilder->Version++;
		break;
	}
	case __XRT_PATTERN_EDIT_REMOVE:
		__xrtPatternBuilderRemoveValid(pBuilder, (uint32)pEdit->Items[0].Id - 1u);
		break;
	case __XRT_PATTERN_EDIT_CLEAR:
		if ( pBuilder->Count != 0 ) __xrtPatternBuilderClearValid(pBuilder);
		break;
	}
	pBuilder->Pending = NULL; pEdit->Builder = NULL;
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
