#include "../internal/xrt_http_cache_store.h"



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 安全累加紧凑布局大小。 */
static bool __xrtHttpCacheSizeAdd(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( (pResult == NULL) || (iLeft > (SIZE_MAX - iRight)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 安全累加带零结尾的文本大小。 */
static bool __xrtHttpCacheTextSizeAdd(
	size_t* pTotal,
	size_t iTextSize
)
{
	size_t iStoredSize;

	if ( !__xrtHttpCacheSizeAdd(
		iTextSize, 1u, &iStoredSize
	) ) {
		return false;
	}
	return __xrtHttpCacheSizeAdd(
		*pTotal, iStoredSize, pTotal
	);
}



/* 安全计算数组字节数。 */
static bool __xrtHttpCacheSizeMultiply(
	size_t iCount,
	size_t iSize,
	size_t* pResult
)
{
	if ( (pResult == NULL) ||
		((iSize != 0) && (iCount > (SIZE_MAX / iSize))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pResult = iCount * iSize;
	return true;
}



/* 在总布局中加入一个满足 ABI 对齐的数组。 */
static bool __xrtHttpCacheLayoutArray(
	size_t* pTotal,
	size_t iCount,
	size_t iSize,
	size_t iAlignment,
	size_t* pOffset
)
{
	size_t iPadding;
	size_t iBytes;

	if ( (pTotal == NULL) || (pOffset == NULL) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pOffset = 0;
	if ( iCount == 0 ) {
		return true;
	}
	iPadding = (iAlignment - (*pTotal & (iAlignment - 1u))) &
		(iAlignment - 1u);
	if ( !__xrtHttpCacheSizeAdd(
		*pTotal, iPadding, pTotal
	) || !__xrtHttpCacheSizeMultiply(
		iCount, iSize, &iBytes
	) ) {
		return false;
	}
	*pOffset = *pTotal;
	return __xrtHttpCacheSizeAdd(*pTotal, iBytes, pTotal);
}



/* 判断两个借用视图是否逐字节相同。 */
static bool __xrtHttpCacheViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证目标 URI 可以作为不含控制字符或空格的稳定键。 */
static bool __xrtHttpCacheURIValid(xstrview URI)
{
	size_t i;

	if ( !__xrtHttpViewValid(URI) || (URI.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < URI.Size; i++ ) {
		uint8 iByte = (uint8)URI.Data[i];

		if ( (iByte <= 0x20u) || (iByte == 0x7fu) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	return true;
}



/* 验证主键视图、方法、URI 和字段数组，并复制到对齐快照。 */
bool __xrtHttpCacheKeyResolve(
	const xhttpcachekey* pInput,
	xhttpcachekey* pKey
)
{
	xhttpcachekey Key;

	if ( !__xrtRangeValid(pInput, sizeof(Key)) ||
		(pKey == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Key, pInput, sizeof(Key));
	if ( !__xrtHttpViewValid(Key.Method) ||
		!__xrtHttpViewValid(Key.Partition) ||
		!__xrtHttpFieldArrayValid(
			Key.Fields, Key.FieldCount
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Key.Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpCacheURIValid(Key.URI) ) {
		return false;
	}
	*pKey = Key;
	return true;
}



/* 判断一段内存是否覆盖已验证主键或任一借用内容。 */
bool __xrtHttpCacheKeyOverlap(
	const xhttpcachekey* pKey,
	const void* pMemory,
	size_t iSize
)
{
	return __xrtRangesOverlap(
		pKey->Method.Data, pKey->Method.Size,
		pMemory, iSize
	) || __xrtRangesOverlap(
		pKey->URI.Data, pKey->URI.Size,
		pMemory, iSize
	) || __xrtRangesOverlap(
		pKey->Partition.Data, pKey->Partition.Size,
		pMemory, iSize
	) || __xrtHttpFieldArrayOverlap(
		pKey->Fields, pKey->FieldCount,
		pMemory, iSize
	);
}



/* 验证 Record 的响应元数据、时钟和字段数组。 */
static bool __xrtHttpCacheRecordHeadValid(
	const xhttpcacherecordinput* pInput
)
{
	xhttpcachekey Key;
	uint32 iKnownFlags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;

	if ( pInput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&pInput->Key, &Key) ||
		!__xrtHttpViewValid(pInput->Reason) ||
		!__xrtHttpFieldArrayValid(
			pInput->Fields, pInput->FieldCount
		) ||
		!__xrtHttpFieldArrayValid(
			pInput->Trailers, pInput->TrailerCount
		) ) {
		return false;
	}
	if ( ((pInput->Parts == NULL) && (pInput->PartCount != 0)) ||
		(pInput->PartCount >
		 (SIZE_MAX / sizeof(*pInput->Parts))) ||
		!__xrtRangeValid(
			pInput->Parts,
			pInput->PartCount * sizeof(*pInput->Parts)
		) ||
		((pInput->Version != XHTTP_VERSION_1_0) &&
		 (pInput->Version != XHTTP_VERSION_1_1)) ||
		(pInput->Status < 200) || (pInput->Status > 999) ||
		((pInput->Flags & ~iKnownFlags) != 0) ||
		(((pInput->Flags & XHTTP_CACHE_RECORD_HAS_LENGTH) == 0) &&
		 (pInput->Length != 0)) ||
		(((pInput->Flags & XHTTP_CACHE_RECORD_COMPLETE) != 0) &&
		 ((pInput->Flags & XHTTP_CACHE_RECORD_HAS_LENGTH) == 0)) ||
		(pInput->RequestClock > pInput->ResponseClock) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtHttpFieldValueValid(pInput->Reason);
}



/* 验证片段排序、覆盖范围和完整标志，并累计正文大小。 */
static bool __xrtHttpCachePartsValid(
	const xhttpcacherecordinput* pInput,
	size_t* pBodySize,
	uint64* pBodyBytes
)
{
	xhttpcachepart Part;
	uint64 iPreviousEnd = 0;
	uint64 iBodyBytes = 0;
	size_t iBodySize = 0;
	size_t i;

	if ( (pBodySize == NULL) || (pBodyBytes == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < pInput->PartCount; i++ ) {
		uint64 iSize;
		uint64 iEnd;

		memcpy(
			&Part,
			((const uint8*)pInput->Parts) +
				(i * sizeof(Part)),
			sizeof(Part)
		);
		if ( !__xrtRangeValid(Part.Data.Data, Part.Data.Size) ||
			(Part.Data.Size == 0) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		iSize = (uint64)Part.Data.Size;
		if ( (uint64)(size_t)iSize != iSize ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		if ( Part.Offset > (UINT64_MAX - iSize) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iEnd = Part.Offset + iSize;
		if ( (i != 0) && (Part.Offset < iPreviousEnd) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( ((pInput->Flags &
				XHTTP_CACHE_RECORD_HAS_LENGTH) != 0) &&
			(iEnd > pInput->Length) ) {
			__xrtErrorSetRange();
			return false;
		}
		if ( iBodyBytes > (UINT64_MAX - iSize) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBodyBytes += iSize;
		if ( !__xrtHttpCacheSizeAdd(
			iBodySize, Part.Data.Size, &iBodySize
		) ) {
			return false;
		}
		iPreviousEnd = iEnd;
	}
	if ( (pInput->Flags & XHTTP_CACHE_RECORD_COMPLETE) != 0 ) {
		uint64 iExpected = 0;

		for ( i = 0; i < pInput->PartCount; i++ ) {
			memcpy(
				&Part,
				((const uint8*)pInput->Parts) +
					(i * sizeof(Part)),
				sizeof(Part)
			);
			if ( Part.Offset != iExpected ) {
				__xrtErrorSetValue();
				return false;
			}
			iExpected += (uint64)Part.Data.Size;
		}
		if ( iExpected != pInput->Length ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	*pBodySize = iBodySize;
	*pBodyBytes = iBodyBytes;
	return true;
}



/* 判断当前 Vary 成员是否已经在更早成员中出现。 */
static bool __xrtHttpCacheVaryDuplicate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iBefore,
	xstrview Name
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	size_t iSeen = 0;

	xrtHttpVaryCursorInit(&Cursor);
	while ( iSeen < iBefore ) {
		xhttpnext Next = xrtHttpVaryNext(
			pFields, iCount, &Cursor, &Item
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( !Item.Wildcard &&
			xrtHttpFieldNameEqual(Item.Name, Name) ) {
			return true;
		}
		iSeen++;
	}
	return false;
}



/* 累计去重 Vary 维度、原请求字段和对应文本大小。 */
static bool __xrtHttpCacheVaryMeasure(
	const xhttpcacherecordinput* pInput,
	size_t* pVaryCount,
	size_t* pVaryFieldCount,
	size_t* pTextSize
)
{
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	size_t iItem = 0;
	size_t iVaryCount = 0;
	size_t iFieldCount = 0;
	size_t iTextSize = 0;

	if ( (pVaryCount == NULL) ||
		(pVaryFieldCount == NULL) ||
		(pTextSize == NULL) ||
		!xrtHttpVaryPlan(
			pInput->Fields, pInput->FieldCount, &Plan
		) ) {
		return false;
	}
	if ( (Plan.Flags & XHTTP_VARY_WILDCARD) != 0 ) {
		__xrtErrorSetUnsupported();
		return false;
	}
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pInput->Fields,
		pInput->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		size_t i;

		if ( __xrtHttpCacheVaryDuplicate(
			pInput->Fields,
			pInput->FieldCount,
			iItem,
			Item.Name
		) ) {
			iItem++;
			continue;
		}
		if ( (iVaryCount == SIZE_MAX) ||
			!__xrtHttpCacheTextSizeAdd(
				&iTextSize, Item.Name.Size
			) ) {
			return false;
		}
		iVaryCount++;
		for ( i = 0; i < pInput->Key.FieldCount; i++ ) {
			xhttpfield Field;

			__xrtHttpFieldLoad(
				pInput->Key.Fields, i, &Field
			);
			if ( !xrtHttpFieldNameEqual(
				Field.Name, Item.Name
			) ) {
				continue;
			}
			if ( (iFieldCount == SIZE_MAX) ||
				!__xrtHttpCacheTextSizeAdd(
					&iTextSize, Field.Value.Size
				) ) {
				return false;
			}
			iFieldCount++;
		}
		iItem++;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	*pVaryCount = iVaryCount;
	*pVaryFieldCount = iFieldCount;
	*pTextSize = iTextSize;
	return true;
}



/* 累计字段数组的名称、值和零结尾文本大小。 */
static bool __xrtHttpCacheFieldsMeasure(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pTextSize
)
{
	xhttpfield Field;
	size_t iTextSize = *pTextSize;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		size_t iFieldSize;

		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpCacheSizeAdd(
			Field.Name.Size,
			Field.Value.Size,
			&iFieldSize
		) || !__xrtHttpCacheSizeAdd(
			iFieldSize, 2u, &iFieldSize
		) || !__xrtHttpCacheSizeAdd(
			iTextSize, iFieldSize, &iTextSize
		) ) {
			return false;
		}
	}
	*pTextSize = iTextSize;
	return true;
}



/* 复制一个文本视图并追加只用于诊断和 C 互操作的零字符。 */
static xstrview __xrtHttpCacheTextCopy(
	char** ppWrite,
	xstrview Text
)
{
	char* pWrite = *ppWrite;
	xstrview Copy;

	if ( Text.Size != 0 ) {
		memcpy(pWrite, Text.Data, Text.Size);
	}
	pWrite[Text.Size] = '\0';
	Copy.Data = pWrite;
	Copy.Size = Text.Size;
	*ppWrite = pWrite + Text.Size + 1u;
	return Copy;
}



/* 把字段数组复制到 Record 的连续文本区。 */
static void __xrtHttpCacheFieldsCopy(
	xhttpfield* pOutput,
	const xhttpfield* pInput,
	size_t iCount,
	char** ppWrite
)
{
	xhttpfield Field;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pInput, i, &Field);
		pOutput[i].Name = __xrtHttpCacheTextCopy(
			ppWrite, Field.Name
		);
		pOutput[i].Value = __xrtHttpCacheTextCopy(
			ppWrite, Field.Value
		);
	}
}



/* 复制去重 Vary 名称及其对应的原请求字段值。 */
static bool __xrtHttpCacheVaryCopy(
	xhttpcacherecord* pRecord,
	const xhttpcacherecordinput* pInput,
	char** ppWrite
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	size_t iItem = 0;
	size_t iVary = 0;
	size_t iField = 0;

	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pInput->Fields,
		pInput->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		xrt_http_cache_vary* pVary;
		size_t i;

		if ( __xrtHttpCacheVaryDuplicate(
			pInput->Fields,
			pInput->FieldCount,
			iItem,
			Item.Name
		) ) {
			iItem++;
			continue;
		}
		pVary = &pRecord->Vary[iVary++];
		pVary->Name = __xrtHttpCacheTextCopy(
			ppWrite, Item.Name
		);
		pVary->Field = iField;
		for ( i = 0; i < pInput->Key.FieldCount; i++ ) {
			xhttpfield Source;
			xhttpfield* pTarget;

			__xrtHttpFieldLoad(
				pInput->Key.Fields, i, &Source
			);
			if ( !xrtHttpFieldNameEqual(
				Source.Name, Item.Name
			) ) {
				continue;
			}
			pTarget = (xhttpfield*)&pRecord->Key.Fields[
				iField++
			];
			pTarget->Name = pVary->Name;
			pTarget->Value = __xrtHttpCacheTextCopy(
				ppWrite, Source.Value
			);
			pVary->FieldCount++;
		}
		iItem++;
	}
	return Next != XHTTP_NEXT_ERROR;
}



/* 复制正文片段并把每个公开视图重新指向 Record 内存。 */
static void __xrtHttpCachePartsCopy(
	xhttpcachepart* pOutput,
	const xhttpcachepart* pInput,
	size_t iCount,
	char** ppWrite
)
{
	xhttpcachepart Part;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Part,
			((const uint8*)pInput) + (i * sizeof(Part)),
			sizeof(Part)
		);
		pOutput[i].Offset = Part.Offset;
		pOutput[i].Data.Data = (cbytes)*ppWrite;
		pOutput[i].Data.Size = Part.Data.Size;
		memcpy(
			*ppWrite,
			Part.Data.Data,
			Part.Data.Size
		);
		*ppWrite += Part.Data.Size;
	}
}



/* 从 Date 或接收时间建立多个匹配项的稳定新旧顺序。 */
static bool __xrtHttpCacheSelectionTime(
	xhttpcacherecord* pRecord
)
{
	xhttpcachetime Time;

	if ( !xrtHttpCacheTimeParse(
		pRecord->Fields,
		pRecord->FieldCount,
		&Time
	) ) {
		return false;
	}
	pRecord->SelectionTime = pRecord->ResponseTime;
	if ( (Time.DateCount == 1) &&
		((Time.Flags & XHTTP_CACHE_TIME_DATE) != 0) &&
		((Time.Flags & (XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID)) == 0) ) {
		pRecord->SelectionTime = Time.Date;
	}
	return true;
}



/* 初始化适合通用私有客户端缓存的条目、总字节和单条目限额。 */
XRT_API void xrtHttpCacheConfigInit(xhttpcacheconfig* pConfig)
{
	const xhttpcacheconfig Config = {
		64u,
		XHTTP_CACHE_ENTRIES_DEFAULT,
		(size_t)XHTTP_CACHE_BYTES_DEFAULT,
		(size_t)XHTTP_CACHE_ENTRY_BYTES_DEFAULT
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化没有分区和请求字段的主键。 */
XRT_API bool xrtHttpCacheKeyInit(
	xhttpcachekey* pKey,
	xstrview Method,
	xstrview URI
)
{
	xhttpcachekey Key;

	memset(&Key, 0, sizeof(Key));
	Key.Method = Method;
	Key.URI = URI;
	if ( !__xrtRangeValid(pKey, sizeof(Key)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&Key, &Key) ) {
		return false;
	}
	if ( __xrtHttpCacheKeyOverlap(&Key, pKey, sizeof(Key)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pKey, &Key, sizeof(Key));
	return true;
}



/* 初始化 HTTP/1.1 响应记录输入并借用完整 Key。 */
XRT_API bool xrtHttpCacheRecordInputInit(
	xhttpcacherecordinput* pInput,
	const xhttpcachekey* pKey,
	uint16 iStatus
)
{
	xhttpcachekey Key;
	xhttpcacherecordinput Input;

	if ( !__xrtRangeValid(pInput, sizeof(Input)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pKey, sizeof(Key), pInput, sizeof(Input)
	) || __xrtHttpCacheKeyOverlap(
		&Key, pInput, sizeof(Input)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iStatus < 200) || (iStatus > 999) ) {
		__xrtErrorSetRange();
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Key = Key;
	Input.Version = XHTTP_VERSION_1_1;
	Input.Status = iStatus;
	memcpy(pInput, &Input, sizeof(Input));
	return true;
}



/* 创建完全拥有输入内容的单分配不可变 Record。 */
XRT_API xhttpcacherecord* xrtHttpCacheRecordCreate(
	const xhttpcacherecordinput* pInput
)
{
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord;
	size_t iFieldOffset;
	size_t iTrailerOffset;
	size_t iPartOffset;
	size_t iVaryOffset;
	size_t iVaryFieldOffset;
	size_t iTextOffset;
	size_t iTextSize = 0;
	size_t iBodySize;
	size_t iVaryCount;
	size_t iVaryFieldCount;
	size_t iTotal = sizeof(*pRecord);
	uint64 iBodyBytes;
	char* pWrite;

	if ( !__xrtRangeValid(pInput, sizeof(Input)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memcpy(&Input, pInput, sizeof(Input));
	pInput = &Input;
	if ( !__xrtHttpCacheRecordHeadValid(pInput) ||
		!__xrtHttpCachePartsValid(
			pInput, &iBodySize, &iBodyBytes
		) ||
		!__xrtHttpCacheVaryMeasure(
			pInput,
			&iVaryCount,
			&iVaryFieldCount,
			&iTextSize
		) ||
		!__xrtHttpCacheFieldsMeasure(
			pInput->Fields,
			pInput->FieldCount,
			&iTextSize
		) ||
		!__xrtHttpCacheFieldsMeasure(
			pInput->Trailers,
			pInput->TrailerCount,
			&iTextSize
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.Method.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.URI.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.Partition.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Reason.Size
		) ||
		!__xrtHttpCacheSizeAdd(
			iTextSize, iBodySize, &iTextSize
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->FieldCount,
			sizeof(xhttpfield),
			XRT_INTERNAL_ALIGNOF(xhttpfield),
			&iFieldOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->TrailerCount,
			sizeof(xhttpfield),
			XRT_INTERNAL_ALIGNOF(xhttpfield),
			&iTrailerOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->PartCount,
			sizeof(xhttpcachepart),
			XRT_INTERNAL_ALIGNOF(xhttpcachepart),
			&iPartOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			iVaryCount,
			sizeof(xrt_http_cache_vary),
			XRT_INTERNAL_ALIGNOF(xrt_http_cache_vary),
			&iVaryOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			iVaryFieldCount,
			sizeof(xhttpfield),
			XRT_INTERNAL_ALIGNOF(xhttpfield),
			&iVaryFieldOffset
		) ) {
		return NULL;
	}
	iTextOffset = iTotal;
	if ( !__xrtHttpCacheSizeAdd(
		iTotal, iTextSize, &iTotal
	) ) {
		return NULL;
	}
	pRecord = (xhttpcacherecord*)xrtCalloc(1, iTotal);
	if ( pRecord == NULL ) {
		return NULL;
	}
	pRecord->References = 1;
	pRecord->Charge = iTotal;
	pRecord->Version = pInput->Version;
	pRecord->Status = pInput->Status;
	pRecord->Flags = pInput->Flags;
	pRecord->FieldCount = pInput->FieldCount;
	pRecord->TrailerCount = pInput->TrailerCount;
	pRecord->PartCount = pInput->PartCount;
	pRecord->BodyBytes = iBodyBytes;
	pRecord->Length = pInput->Length;
	pRecord->ResponseTime = pInput->ResponseTime;
	pRecord->RequestClock = pInput->RequestClock;
	pRecord->ResponseClock = pInput->ResponseClock;
	pRecord->VaryCount = iVaryCount;
	pRecord->Fields = iFieldOffset != 0 ?
		(xhttpfield*)((uint8*)pRecord + iFieldOffset) : NULL;
	pRecord->Trailers = iTrailerOffset != 0 ?
		(xhttpfield*)((uint8*)pRecord + iTrailerOffset) : NULL;
	pRecord->Parts = iPartOffset != 0 ?
		(xhttpcachepart*)((uint8*)pRecord + iPartOffset) : NULL;
	pRecord->Vary = iVaryOffset != 0 ?
		(xrt_http_cache_vary*)((uint8*)pRecord + iVaryOffset) :
		NULL;
	pRecord->Key.Fields = iVaryFieldOffset != 0 ?
		(const xhttpfield*)((uint8*)pRecord + iVaryFieldOffset) :
		NULL;
	pRecord->Key.FieldCount = iVaryFieldCount;
	pWrite = (char*)pRecord + iTextOffset;
	pRecord->Key.Method = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.Method
	);
	pRecord->Key.URI = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.URI
	);
	pRecord->Key.Partition = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.Partition
	);
	pRecord->Reason = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Reason
	);
	__xrtHttpCacheFieldsCopy(
		pRecord->Fields,
		pInput->Fields,
		pInput->FieldCount,
		&pWrite
	);
	__xrtHttpCacheFieldsCopy(
		pRecord->Trailers,
		pInput->Trailers,
		pInput->TrailerCount,
		&pWrite
	);
	if ( !__xrtHttpCacheVaryCopy(
		pRecord, pInput, &pWrite
	) ) {
		xrtFree(pRecord);
		return NULL;
	}
	__xrtHttpCachePartsCopy(
		pRecord->Parts,
		pInput->Parts,
		pInput->PartCount,
		&pWrite
	);
	if ( !__xrtHttpCacheSelectionTime(pRecord) ) {
		xrtFree(pRecord);
		return NULL;
	}
	return pRecord;
}



/* 增加 Record 引用并返回原指针。 */
XRT_API xhttpcacherecord* xrtHttpCacheRecordRetain(
	const xhttpcacherecord* pRecord
)
{
	if ( (pRecord == NULL) ||
		(xrtRefRetain(
			&((xhttpcacherecord*)pRecord)->References
		) < 0) ) {
		return NULL;
	}
	return (xhttpcacherecord*)pRecord;
}



/* 释放最后一个 Record 引用及其唯一紧凑分配。 */
XRT_API void xrtHttpCacheRecordRelease(xhttpcacherecord* pRecord)
{
	if ( (pRecord == NULL) ||
		(xrtRefRelease(&pRecord->References) != 0) ) {
		return;
	}
	xrtFree(pRecord);
}



/* 返回 Record 拥有的主键。 */
XRT_API const xhttpcachekey* xrtHttpCacheRecordKey(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? &pRecord->Key : NULL;
}



/* 返回响应协议版本。 */
XRT_API xhttpversion xrtHttpCacheRecordVersion(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Version : 0;
}



/* 返回最终响应状态码。 */
XRT_API uint16 xrtHttpCacheRecordStatus(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Status : 0;
}



/* 返回 Record 标志。 */
XRT_API uint32 xrtHttpCacheRecordFlags(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Flags : 0;
}



/* 返回借用的 reason phrase。 */
XRT_API xstrview xrtHttpCacheRecordReason(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ?
		pRecord->Reason : (xstrview){ NULL, 0 };
}



/* 返回响应 Header 数量。 */
XRT_API size_t xrtHttpCacheRecordFieldCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->FieldCount : 0;
}



/* 返回指定响应 Header。 */
XRT_API const xhttpfield* xrtHttpCacheRecordFieldAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->FieldCount) ?
		&pRecord->Fields[iIndex] : NULL;
}



/* 返回首个同名响应 Header。 */
XRT_API const xhttpfield* xrtHttpCacheRecordField(
	const xhttpcacherecord* pRecord,
	xstrview Name
)
{
	size_t i;

	if ( (pRecord == NULL) || !xrtHttpTokenValid(Name) ) {
		return NULL;
	}
	for ( i = 0; i < pRecord->FieldCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pRecord->Fields[i].Name, Name
		) ) {
			return &pRecord->Fields[i];
		}
	}
	return NULL;
}



/* 返回响应 Trailer 数量。 */
XRT_API size_t xrtHttpCacheRecordTrailerCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->TrailerCount : 0;
}



/* 返回指定响应 Trailer。 */
XRT_API const xhttpfield* xrtHttpCacheRecordTrailerAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) &&
		(iIndex < pRecord->TrailerCount) ?
			&pRecord->Trailers[iIndex] : NULL;
}



/* 返回正文片段数量。 */
XRT_API size_t xrtHttpCacheRecordPartCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->PartCount : 0;
}



/* 返回指定不可变正文片段。 */
XRT_API const xhttpcachepart* xrtHttpCacheRecordPartAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->PartCount) ?
		&pRecord->Parts[iIndex] : NULL;
}



/* 返回全部片段有效载荷字节数。 */
XRT_API uint64 xrtHttpCacheRecordBodyBytes(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->BodyBytes : 0;
}



/* 返回完整表示长度。 */
XRT_API uint64 xrtHttpCacheRecordLength(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Length : 0;
}



/* 返回响应接收墙钟时间。 */
XRT_API xtime xrtHttpCacheRecordResponseTime(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->ResponseTime : 0;
}



/* 返回发出请求时的单调时钟。 */
XRT_API uint64 xrtHttpCacheRecordRequestClock(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->RequestClock : 0;
}



/* 返回收到响应时的单调时钟。 */
XRT_API uint64 xrtHttpCacheRecordResponseClock(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->ResponseClock : 0;
}



/* 返回 Record 单次紧凑分配的实际字节数。 */
XRT_API size_t xrtHttpCacheRecordCharge(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Charge : 0;
}



/* 返回去重后的 Vary 选择字段数量。 */
XRT_API size_t xrtHttpCacheRecordVaryCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->VaryCount : 0;
}



/* 返回指定 Vary 字段名。 */
XRT_API xstrview xrtHttpCacheRecordVaryAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->VaryCount) ?
		pRecord->Vary[iIndex].Name :
		(xstrview){ NULL, 0 };
}



/* 去掉允许的首尾 OWS 后返回字段值。 */
static xstrview __xrtHttpCacheValueTrim(xstrview Value)
{
	size_t iBegin = 0;
	size_t iEnd = Value.Size;

	while ( (iBegin < iEnd) &&
		((Value.Data[iBegin] == ' ') ||
		 (Value.Data[iBegin] == '\t')) ) {
		iBegin++;
	}
	while ( (iEnd > iBegin) &&
		((Value.Data[iEnd - 1u] == ' ') ||
		 (Value.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return (xstrview){
		Value.Data != NULL ? Value.Data + iBegin : NULL,
		iEnd - iBegin
	};
}



/* 比较一个 Vary 维度在保存请求和当前请求中的字段序列。 */
static bool __xrtHttpCacheVaryMatches(
	const xhttpcacherecord* pRecord,
	const xrt_http_cache_vary* pVary,
	const xhttpcachekey* pKey
)
{
	size_t iStored = 0;
	size_t i;

	for ( i = 0; i < pKey->FieldCount; i++ ) {
		xstrview Stored;
		xstrview Current;

		if ( !xrtHttpFieldNameEqual(
			pKey->Fields[i].Name, pVary->Name
		) ) {
			continue;
		}
		if ( iStored >= pVary->FieldCount ) {
			return false;
		}
		Stored = __xrtHttpCacheValueTrim(
			pRecord->Key.Fields[
				pVary->Field + iStored
			].Value
		);
		Current = __xrtHttpCacheValueTrim(
			pKey->Fields[i].Value
		);
		if ( !__xrtHttpCacheViewEqual(Stored, Current) ) {
			return false;
		}
		iStored++;
	}
	return iStored == pVary->FieldCount;
}



/* 判断已校验主键和全部 Vary 原请求字段是否匹配。 */
bool __xrtHttpCacheRecordMatchesValid(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
)
{
	size_t i;

	if ( !__xrtHttpCacheViewEqual(
		pRecord->Key.Method, pKey->Method
	) || !__xrtHttpCacheViewEqual(
		pRecord->Key.URI, pKey->URI
	) || !__xrtHttpCacheViewEqual(
		pRecord->Key.Partition, pKey->Partition
	) ) {
		return false;
	}
	for ( i = 0; i < pRecord->VaryCount; i++ ) {
		if ( !__xrtHttpCacheVaryMatches(
			pRecord, &pRecord->Vary[i], pKey
		) ) {
			return false;
		}
	}
	return true;
}



/* 判断主键和全部 Vary 原请求字段是否匹配。 */
XRT_API bool xrtHttpCacheRecordMatches(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
)
{
	xhttpcachekey Key;

	if ( pRecord == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		return false;
	}
	return __xrtHttpCacheRecordMatchesValid(pRecord, &Key);
}



/* 查找 Record 中指定名称的 Vary 维度。 */
static const xrt_http_cache_vary* __xrtHttpCacheVaryFind(
	const xhttpcacherecord* pRecord,
	xstrview Name
)
{
	size_t i;

	for ( i = 0; i < pRecord->VaryCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pRecord->Vary[i].Name, Name
		) ) {
			return &pRecord->Vary[i];
		}
	}
	return NULL;
}



/* 比较两个 Record 中一个 Vary 维度的保存字段序列。 */
static bool __xrtHttpCacheVaryEqual(
	const xhttpcacherecord* pLeft,
	const xrt_http_cache_vary* pLeftVary,
	const xhttpcacherecord* pRight,
	const xrt_http_cache_vary* pRightVary
)
{
	size_t i;

	if ( pLeftVary->FieldCount != pRightVary->FieldCount ) {
		return false;
	}
	for ( i = 0; i < pLeftVary->FieldCount; i++ ) {
		xstrview Left = __xrtHttpCacheValueTrim(
			pLeft->Key.Fields[pLeftVary->Field + i].Value
		);
		xstrview Right = __xrtHttpCacheValueTrim(
			pRight->Key.Fields[pRightVary->Field + i].Value
		);

		if ( !__xrtHttpCacheViewEqual(Left, Right) ) {
			return false;
		}
	}
	return true;
}



/* 比较两个 Record 是否描述同一个 Vary 变体。 */
bool __xrtHttpCacheRecordVariantEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
)
{
	size_t i;

	if ( (pLeft == NULL) || (pRight == NULL) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.Method, pRight->Key.Method
		) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.URI, pRight->Key.URI
		) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.Partition, pRight->Key.Partition
		) ||
		(pLeft->VaryCount != pRight->VaryCount) ) {
		return false;
	}
	for ( i = 0; i < pLeft->VaryCount; i++ ) {
		const xrt_http_cache_vary* pRightVary =
			__xrtHttpCacheVaryFind(
				pRight, pLeft->Vary[i].Name
			);

		if ( (pRightVary == NULL) ||
			!__xrtHttpCacheVaryEqual(
				pLeft,
				&pLeft->Vary[i],
				pRight,
				pRightVary
			) ) {
			return false;
		}
	}
	return true;
}



/* 返回用于多个匹配响应之间选择最新项的时间。 */
xtime __xrtHttpCacheRecordSelectionTime(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->SelectionTime : 0;
}



/* 构造验证协议层可直接使用的借用 Entry 视图。 */
XRT_API bool xrtHttpCacheRecordEntry(
	const xhttpcacherecord* pRecord,
	xhttpcacheentry* pEntry
)
{
	xhttpcacheentry Entry = { 0 };

	if ( (pRecord == NULL) ||
		!__xrtRangeValid(pEntry, sizeof(Entry)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Entry.Fields = pRecord->Fields;
	Entry.FieldCount = pRecord->FieldCount;
	Entry.ResponseTime = pRecord->ResponseTime;
	if ( (pRecord->Flags & XHTTP_CACHE_RECORD_COMPLETE) == 0 ) {
		Entry.Flags |= XHTTP_CACHE_ENTRY_PARTIAL;
	}
	memcpy(pEntry, &Entry, sizeof(Entry));
	return true;
}

#endif
