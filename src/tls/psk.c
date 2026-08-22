#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_PSK)

/* 设置 PSK 线路格式错误并返回 false。 */
static bool __xrtTlsPskError(cstr sOperation, cstr sMessage, size_t iOffset)
{
	__xrtTlsError(
		XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
		sOperation, sMessage, iOffset
	);
	return false;
}



/* 验证 PSK 模式向量完整、非空且没有重复线路值。 */
XRT_API bool xrtTlsPskModes(xbytesview Data, xbytesview* pModes)
{
	uint64 Seen[4] = { 0, 0, 0, 0 };
	xbytesview Modes;

	if ( !__xrtTlsViewValid(Data) || (pModes == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"parse-psk-modes", "TLS PSK modes input or output is invalid",
			SIZE_MAX
		);
		return false;
	}
	if ( (Data.Size < 2u) || (Data.Data[0] != Data.Size - 1u) ) {
		return __xrtTlsPskError(
			"parse-psk-modes", "TLS PSK modes vector length is invalid", 0
		);
	}
	Modes.Data = Data.Data + 1u;
	Modes.Size = Data.Size - 1u;
	for ( size_t i = 0; i < Modes.Size; i++ ) {
		uint8 iMode = Modes.Data[i];
		uint64 iBit = UINT64_C(1) << (iMode & 63u);

		if ( (Seen[iMode >> 6u] & iBit) != 0 ) {
			return __xrtTlsPskError(
				"parse-psk-modes", "TLS PSK mode appears more than once", i + 1u
			);
		}
		Seen[iMode >> 6u] |= iBit;
	}
	*pModes = Modes;
	return true;
}



/* 验证 identities 向量并返回条目数量。 */
static bool __xrtTlsPskIdentities(xbytesview Data, size_t* pCount)
{
	size_t iOffset = 0;
	size_t iCount = 0;

	while ( iOffset < Data.Size ) {
		size_t iIdentity;

		if ( (Data.Size - iOffset) < 6u ) {
			return __xrtTlsPskError(
				"parse-client-psks", "TLS PSK identity is truncated", iOffset
			);
		}
		iIdentity = __xrtTlsRead16(Data.Data + iOffset);
		if ( (iIdentity == 0) ||
			(iIdentity > Data.Size - iOffset - 6u) ) {
			return __xrtTlsPskError(
				"parse-client-psks", "TLS PSK identity length is invalid", iOffset
			);
		}
		iOffset += 2u + iIdentity + 4u;
		iCount++;
	}
	*pCount = iCount;
	return iCount != 0;
}



/* 验证 binders 向量并返回条目数量。 */
static bool __xrtTlsPskBinders(xbytesview Data, size_t* pCount)
{
	size_t iOffset = 0;
	size_t iCount = 0;

	while ( iOffset < Data.Size ) {
		size_t iBinder = Data.Data[iOffset];

		if ( (iBinder < 32u) ||
			(iBinder > Data.Size - iOffset - 1u) ) {
			return __xrtTlsPskError(
				"parse-client-psks", "TLS PSK binder length is invalid", iOffset
			);
		}
		iOffset += 1u + iBinder;
		iCount++;
	}
	*pCount = iCount;
	return iCount != 0;
}



/* 严格拆分并验证 OfferedPsks 的两个等量向量。 */
XRT_API bool xrtTlsClientPsks(
	xbytesview Data,
	xtlspskcursor* pCursor
)
{
	xtlspskcursor Cursor;
	size_t iIdentities;
	size_t iBinders;
	size_t iIdentityCount;
	size_t iBinderCount;

	if ( !__xrtTlsViewValid(Data) || (pCursor == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"parse-client-psks", "TLS PSK input or cursor is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Data.Size < 2u ) {
		return __xrtTlsPskError(
			"parse-client-psks", "TLS PSK identities length is missing", 0
		);
	}
	iIdentities = __xrtTlsRead16(Data.Data);
	if ( (iIdentities < 7u) || (iIdentities > Data.Size - 2u) ||
		((Data.Size - 2u - iIdentities) < 2u) ) {
		return __xrtTlsPskError(
			"parse-client-psks", "TLS PSK identities vector is invalid", 0
		);
	}
	Cursor.Identities = (xbytesview) { Data.Data + 2u, iIdentities };
	iBinders = __xrtTlsRead16(Data.Data + 2u + iIdentities);
	if ( (iBinders < 33u) ||
		(iBinders != Data.Size - 4u - iIdentities) ) {
		return __xrtTlsPskError(
			"parse-client-psks", "TLS PSK binders vector is invalid",
			2u + iIdentities
		);
	}
	Cursor.Binders = (xbytesview) {
		Data.Data + 4u + iIdentities, iBinders
	};
	if ( !__xrtTlsPskIdentities(Cursor.Identities, &iIdentityCount) ||
		!__xrtTlsPskBinders(Cursor.Binders, &iBinderCount) ) {
		return false;
	}
	if ( iIdentityCount != iBinderCount ) {
		return __xrtTlsPskError(
			"parse-client-psks", "TLS PSK identity and binder counts differ",
			2u + iIdentities
		);
	}
	Cursor.IdentityOffset = 0;
	Cursor.BinderOffset = 0;
	*pCursor = Cursor;
	return true;
}



/* 同步读取一项已经在初始化阶段完成边界校验的 PSK。 */
XRT_API xtlsitemresult xrtTlsPsksRead(
	xtlspskcursor* pCursor,
	xtlspsk* pPsk
)
{
	xtlspsk Psk;
	size_t iIdentity;
	size_t iBinder;

	if ( (pCursor == NULL) || (pPsk == NULL) ||
		!__xrtTlsViewValid(
			pCursor != NULL ? pCursor->Identities : (xbytesview) { NULL, 1u }
		) || !__xrtTlsViewValid(
			pCursor != NULL ? pCursor->Binders : (xbytesview) { NULL, 1u }
		) || ((pCursor != NULL) &&
			((pCursor->IdentityOffset > pCursor->Identities.Size) ||
			 (pCursor->BinderOffset > pCursor->Binders.Size))) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"read-client-psk", "TLS PSK cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->IdentityOffset == pCursor->Identities.Size ) {
		if ( pCursor->BinderOffset == pCursor->Binders.Size ) {
			return XTLS_ITEM_DONE;
		}
		(void)__xrtTlsPskError(
			"read-client-psk", "TLS PSK cursor vectors ended unevenly",
			pCursor->IdentityOffset
		);
		return XTLS_ITEM_ERROR;
	}
	if ( (pCursor->BinderOffset == pCursor->Binders.Size) ||
		((pCursor->Identities.Size - pCursor->IdentityOffset) < 6u) ||
		((pCursor->Binders.Size - pCursor->BinderOffset) < 1u) ) {
		(void)__xrtTlsPskError(
			"read-client-psk", "TLS PSK cursor is truncated",
			pCursor->IdentityOffset
		);
		return XTLS_ITEM_ERROR;
	}
	iIdentity = __xrtTlsRead16(
		pCursor->Identities.Data + pCursor->IdentityOffset
	);
	iBinder = pCursor->Binders.Data[pCursor->BinderOffset];
	if ( (iIdentity == 0) ||
		(iIdentity > pCursor->Identities.Size -
			pCursor->IdentityOffset - 6u) ||
		(iBinder < 32u) ||
		(iBinder > pCursor->Binders.Size -
			pCursor->BinderOffset - 1u) ) {
		(void)__xrtTlsPskError(
			"read-client-psk", "TLS PSK cursor entry is invalid",
			pCursor->IdentityOffset
		);
		return XTLS_ITEM_ERROR;
	}
	Psk.Identity = (xbytesview) {
		pCursor->Identities.Data + pCursor->IdentityOffset + 2u,
		iIdentity
	};
	Psk.ObfuscatedAge = __xrtTlsRead32(
		Psk.Identity.Data + Psk.Identity.Size
	);
	Psk.Binder = (xbytesview) {
		pCursor->Binders.Data + pCursor->BinderOffset + 1u,
		iBinder
	};
	pCursor->IdentityOffset += 2u + iIdentity + 4u;
	pCursor->BinderOffset += 1u + iBinder;
	*pPsk = Psk;
	return XTLS_ITEM_VALUE;
}



/* ServerHello 的 PSK 选择必须恰好占用一个 16 位索引。 */
XRT_API bool xrtTlsServerPsk(xbytesview Data, uint16* pSelected)
{
	if ( !__xrtTlsViewValid(Data) || (pSelected == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"parse-server-psk", "TLS server PSK input or output is invalid",
			SIZE_MAX
		);
		return false;
	}
	if ( Data.Size != 2u ) {
		return __xrtTlsPskError(
			"parse-server-psk", "TLS server PSK selection length is invalid", 0
		);
	}
	*pSelected = __xrtTlsRead16(Data.Data);
	return true;
}

#endif
