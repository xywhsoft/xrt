#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_RESUME)

/* 恢复对象使用一块连续内存持有结构和全部可变长度字段。 */
struct xtlsresume {
	volatile int32 RefCount;
	xtlsresumeinfo Info;
	size_t AllocationSize;
};



/* 设置恢复模块的结构化错误。 */
static bool __xrtTlsResumeError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, SIZE_MAX);
	return false;
}



/* 判断借用字节视图是否具有一致的空值语义。 */
static bool __xrtTlsResumeBytesValid(xbytesview Value)
{
	return (Value.Data != NULL) || (Value.Size == 0);
}



/* 判断借用字符串视图是否具有一致的空值语义。 */
static bool __xrtTlsResumeStringValid(xstrview Value)
{
	return (Value.Data != NULL) || (Value.Size == 0);
}



/* 安全累加单块分配大小。 */
static bool __xrtTlsResumeAdd(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 验证配置并计算精确分配大小和过期时刻。 */
static bool __xrtTlsResumeConfigValid(
	const xtlsresumeconfig* pConfig,
	size_t* pSize,
	xtime* pExpiresAt
)
{
	const xtlscipherinfo* pCipher;
	int64 iLifetime;
	size_t iSize = sizeof(xtlsresume);

	if ( (pConfig == NULL) || (pSize == NULL) || (pExpiresAt == NULL) ) {
		return __xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"create-tls-resume", "TLS resume config or output is null"
		);
	}
	if ( !__xrtTlsResumeBytesValid(pConfig->Ticket) ||
		!__xrtTlsResumeBytesValid(pConfig->Secret) ||
		!__xrtTlsResumeStringValid(pConfig->ServerName) ||
		!__xrtTlsResumeBytesValid(pConfig->Protocol) ||
		!__xrtTlsResumeBytesValid(pConfig->PeerIdentity) ) {
		return __xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"create-tls-resume", "TLS resume contains an invalid borrowed view"
		);
	}
	if ( pConfig->Version != XTLS_VERSION_13 ) {
		return __xrtTlsResumeError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION,
			"create-tls-resume", "TLS resume currently requires TLS 1.3"
		);
	}
	pCipher = xrtTlsCipherInfo(pConfig->Cipher);
	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_13) ) {
		return __xrtTlsResumeError(
			XERR_VALUE, XTLS_ERROR_CIPHER,
			"create-tls-resume", "TLS resume cipher is not a TLS 1.3 suite"
		);
	}
	if ( (pConfig->Ticket.Size == 0) ||
		(pConfig->Ticket.Size > UINT16_MAX) ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume ticket length is invalid"
		);
	}
	if ( pConfig->Secret.Size != pCipher->HashSize ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume PSK length does not match its cipher"
		);
	}
	if ( (pConfig->Lifetime == 0) ||
		(pConfig->Lifetime > XTLS13_TICKET_LIFETIME_MAX) ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume lifetime is outside the TLS 1.3 range"
		);
	}
	if ( pConfig->ServerName.Size > (XTLS_EXTENSION_DATA_MAX - 5u) ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_EXTENSION,
			"create-tls-resume", "TLS resume server name is too long"
		);
	}
	for ( size_t i = 0; i < pConfig->ServerName.Size; i++ ) {
		if ( pConfig->ServerName.Data[i] == '\0' ) {
			return __xrtTlsResumeError(
				XERR_VALUE, XTLS_ERROR_EXTENSION,
				"create-tls-resume", "TLS resume server name contains a null byte"
			);
		}
	}
	if ( pConfig->Protocol.Size > UINT8_MAX ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_EXTENSION,
			"create-tls-resume", "TLS resume ALPN protocol is too long"
		);
	}
	if ( pConfig->PeerIdentity.Size > UINT16_MAX ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume peer identity is too long"
		);
	}

	iLifetime = (int64)pConfig->Lifetime * INT64_C(1000000);
	if ( pConfig->IssuedAt > (INT64_MAX - iLifetime) ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume expiration time overflows"
		);
	}
	if ( !__xrtTlsResumeAdd(&iSize, pConfig->Ticket.Size) ||
		!__xrtTlsResumeAdd(&iSize, pConfig->Secret.Size) ||
		!__xrtTlsResumeAdd(&iSize, pConfig->ServerName.Size) ||
		!__xrtTlsResumeAdd(&iSize, pConfig->Protocol.Size) ||
		!__xrtTlsResumeAdd(&iSize, pConfig->PeerIdentity.Size) ) {
		return __xrtTlsResumeError(
			XERR_RANGE, XTLS_ERROR_LIMIT,
			"create-tls-resume", "TLS resume allocation size overflows"
		);
	}
	*pSize = iSize;
	*pExpiresAt = pConfig->IssuedAt + iLifetime;
	return true;
}



/* 把一个借用字节视图复制到对象尾部并推进写指针。 */
static xbytesview __xrtTlsResumeCopyBytes(
	bytes* pStorage,
	xbytesview Value
)
{
	xbytesview Result = { *pStorage, Value.Size };

	if ( Value.Size != 0 ) {
		memcpy(*pStorage, Value.Data, Value.Size);
		*pStorage += Value.Size;
	}
	return Result;
}



/* 把一个借用字符串视图复制到对象尾部并推进写指针。 */
static xstrview __xrtTlsResumeCopyString(
	bytes* pStorage,
	xstrview Value
)
{
	xstrview Result = { (cstr)*pStorage, Value.Size };

	if ( Value.Size != 0 ) {
		memcpy(*pStorage, Value.Data, Value.Size);
		*pStorage += Value.Size;
	}
	return Result;
}



/* 初始化默认 TLS 1.3 恢复配置。 */
XRT_API void xrtTlsResumeConfigInit(xtlsresumeconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-tls-resume-config", "TLS resume config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Version = XTLS_VERSION_13;
	pConfig->IssuedAt = xrtNow();
}



/* 创建持有全部恢复材料的一块连续内存。 */
XRT_API xtlsresume* xrtTlsResumeCreate(
	const xtlsresumeconfig* pConfig
)
{
	xtlsresume* pResume;
	bytes pStorage;
	size_t iSize;
	xtime iExpiresAt;

	if ( !__xrtTlsResumeConfigValid(
		pConfig, &iSize, &iExpiresAt
	) ) {
		return NULL;
	}
	pResume = (xtlsresume*)xrtMalloc(iSize);
	if ( pResume == NULL ) {
		const xerror* pCause = xrtGetError();

		__xrtTlsErrorCause(
			XERR_MEMORY, XTLS_ERROR_RESUME,
			"create-tls-resume", "TLS resume allocation failed",
			SIZE_MAX, pCause
		);
		return NULL;
	}
	memset(pResume, 0, sizeof(*pResume));
	pResume->RefCount = 1;
	pResume->AllocationSize = iSize;
	pResume->Info.Version = pConfig->Version;
	pResume->Info.Cipher = pConfig->Cipher;
	pResume->Info.Lifetime = pConfig->Lifetime;
	pResume->Info.AgeAdd = pConfig->AgeAdd;
	pResume->Info.MaxEarlyData = pConfig->MaxEarlyData;
	pResume->Info.IssuedAt = pConfig->IssuedAt;
	pResume->Info.ExpiresAt = iExpiresAt;

	pStorage = (bytes)(pResume + 1);
	pResume->Info.Ticket = __xrtTlsResumeCopyBytes(
		&pStorage, pConfig->Ticket
	);
	pResume->Info.Secret = __xrtTlsResumeCopyBytes(
		&pStorage, pConfig->Secret
	);
	pResume->Info.ServerName = __xrtTlsResumeCopyString(
		&pStorage, pConfig->ServerName
	);
	pResume->Info.Protocol = __xrtTlsResumeCopyBytes(
		&pStorage, pConfig->Protocol
	);
	pResume->Info.PeerIdentity = __xrtTlsResumeCopyBytes(
		&pStorage, pConfig->PeerIdentity
	);
	return pResume;
}



/* 增加不可变恢复对象的共享引用。 */
XRT_API xtlsresume* xrtTlsResumeRetain(const xtlsresume* pResume)
{
	if ( pResume == NULL ) {
		(void)__xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"retain-tls-resume", "TLS resume is null"
		);
		return NULL;
	}
	if ( xrtRefRetain((volatile int32*)&pResume->RefCount) < 0 ) {
		(void)__xrtTlsResumeError(
			XERR_STATE, XTLS_ERROR_STATE,
			"retain-tls-resume", "TLS resume reference is invalid"
		);
		return NULL;
	}
	return (xtlsresume*)pResume;
}



/* 释放最后一个引用前清除包括 PSK 在内的整块对象内存。 */
XRT_API void xrtTlsResumeRelease(xtlsresume* pResume)
{
	size_t iSize;

	if ( (pResume == NULL) ||
		(xrtRefRelease(&pResume->RefCount) != 0) ) {
		return;
	}
	iSize = pResume->AllocationSize;
	xrtSecureZero(pResume, iSize);
	xrtFree(pResume);
}



/* 复制只读信息结构，内部视图仍借用恢复对象。 */
XRT_API bool xrtTlsResumeInfo(
	const xtlsresume* pResume,
	xtlsresumeinfo* pInfo
)
{
	if ( (pResume == NULL) || (pInfo == NULL) ) {
		return __xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-resume-info", "TLS resume or info output is null"
		);
	}
	*pInfo = pResume->Info;
	return true;
}



/* 票据只在签发时刻到过期时刻之间有效，时钟回退时安全失效。 */
XRT_API bool xrtTlsResumeValidAt(
	const xtlsresume* pResume,
	xtime iNow
)
{
	if ( pResume == NULL ) {
		return __xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"validate-tls-resume", "TLS resume is null"
		);
	}
	return (iNow >= pResume->Info.IssuedAt) &&
		(iNow < pResume->Info.ExpiresAt);
}



/* 先验证有效期，再按协议要求对毫秒年龄执行 32 位模加。 */
XRT_API bool xrtTlsResumeTicketAge(
	const xtlsresume* pResume,
	xtime iNow,
	uint32* pAge
)
{
	uint64 iMilliseconds;

	if ( (pResume == NULL) || (pAge == NULL) ) {
		return __xrtTlsResumeError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"get-tls-resume-ticket-age",
			"TLS resume or ticket age output is null"
		);
	}
	if ( !xrtTlsResumeValidAt(pResume, iNow) ) {
		return false;
	}
	iMilliseconds = (uint64)(iNow - pResume->Info.IssuedAt) / 1000u;
	*pAge = (uint32)iMilliseconds + pResume->Info.AgeAdd;
	return true;
}

#endif
