#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_STORE)

typedef struct xrt_x509_store_entry {
	bytes Data;
	size_t Size;
	xx509cert Certificate;
} xrt_x509_store_entry;



struct xx509store {
	xrt_x509_store_entry* Entries;
	xx509anchor* Anchors;
	size_t Count;
	size_t Capacity;
};



/* 包装信任库底层错误并保留原始原因链。 */
static void __xrtX509StoreError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		Kind, X509_ERROR_TRUST_STORE, sOperation,
		sMessage, SIZE_MAX, pCause
	);
}



/* 使用当前错误作为原因设置一项信任库错误。 */
static xx509result __xrtX509StoreFailure(
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

	__xrtX509StoreError(Kind, sOperation, sMessage, pCause);
	return X509_ERROR;
}



/* 判断输入 DER 是否已经存在于信任库中。 */
static bool __xrtX509StoreDuplicate(
	const xx509store* pStore,
	const void* pDer,
	size_t iSize
)
{
	for ( size_t i = 0; i < pStore->Count; i++ ) {
		const xrt_x509_store_entry* pEntry = &pStore->Entries[i];

		if ( (pEntry->Size == iSize) &&
			(memcmp(pEntry->Data, pDer, iSize) == 0) ) {
			return true;
		}
	}
	return false;
}



/* 同时增长证书与锚数组，任一分配失败都保持原对象不变。 */
static bool __xrtX509StoreReserve(xx509store* pStore, size_t iRequired)
{
	xrt_x509_store_entry* pEntries;
	xx509anchor* pAnchors;
	size_t iCapacity;

	if ( iRequired <= pStore->Capacity ) {
		return true;
	}
	iCapacity = pStore->Capacity == 0 ? 8u : pStore->Capacity;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( (iCapacity > (SIZE_MAX / sizeof(*pEntries))) ||
		(iCapacity > (SIZE_MAX / sizeof(*pAnchors))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pEntries = (xrt_x509_store_entry*)xrtMalloc(
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		return false;
	}
	pAnchors = (xx509anchor*)xrtMalloc(iCapacity * sizeof(*pAnchors));
	if ( pAnchors == NULL ) {
		xrtFree(pEntries);
		return false;
	}
	if ( pStore->Count != 0 ) {
		memcpy(
			pEntries, pStore->Entries,
			pStore->Count * sizeof(*pEntries)
		);
		memcpy(
			pAnchors, pStore->Anchors,
			pStore->Count * sizeof(*pAnchors)
		);
	}
	xrtFree(pStore->Entries);
	xrtFree(pStore->Anchors);
	pStore->Entries = pEntries;
	pStore->Anchors = pAnchors;
	pStore->Capacity = iCapacity;
	return true;
}



/* 接管一张独立 DER，严格解析后原子地加入信任库。 */
static xx509result __xrtX509StoreAddOwned(
	xx509store* pStore,
	bytes pDer,
	size_t iSize,
	cstr sOperation
)
{
	xx509cert Certificate;
	xx509anchor Anchor;

	if ( __xrtX509StoreDuplicate(pStore, pDer, iSize) ) {
		return X509_DONE;
	}
	if ( !xrtX509Parse(pDer, iSize, &Certificate) ) {
		return __xrtX509StoreFailure(
			sOperation, "trust anchor certificate parsing failed"
		);
	}
	if ( !xrtX509Anchor(&Certificate, &Anchor) ) {
		return __xrtX509StoreFailure(
			sOperation, "trust anchor extraction failed"
		);
	}
	if ( !__xrtX509StoreReserve(pStore, pStore->Count + 1u) ) {
		return __xrtX509StoreFailure(
			sOperation, "trust store growth failed"
		);
	}
	pStore->Entries[pStore->Count].Data = pDer;
	pStore->Entries[pStore->Count].Size = iSize;
	pStore->Entries[pStore->Count].Certificate = Certificate;
	pStore->Anchors[pStore->Count] = Anchor;
	pStore->Count++;
	return X509_VALUE;
}



/* 比较一个借用 PEM 标签与 CERTIFICATE。 */
static bool __xrtX509StoreCertificateLabel(xstrview Label)
{
	static const char Certificate[] = "CERTIFICATE";

	return (Label.Size == sizeof(Certificate) - 1u) &&
		(memcmp(Label.Data, Certificate, sizeof(Certificate) - 1u) == 0);
}



/* 创建空信任库。 */
XRT_API xx509store* xrtX509StoreCreate(void)
{
	xx509store* pStore = (xx509store*)xrtMalloc(sizeof(xx509store));

	if ( pStore == NULL ) {
		return NULL;
	}
	memset(pStore, 0, sizeof(*pStore));
	return pStore;
}



/* 回滚并释放指定索引之后的全部锚。 */
void __xrtX509StoreTruncate(xx509store* pStore, size_t iCount)
{
	if ( (pStore == NULL) || (iCount > pStore->Count) ) {
		return;
	}
	for ( size_t i = iCount; i < pStore->Count; i++ ) {
		xrtFree(pStore->Entries[i].Data);
		memset(&pStore->Entries[i], 0, sizeof(pStore->Entries[i]));
		memset(&pStore->Anchors[i], 0, sizeof(pStore->Anchors[i]));
	}
	pStore->Count = iCount;
}



/* 释放信任库拥有的全部资源。 */
XRT_API void xrtX509StoreFree(xx509store* pStore)
{
	if ( pStore == NULL ) {
		return;
	}
	__xrtX509StoreTruncate(pStore, 0);
	xrtFree(pStore->Entries);
	xrtFree(pStore->Anchors);
	xrtFree(pStore);
}



/* 已有源条目互异，克隆直接重建索引，不重复执行二次方去重扫描。 */
XRT_API xx509store* xrtX509StoreClone(const xx509store* pSource)
{
	xx509store* pCopy;

	if ( pSource == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pCopy = xrtX509StoreCreate();
	if ( pCopy == NULL ) {
		return NULL;
	}
	if ( !__xrtX509StoreReserve(pCopy, pSource->Count) ) {
		xrtX509StoreFree(pCopy);
		return NULL;
	}
	for ( size_t i = 0; i < pSource->Count; i++ ) {
		const xrt_x509_store_entry* pEntry = &pSource->Entries[i];
		xx509cert Certificate;
		xx509anchor Anchor;
		bytes pDer = (bytes)xrtMalloc(pEntry->Size);

		if ( pDer == NULL ) {
			xrtX509StoreFree(pCopy);
			return NULL;
		}
		memcpy(pDer, pEntry->Data, pEntry->Size);
		if ( !xrtX509Parse(pDer, pEntry->Size, &Certificate) ||
			!xrtX509Anchor(&Certificate, &Anchor) ) {
			xrtFree(pDer);
			xrtX509StoreFree(pCopy);
			return NULL;
		}
		pCopy->Entries[i].Data = pDer;
		pCopy->Entries[i].Size = pEntry->Size;
		pCopy->Entries[i].Certificate = Certificate;
		pCopy->Anchors[i] = Anchor;
		pCopy->Count++;
	}
	return pCopy;
}



/* 返回当前唯一信任锚数量。 */
XRT_API size_t xrtX509StoreCount(const xx509store* pStore)
{
	if ( pStore == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pStore->Count;
}



/* 复制并导入一张 DER 信任锚证书。 */
XRT_API xx509result xrtX509StoreAdd(
	xx509store* pStore,
	const void* pDer,
	size_t iSize
)
{
	bytes pCopy;
	xx509result Result;

	if ( (pStore == NULL) || (pDer == NULL) || (iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( __xrtX509StoreDuplicate(pStore, pDer, iSize) ) {
		return X509_DONE;
	}
	pCopy = (bytes)xrtMalloc(iSize);
	if ( pCopy == NULL ) {
		(void)__xrtX509StoreFailure(
			"x509-store-add", "trust anchor DER copy failed"
		);
		return X509_ERROR;
	}
	memcpy(pCopy, pDer, iSize);
	Result = __xrtX509StoreAddOwned(
		pStore, pCopy, iSize, "x509-store-add"
	);
	if ( Result != X509_VALUE ) {
		xrtFree(pCopy);
	}
	return Result;
}



/* 复用唯一导入算法；部分追加失败时只回滚本次新条目。 */
XRT_API bool xrtX509StoreMerge(
	xx509store* pStore,
	const xx509store* pSource,
	size_t* pAdded
)
{
	size_t iBefore;

	if ( (pStore == NULL) || (pSource == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBefore = pStore->Count;
	if ( pStore != pSource ) {
		for ( size_t i = 0; i < pSource->Count; i++ ) {
			const xrt_x509_store_entry* pEntry = &pSource->Entries[i];

			if ( xrtX509StoreAdd(pStore, pEntry->Data, pEntry->Size) ==
				X509_ERROR ) {
				__xrtX509StoreTruncate(pStore, iBefore);
				return false;
			}
		}
	}
	if ( pAdded != NULL ) {
		*pAdded = pStore->Count - iBefore;
	}
	return true;
}



/* 事务式导入一段 PEM 中的全部证书块。 */
XRT_API bool xrtX509StoreAddPem(
	xx509store* pStore,
	cstr sText,
	size_t iSize,
	size_t* pAdded
)
{
	xpemcursor Cursor;
	xpemblock Block;
	size_t iBefore;
	size_t iAdded = 0;
	bool bFound = false;

	if ( (pStore == NULL) || ((sText == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPemInit(&Cursor, sText, iSize) ) {
		return false;
	}
	iBefore = pStore->Count;
	while ( true ) {
		xpemresult Read = xrtPemRead(&Cursor, &Block);

		if ( Read == XPEM_ERROR ) {
			(void)__xrtX509StoreFailure(
				"x509-store-add-pem", "PEM trust store parsing failed"
			);
			__xrtX509StoreTruncate(pStore, iBefore);
			return false;
		}
		if ( Read == XPEM_DONE ) {
			break;
		}
		if ( !__xrtX509StoreCertificateLabel(Block.Label) ) {
			continue;
		}
		bFound = true;
		{
			size_t iDerSize;
			bytes pDer = xrtPemDecodeNew(&Block, &iDerSize);
			xx509result Add;

			if ( pDer == NULL ) {
				(void)__xrtX509StoreFailure(
					"x509-store-add-pem", "PEM certificate decoding failed"
				);
				__xrtX509StoreTruncate(pStore, iBefore);
				return false;
			}
			Add = __xrtX509StoreAddOwned(
				pStore, pDer, iDerSize, "x509-store-add-pem"
			);
			if ( Add != X509_VALUE ) {
				xrtFree(pDer);
			}
			if ( Add == X509_ERROR ) {
				__xrtX509StoreTruncate(pStore, iBefore);
				return false;
			}
			if ( Add == X509_VALUE ) {
				iAdded++;
			}
		}
	}
	if ( !bFound ) {
		__xrtX509StoreError(
			XERR_NOT_FOUND, "x509-store-add-pem",
			"PEM text contains no CERTIFICATE block", NULL
		);
		return false;
	}
	if ( pAdded != NULL ) {
		*pAdded = iAdded;
	}
	return true;
}



/* 借用指定索引的已解析证书。 */
XRT_API const xx509cert* xrtX509StoreCertificate(
	const xx509store* pStore,
	size_t iIndex
)
{
	if ( (pStore == NULL) || (iIndex >= pStore->Count) ) {
		if ( pStore == NULL ) {
			__xrtErrorSetInvalidArgument();
		} else {
			__xrtErrorSetRange();
		}
		return NULL;
	}
	return &pStore->Entries[iIndex].Certificate;
}



/* 借用指定索引的独立信任锚。 */
XRT_API const xx509anchor* xrtX509StoreAnchor(
	const xx509store* pStore,
	size_t iIndex
)
{
	if ( (pStore == NULL) || (iIndex >= pStore->Count) ) {
		if ( pStore == NULL ) {
			__xrtErrorSetInvalidArgument();
		} else {
			__xrtErrorSetRange();
		}
		return NULL;
	}
	return &pStore->Anchors[iIndex];
}



/* 组合外部候选与信任库锚，供自动建链直接使用。 */
XRT_API bool xrtX509StoreSource(
	const xx509store* pStore,
	const xx509cert* const* ppIssuers,
	size_t iIssuerCount,
	xx509pathsource* pSource
)
{
	xx509pathsource Source;

	if ( (pStore == NULL) || (pSource == NULL) ||
		((ppIssuers == NULL) && (iIssuerCount != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Source.Issuers = ppIssuers;
	Source.IssuerCount = iIssuerCount;
	Source.Anchors = pStore->Anchors;
	Source.AnchorCount = pStore->Count;
	*pSource = Source;
	return true;
}

#endif
