#include "../internal/xrt_http_server_mux.h"



#if defined(XRT_FEATURE_HTTP_SERVER_MUX)

/* 原因缺失或没有类别时使用调用点给出的稳定默认类别。 */
static xerrkind __xrtHttpServerMuxCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 设置包含稳定域、代码、操作和原因链的 Mux 错误。 */
void __xrtHttpServerMuxSetError(
	xerrkind Kind,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind != XERR_NONE ? Kind :
		__xrtHttpServerMuxCauseKind(pCause, XERR_INTERNAL);
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.http.server.mux";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把当前错误包装为稳定 Mux 错误。 */
void __xrtHttpServerMuxWrapError(
	xerrkind Default,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtHttpServerMuxSetError(
		__xrtHttpServerMuxCauseKind(pCause, Default),
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 把一个 ASCII 字节折叠为 Host 比较使用的小写形式。 */
static uint8 __xrtHttpServerMuxFold(uint8 Value)
{
	return (Value >= (uint8)'A') && (Value <= (uint8)'Z') ?
		(uint8)(Value + ((uint8)'a' - (uint8)'A')) : Value;
}



/* 按 ASCII 不区分大小写顺序比较两个 Host。 */
static int __xrtHttpServerMuxCompare(
	xstrview Left,
	xstrview Right
)
{
	size_t iLimit = Left.Size < Right.Size ?
		Left.Size : Right.Size;
	size_t i;

	for ( i = 0; i < iLimit; i++ ) {
		uint8 A = __xrtHttpServerMuxFold(
			(uint8)Left.Data[i]
		);
		uint8 B = __xrtHttpServerMuxFold(
			(uint8)Right.Data[i]
		);

		if ( A != B ) {
			return A < B ? -1 : 1;
		}
	}
	return Left.Size == Right.Size ? 0 :
		(Left.Size < Right.Size ? -1 : 1);
}



/* 在有序 Host 表中返回命中位置或稳定插入位置。 */
static size_t __xrtHttpServerMuxSearch(
	const xhttpservermux* pMux,
	xstrview Host,
	bool* pFound
)
{
	size_t iLeft = 0;
	size_t iRight = pMux->Count;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		const xrt_http_server_mux_entry* pEntry =
			&pMux->Entries[iMiddle];
		int iOrder = __xrtHttpServerMuxCompare(
			Host,
			(xstrview){ pEntry->Host, pEntry->Size }
		);

		if ( iOrder == 0 ) {
			*pFound = true;
			return iMiddle;
		}
		if ( iOrder < 0 ) {
			iRight = iMiddle;
		} else {
			iLeft = iMiddle + 1u;
		}
	}
	*pFound = false;
	return iLeft;
}



/* 解析一个无端口 HTTP Host，并返回借用的规范 Host 部分。 */
static bool __xrtHttpServerMuxParseHost(
	xstrview Text,
	xstrview* pHost,
	cstr sOperation
)
{
	xurl Parsed;

	if ( (pHost == NULL) ||
		((Text.Data == NULL) && (Text.Size != 0)) ||
		!__xrtRangeValid(Text.Data, Text.Size) ||
		(Text.Size == 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			sOperation,
			"HTTP server mux Host is empty or invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpHostParse(Text, &Parsed) ||
		(Parsed.Host.Size == 0) ||
		((Parsed.Flags & XURL_HAS_PORT) != 0) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMuxSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_MUX_ERROR_HOST,
			sOperation,
			"HTTP server mux Host must be valid and contain no port",
			pCause
		);
		return false;
	}
	*pHost = Parsed.Host;
	return true;
}



/* 分配一份小写 Host 副本，避免热路径重复折叠存储文本。 */
static str __xrtHttpServerMuxHostCopy(xstrview Host)
{
	str sHost;
	size_t i;

	if ( Host.Size == SIZE_MAX ) {
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux Host size overflows storage",
			NULL
		);
		return NULL;
	}
	sHost = (str)xrtMalloc(Host.Size + 1u);
	if ( sHost == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"set-http-server-mux-host",
			"HTTP server mux Host allocation failed"
		);
		return NULL;
	}
	for ( i = 0; i < Host.Size; i++ ) {
		sHost[i] = (char)__xrtHttpServerMuxFold(
			(uint8)Host.Data[i]
		);
	}
	sHost[Host.Size] = '\0';
	return sHost;
}



/* 为一个新 Host 记录扩展有序表容量。 */
static bool __xrtHttpServerMuxReserve(xhttpservermux* pMux)
{
	size_t iRequired = pMux->Count + 1u;
	size_t iCapacity;
	xrt_http_server_mux_entry* pEntries;

	if ( iRequired <= pMux->Capacity ) {
		return true;
	}
	iCapacity = pMux->Capacity != 0 ?
		pMux->Capacity :
		(pMux->Config.MaxHosts < 8u ?
		 pMux->Config.MaxHosts : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity >
			(pMux->Config.MaxHosts / 2u) ?
			pMux->Config.MaxHosts : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	if ( iCapacity >
		(SIZE_MAX / sizeof(*pEntries)) ) {
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux table size overflows storage",
			NULL
		);
		return false;
	}
	pEntries = (xrt_http_server_mux_entry*)xrtRealloc(
		pMux->Entries,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"set-http-server-mux-host",
			"HTTP server mux table allocation failed"
		);
		return false;
	}
	pMux->Entries = pEntries;
	pMux->Capacity = iCapacity;
	return true;
}



/* 初始化有限但足以覆盖常见多租户服务的默认配置。 */
XRT_API void xrtHttpServerMuxConfigInit(
	xhttpservermuxconfig* pConfig
)
{
	const xhttpservermuxconfig Config = {
		256u,
		64u * 1024u
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"init-http-server-mux-config",
			"HTTP server mux config range is invalid",
			NULL
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建空 Mux 并初始化读写锁。 */
XRT_API xhttpservermux* xrtHttpServerMuxCreate(
	const xhttpservermuxconfig* pConfig
)
{
	xhttpservermuxconfig Config;
	xhttpservermux* pMux;

	xrtHttpServerMuxConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerMuxSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
				"create-http-server-mux",
				"HTTP server mux config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( (Config.MaxHosts == 0) ||
		(Config.MaxHostBytes == 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"create-http-server-mux",
			"HTTP server mux limits must be nonzero",
			NULL
		);
		return NULL;
	}
	pMux = (xhttpservermux*)xrtCalloc(1, sizeof(*pMux));
	if ( pMux == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"create-http-server-mux",
			"HTTP server mux allocation failed"
		);
		return NULL;
	}
	if ( !xrtRWLockInit(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"create-http-server-mux",
			"HTTP server mux rwlock initialization failed"
		);
		xrtFree(pMux);
		return NULL;
	}
	pMux->References = 1;
	pMux->Config = Config;
	return pMux;
}



/* 增加 Mux 引用并拒绝空或已经释放的对象。 */
XRT_API xhttpservermux* xrtHttpServerMuxRef(
	xhttpservermux* pMux
)
{
	if ( (pMux == NULL) ||
		(xrtRefRetain(&pMux->References) < 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"retain-http-server-mux",
			"HTTP server mux is null or released",
			NULL
		);
		return NULL;
	}
	return pMux;
}



/* 释放最后一个引用持有的 Host、Router 和同步原语。 */
XRT_API void xrtHttpServerMuxDestroy(
	xhttpservermux* pMux
)
{
	size_t i;

	if ( (pMux == NULL) ||
		(xrtRefRelease(&pMux->References) != 0) ) {
		return;
	}
	for ( i = 0; i < pMux->Count; i++ ) {
		xrtFree(pMux->Entries[i].Host);
		xrtHttpServerRouterDestroy(
			pMux->Entries[i].Router
		);
	}
	xrtHttpServerRouterDestroy(pMux->Default);
	xrtFree(pMux->Entries);
	(void)xrtRWLockUnit(&pMux->Lock);
	memset(pMux, 0, sizeof(*pMux));
	xrtFree(pMux);
}



/* 设置默认 Router，并把旧引用释放放到写锁之外。 */
XRT_API bool xrtHttpServerMuxDefault(
	xhttpservermux* pMux,
	xhttpserverrouter* pRouter
)
{
	xhttpserverrouter* pHeld;
	xhttpserverrouter* pOld;

	if ( (pMux == NULL) || (pRouter == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMuxSetError(
			(pRouter != NULL) ? XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) ?
				XHTTP_SERVER_MUX_ERROR_STATE :
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"set-http-server-mux-default",
			"HTTP server mux and frozen default Router are required",
			NULL
		);
		return false;
	}
	pHeld = xrtHttpServerRouterRef(pRouter);
	if ( pHeld == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_STATE,
			"set-http-server-mux-default",
			"HTTP server mux default Router could not be retained"
		);
		return false;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		xrtHttpServerRouterDestroy(pHeld);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"set-http-server-mux-default",
			"HTTP server mux write lock failed"
		);
		return false;
	}
	pOld = pMux->Default;
	pMux->Default = pHeld;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	xrtHttpServerRouterDestroy(pOld);
	return true;
}



/* 设置精确 Host，并在同名项上原子替换 Router。 */
XRT_API bool xrtHttpServerMuxHost(
	xhttpservermux* pMux,
	xstrview Text,
	xhttpserverrouter* pRouter
)
{
	xstrview Host;
	xhttpserverrouter* pHeld;
	xhttpserverrouter* pOld = NULL;
	str sHost;
	bool bFound;
	size_t iIndex;

	if ( (pMux == NULL) || (pRouter == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMuxSetError(
			(pRouter != NULL) ? XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) ?
				XHTTP_SERVER_MUX_ERROR_STATE :
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"set-http-server-mux-host",
			"HTTP server mux and frozen Host Router are required",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "set-http-server-mux-host"
	) ) {
		return false;
	}
	sHost = __xrtHttpServerMuxHostCopy(Host);
	if ( sHost == NULL ) {
		return false;
	}
	pHeld = xrtHttpServerRouterRef(pRouter);
	if ( pHeld == NULL ) {
		xrtFree(sHost);
		__xrtHttpServerMuxWrapError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_STATE,
			"set-http-server-mux-host",
			"HTTP server mux Host Router could not be retained"
		);
		return false;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"set-http-server-mux-host",
			"HTTP server mux write lock failed"
		);
		return false;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( bFound ) {
		pOld = pMux->Entries[iIndex].Router;
		pMux->Entries[iIndex].Router = pHeld;
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pOld);
		xrtFree(sHost);
		return true;
	}
	if ( (pMux->Count >= pMux->Config.MaxHosts) ||
		(Host.Size >
		 (pMux->Config.MaxHostBytes - pMux->HostBytes)) ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux Host limits were exceeded",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMuxReserve(pMux) ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		return false;
	}
	memmove(
		&pMux->Entries[iIndex + 1u],
		&pMux->Entries[iIndex],
		(pMux->Count - iIndex) * sizeof(*pMux->Entries)
	);
	pMux->Entries[iIndex].Host = sHost;
	pMux->Entries[iIndex].Size = Host.Size;
	pMux->Entries[iIndex].Router = pHeld;
	pMux->Count++;
	pMux->HostBytes += Host.Size;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	return true;
}



/* 移除精确 Host，并在锁外释放其文本和 Router。 */
XRT_API xhttpservermuxstatus xrtHttpServerMuxRemove(
	xhttpservermux* pMux,
	xstrview Text
)
{
	xrt_http_server_mux_entry Removed;
	xstrview Host;
	bool bFound;
	size_t iIndex;

	if ( pMux == NULL ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"remove-http-server-mux-host",
			"HTTP server mux is null",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "remove-http-server-mux-host"
	) ) {
		return XHTTP_SERVER_MUX_ERROR;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"remove-http-server-mux-host",
			"HTTP server mux write lock failed"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( !bFound ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		return XHTTP_SERVER_MUX_NOT_FOUND;
	}
	Removed = pMux->Entries[iIndex];
	memmove(
		&pMux->Entries[iIndex],
		&pMux->Entries[iIndex + 1u],
		(pMux->Count - iIndex - 1u) * sizeof(*pMux->Entries)
	);
	pMux->Count--;
	pMux->HostBytes -= Removed.Size;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	xrtFree(Removed.Host);
	xrtHttpServerRouterDestroy(Removed.Router);
	return XHTTP_SERVER_MUX_HOST;
}



/* 在读锁内选择并保留精确 Host 或默认 Router。 */
xhttpservermuxstatus __xrtHttpServerMuxSelect(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
)
{
	xhttpserverrouter* pRouter = NULL;
	xhttpservermuxstatus Status = XHTTP_SERVER_MUX_NOT_FOUND;
	bool bFound;
	size_t iIndex;

	if ( (pMux == NULL) ||
		!__xrtRangeValid(ppRouter, sizeof(pRouter)) ||
		!__xrtRangeValid(Host.Data, Host.Size) ||
		(Host.Size == 0) ||
		__xrtRangesOverlap(
			ppRouter, sizeof(pRouter),
			pMux, sizeof(*pMux)
		) || __xrtRangesOverlap(
			ppRouter, sizeof(pRouter),
			Host.Data, Host.Size
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"match-http-server-mux",
			"HTTP server mux match arguments are invalid",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy(ppRouter, &pRouter, sizeof(pRouter));
	if ( !xrtRWLockRead(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"match-http-server-mux",
			"HTTP server mux read lock failed"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( bFound ) {
		pRouter = pMux->Entries[iIndex].Router;
		Status = XHTTP_SERVER_MUX_HOST;
	} else if ( pMux->Default != NULL ) {
		pRouter = pMux->Default;
		Status = XHTTP_SERVER_MUX_DEFAULT;
	}
	if ( pRouter != NULL ) {
		pRouter = xrtHttpServerRouterRef(pRouter);
	}
	(void)xrtRWLockReadUnlock(&pMux->Lock);
	if ( (Status != XHTTP_SERVER_MUX_NOT_FOUND) &&
		(pRouter == NULL) ) {
		__xrtHttpServerMuxWrapError(
			XERR_INTERNAL,
			XHTTP_SERVER_MUX_ERROR_INTERNAL,
			"match-http-server-mux",
			"HTTP server mux selected Router could not be retained"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy(ppRouter, &pRouter, sizeof(pRouter));
	return Status;
}



/* 解析公开 Host 后执行无分配 Mux 查找。 */
XRT_API xhttpservermuxstatus xrtHttpServerMuxMatch(
	xhttpservermux* pMux,
	xstrview Text,
	xhttpserverrouter** ppRouter
)
{
	xstrview Host;
	xhttpserverrouter* pRouter = NULL;

	if ( (pMux == NULL) ||
		!__xrtRangeValid(ppRouter, sizeof(pRouter)) ||
		!__xrtRangeValid(Text.Data, Text.Size) ||
		__xrtRangesOverlap(
			ppRouter, sizeof(pRouter),
			pMux, sizeof(*pMux)
		) || __xrtRangesOverlap(
			ppRouter, sizeof(pRouter),
			Text.Data, Text.Size
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"match-http-server-mux",
			"HTTP server mux or output is null",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy(ppRouter, &pRouter, sizeof(pRouter));
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "match-http-server-mux"
	) ) {
		return XHTTP_SERVER_MUX_ERROR;
	}
	return __xrtHttpServerMuxSelect(
		pMux, Host, ppRouter
	);
}



/* 复制受读锁保护的一致配置统计。 */
XRT_API bool xrtHttpServerMuxStats(
	xhttpservermux* pMux,
	xhttpservermuxstats* pStats
)
{
	xhttpservermuxstats Stats = { 0 };

	if ( (pMux == NULL) ||
		!__xrtRangeValid(pStats, sizeof(Stats)) ||
		__xrtRangesOverlap(
			pStats, sizeof(Stats),
			pMux, sizeof(*pMux)
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"stat-http-server-mux",
			"HTTP server mux or stats output is null",
			NULL
		);
		return false;
	}
	if ( !xrtRWLockRead(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"stat-http-server-mux",
			"HTTP server mux read lock failed"
		);
		return false;
	}
	Stats.Hosts = pMux->Count;
	Stats.HostBytes = pMux->HostBytes;
	Stats.HasDefault = pMux->Default != NULL;
	(void)xrtRWLockReadUnlock(&pMux->Lock);
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
