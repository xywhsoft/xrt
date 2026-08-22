#include "../internal/xrt_cookie_jar.h"



#if defined(XRT_FEATURE_COOKIE_JAR)

/* 内部选择数组保存稳定条目指针，避免对 Jar 本体重新排列。 */
typedef struct xrt_cookie_selection {
	xrt_cookie_entry** Items;
	size_t Count;
	size_t Size;
} xrt_cookie_selection;



/* 验证通用借用视图。 */
static bool __xrtCookieJarViewValid(xstrview Text)
{
	return (Text.Data != NULL) || (Text.Size == 0);
}



/* 按字节比较两个视图。 */
static bool __xrtCookieJarTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 按 ASCII 大小写不敏感规则比较两个视图。 */
static bool __xrtCookieJarAsciiEqual(xstrview Left, xstrview Right)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xrtHttpAsciiLower((unsigned char)Left.Data[i]) !=
			__xrtHttpAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断名称是否带有大小写不敏感的安全前缀。 */
static bool __xrtCookieJarPrefix(xstrview Name, cstr sPrefix, size_t iSize)
{
	return (Name.Size >= iSize) && __xrtCookieJarAsciiEqual(
		(xstrview){ Name.Data, iSize }, (xstrview){ sPrefix, iSize }
	);
}



/* 验证不透明分区键不包含控制字节。 */
static bool __xrtCookieJarPartitionValid(xstrview Key)
{
	size_t i;

	if ( !__xrtCookieJarViewValid(Key) ) {
		return false;
	}
	for ( i = 0; i < Key.Size; i++ ) {
		unsigned char iByte = (unsigned char)Key.Data[i];

		if ( (iByte < 0x20u) || (iByte == 0x7Fu) ) {
			return false;
		}
	}
	return true;
}



/* 判断 Set 的 pair 与宽松接收解析器产物一致。 */
static bool __xrtCookieJarPairValid(const xsetcookie* pCookie)
{
	size_t i;

	if ( (pCookie->Name.Size > XSET_COOKIE_MAX_PAIR_BYTES) ||
		(pCookie->Value.Size >
		 (XSET_COOKIE_MAX_PAIR_BYTES - pCookie->Name.Size)) ) {
		return false;
	}
	if ( ((pCookie->Name.Size != 0) &&
		 ((pCookie->Name.Data[0] == ' ') ||
		  (pCookie->Name.Data[0] == '\t') ||
		  (pCookie->Name.Data[pCookie->Name.Size - 1u] == ' ') ||
		  (pCookie->Name.Data[pCookie->Name.Size - 1u] == '\t'))) ||
		((pCookie->Value.Size != 0) &&
		 ((pCookie->Value.Data[0] == ' ') ||
		  (pCookie->Value.Data[0] == '\t') ||
		  (pCookie->Value.Data[pCookie->Value.Size - 1u] == ' ') ||
		  (pCookie->Value.Data[pCookie->Value.Size - 1u] == '\t'))) ) {
		return false;
	}
	for ( i = 0; i < pCookie->Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)pCookie->Name.Data[i];

		if ( (iByte <= 0x08u) ||
			((iByte >= 0x0Au) && (iByte <= 0x1Fu)) ||
			(iByte == 0x7Fu) || (iByte == ';') || (iByte == '=') ) {
			return false;
		}
	}
	for ( i = 0; i < pCookie->Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)pCookie->Value.Data[i];

		if ( (iByte <= 0x08u) ||
			((iByte >= 0x0Au) && (iByte <= 0x1Fu)) ||
			(iByte == 0x7Fu) || (iByte == ';') ) {
			return false;
		}
	}
	return true;
}



/* 验证 DNS 标签形式的 ASCII 主机名或 Domain 属性。 */
static bool __xrtCookieJarDomainValid(xstrview Domain)
{
	size_t i = 0;

	if ( (Domain.Size == 0) || (Domain.Size > 253u) ) {
		return false;
	}
	while ( i < Domain.Size ) {
		size_t iBegin = i;

		while ( (i < Domain.Size) && (Domain.Data[i] != '.') ) {
			unsigned char iByte = (unsigned char)Domain.Data[i];

			if ( !((iByte >= (unsigned char)'A' &&
				iByte <= (unsigned char)'Z') ||
				(iByte >= (unsigned char)'a' &&
				 iByte <= (unsigned char)'z') ||
				(iByte >= (unsigned char)'0' &&
				 iByte <= (unsigned char)'9') ||
				(iByte == (unsigned char)'-')) ) {
				return false;
			}
			i++;
		}
		if ( (i == iBegin) || ((i - iBegin) > 63u) ||
			(Domain.Data[iBegin] == '-') ||
			(Domain.Data[i - 1u] == '-') ) {
			return false;
		}
		if ( i < Domain.Size ) {
			i++;
			if ( i == Domain.Size ) {
				return false;
			}
		}
	}
	return true;
}



/* 严格识别规范十进制 IPv4 地址。 */
static bool __xrtCookieJarIpv4(xstrview Host)
{
	size_t i = 0;
	size_t iPart;

	for ( iPart = 0; iPart < 4; iPart++ ) {
		size_t iBegin = i;
		uint32 iValue = 0;

		while ( (i < Host.Size) &&
			(Host.Data[i] >= '0') && (Host.Data[i] <= '9') ) {
			iValue = (iValue * 10u) +
				(uint32)(Host.Data[i] - '0');
			i++;
		}
		if ( (i == iBegin) || ((i - iBegin) > 3u) ||
			(iValue > 255u) ||
			(((i - iBegin) > 1u) && (Host.Data[iBegin] == '0')) ) {
			return false;
		}
		if ( iPart == 3u ) {
			return i == Host.Size;
		}
		if ( (i >= Host.Size) || (Host.Data[i] != '.') ) {
			return false;
		}
		i++;
	}
	return false;
}



/* 规范化 HTTP Cookie 使用的 URL 主机名。 */
bool __xrtCookieOriginParse(xstrview URL, xrt_cookie_origin* pOrigin)
{
	xurl Parsed;
	xstrview Host;
	size_t i;

	if ( (pOrigin == NULL) || !__xrtCookieJarViewValid(URL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pOrigin, 0, sizeof(*pOrigin));
	if ( !xrtUrlParse(URL, &Parsed) ||
		((Parsed.Flags & (XURL_HAS_SCHEME | XURL_HAS_AUTHORITY |
		 XURL_HAS_HOST)) != (XURL_HAS_SCHEME | XURL_HAS_AUTHORITY |
		 XURL_HAS_HOST)) || (Parsed.Host.Size == 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("https")) ||
		xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("wss")) ) {
		pOrigin->Secure = true;
	} else if ( !xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("http")) &&
		!xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("ws")) ) {
		__xrtErrorSetValue();
		return false;
	}
	Host = Parsed.Host;
	while ( (Host.Size != 0) && (Host.Data[Host.Size - 1u] == '.') ) {
		Host.Size--;
	}
	if ( Host.Size == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( (Parsed.Flags & XURL_HOST_IP_LITERAL) == 0 ) {
		if ( !__xrtCookieJarDomainValid(Host) ) {
			__xrtErrorSetValue();
			return false;
		}
		pOrigin->IpHost = __xrtCookieJarIpv4(Host);
	} else {
		pOrigin->IpHost = true;
	}
	pOrigin->Host = (str)xrtMalloc(Host.Size + 1u);
	if ( pOrigin->Host == NULL ) {
		return false;
	}
	for ( i = 0; i < Host.Size; i++ ) {
		pOrigin->Host[i] = (char)__xrtHttpAsciiLower(
			(unsigned char)Host.Data[i]
		);
	}
	pOrigin->Host[Host.Size] = '\0';
	pOrigin->HostSize = Host.Size;
	pOrigin->Path = (Parsed.Path.Size == 0) ?
		XRT_STR_LITERAL("/") : Parsed.Path;
	return true;
}



/* 释放规范 URL 拥有的主机副本。 */
void __xrtCookieOriginClear(xrt_cookie_origin* pOrigin)
{
	if ( pOrigin == NULL ) {
		return;
	}
	xrtFree(pOrigin->Host);
	memset(pOrigin, 0, sizeof(*pOrigin));
}



/* 判断规范 Host 是否与规范 Domain 相同或位于其子域。 */
static bool __xrtCookieJarDomainMatch(xstrview Host, xstrview Domain)
{
	if ( __xrtCookieJarAsciiEqual(Host, Domain) ) {
		return true;
	}
	return (Host.Size > Domain.Size) &&
		(Host.Data[Host.Size - Domain.Size - 1u] == '.') &&
		__xrtCookieJarAsciiEqual(
			(xstrview){ Host.Data + Host.Size - Domain.Size,
				Domain.Size }, Domain
		);
}



/* 按 RFC 10025 的目录边界规则匹配请求路径。 */
static bool __xrtCookieJarPathMatch(xstrview Request, xstrview Cookie)
{
	if ( (Request.Size < Cookie.Size) ||
		((Cookie.Size != 0) &&
		 (memcmp(Request.Data, Cookie.Data, Cookie.Size) != 0)) ) {
		return false;
	}
	if ( (Request.Size == Cookie.Size) ||
		(Cookie.Data[Cookie.Size - 1u] == '/') ) {
		return true;
	}
	return Request.Data[Cookie.Size] == '/';
}



/* 从请求路径借用 RFC 默认 Cookie 路径。 */
static xstrview __xrtCookieJarDefaultPath(xstrview Path)
{
	size_t i;
	size_t iLast = 0;

	if ( (Path.Size == 0) || (Path.Data[0] != '/') ) {
		return XRT_STR_LITERAL("/");
	}
	for ( i = 1; i < Path.Size; i++ ) {
		if ( Path.Data[i] == '/' ) {
			iLast = i;
		}
	}
	return (iLast == 0) ? XRT_STR_LITERAL("/") :
		(xstrview){ Path.Data, iLast };
}



/* 释放单分配块 Cookie 条目。 */
static void __xrtCookieJarEntryClear(xrt_cookie_entry* pCookie)
{
	if ( pCookie == NULL ) {
		return;
	}
	xrtFree(pCookie->Storage);
	memset(pCookie, 0, sizeof(*pCookie));
}



/* 删除指定数组条目并保持其余条目相对顺序。 */
static void __xrtCookieJarRemoveAt(xcookiejar* pJar, size_t iIndex)
{
	__xrtCookieJarEntryClear(&pJar->Cookies[iIndex]);
	if ( (iIndex + 1u) < pJar->Count ) {
		memmove(
			&pJar->Cookies[iIndex], &pJar->Cookies[iIndex + 1u],
			(pJar->Count - iIndex - 1u) * sizeof(*pJar->Cookies)
		);
	}
	pJar->Count--;
	memset(&pJar->Cookies[pJar->Count], 0, sizeof(*pJar->Cookies));
}



/* 清理锁内全部过期条目。 */
static size_t __xrtCookieJarPurgeLocked(xcookiejar* pJar, xtime iNow)
{
	size_t i = 0;
	size_t iRemoved = 0;

	while ( i < pJar->Count ) {
		xrt_cookie_entry* pCookie = &pJar->Cookies[i];

		if ( ((pCookie->Flags & XCOOKIE_INFO_PERSISTENT) != 0) &&
			(pCookie->Expires <= iNow) ) {
			__xrtCookieJarRemoveAt(pJar, i);
			iRemoved++;
		} else {
			i++;
		}
	}
	return iRemoved;
}



/* 事务式扩充条目数组。 */
static bool __xrtCookieJarReserve(xcookiejar* pJar, size_t iNeed)
{
	xrt_cookie_entry* pNew;
	size_t iCapacity;

	if ( iNeed <= pJar->Capacity ) {
		return true;
	}
	iCapacity = (pJar->Capacity == 0) ?
		pJar->Config.InitialCookies : pJar->Capacity;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > pJar->Config.MaxCookies ) {
		iCapacity = pJar->Config.MaxCookies;
	}
	if ( (iCapacity < iNeed) ||
		(iCapacity > (SIZE_MAX / sizeof(*pNew))) ) {
		__xrtErrorSetRange();
		return false;
	}
	pNew = (xrt_cookie_entry*)xrtMalloc(iCapacity * sizeof(*pNew));
	if ( pNew == NULL ) {
		return false;
	}
	memset(pNew, 0, iCapacity * sizeof(*pNew));
	if ( pJar->Count != 0 ) {
		memcpy(pNew, pJar->Cookies,
			pJar->Count * sizeof(*pNew));
	}
	xrtFree(pJar->Cookies);
	pJar->Cookies = pNew;
	pJar->Capacity = iCapacity;
	return true;
}



/* 返回条目视图，集中维护内部拥有文本的长度。 */
static xstrview __xrtCookieJarEntryView(cstr sText, size_t iSize)
{
	return (xstrview){ sText, iSize };
}



/* 判断两个条目是否使用相同的存储主键。 */
static bool __xrtCookieJarSameKey(
	const xrt_cookie_entry* pLeft,
	const xrt_cookie_entry* pRight
)
{
	return (((pLeft->Flags ^ pRight->Flags) &
		XCOOKIE_INFO_HOST_ONLY) == 0) &&
		__xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Name, pLeft->NameSize),
		__xrtCookieJarEntryView(pRight->Name, pRight->NameSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Domain, pLeft->DomainSize),
		__xrtCookieJarEntryView(pRight->Domain, pRight->DomainSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Path, pLeft->PathSize),
		__xrtCookieJarEntryView(pRight->Path, pRight->PathSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(
			pLeft->PartitionKey, pLeft->PartitionKeySize
		),
		__xrtCookieJarEntryView(
			pRight->PartitionKey, pRight->PartitionKeySize
		)
	);
}



/* 在锁内查找相同名称、域、HostOnly、路径和分区键的条目。 */
static size_t __xrtCookieJarFindLocked(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pNeedle
)
{
	size_t i;

	for ( i = 0; i < pJar->Count; i++ ) {
		if ( __xrtCookieJarSameKey(&pJar->Cookies[i], pNeedle) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* Priority 未指定时按 Medium 参与容量淘汰。 */
static int __xrtCookieJarPriority(const xrt_cookie_entry* pCookie)
{
	if ( pCookie->Priority == XCOOKIE_PRIORITY_LOW ) {
		return 1;
	}
	if ( pCookie->Priority == XCOOKIE_PRIORITY_HIGH ) {
		return 3;
	}
	return 2;
}



/* 淘汰最低优先级、最久未访问的一个候选条目。 */
static bool __xrtCookieJarEvictLocked(
	xcookiejar* pJar,
	xstrview Domain,
	bool bFilter
)
{
	size_t i;
	size_t iCandidate = XRT_NPOS;

	for ( i = 0; i < pJar->Count; i++ ) {
		xrt_cookie_entry* pCookie = &pJar->Cookies[i];

		if ( bFilter && !__xrtCookieJarTextEqual(
			__xrtCookieJarEntryView(pCookie->Domain, pCookie->DomainSize),
			Domain
		) ) {
			continue;
		}
		if ( (iCandidate == XRT_NPOS) ||
			(__xrtCookieJarPriority(pCookie) <
			 __xrtCookieJarPriority(&pJar->Cookies[iCandidate])) ||
			((__xrtCookieJarPriority(pCookie) ==
			  __xrtCookieJarPriority(&pJar->Cookies[iCandidate])) &&
			 (pCookie->Accessed < pJar->Cookies[iCandidate].Accessed)) ||
			((pCookie->Accessed == pJar->Cookies[iCandidate].Accessed) &&
			 (pCookie->CreationOrder <
			  pJar->Cookies[iCandidate].CreationOrder)) ) {
			iCandidate = i;
		}
	}
	if ( iCandidate == XRT_NPOS ) {
		return false;
	}
	__xrtCookieJarRemoveAt(pJar, iCandidate);
	return true;
}



/* 复制 Cookie 的全部文本到一个紧凑拥有块。 */
static bool __xrtCookieJarEntryCopy(
	xrt_cookie_entry* pEntry,
	xstrview Name,
	xstrview Value,
	xstrview Domain,
	xstrview Path,
	xstrview PartitionKey
)
{
	xstrview Parts[5];
	str* Outputs[5];
	size_t* Sizes[5];
	size_t i;
	size_t iTotal = 0;
	str sCurrent;

	Parts[0] = Name;
	Parts[1] = Value;
	Parts[2] = Domain;
	Parts[3] = Path;
	Parts[4] = PartitionKey;
	Outputs[0] = &pEntry->Name;
	Outputs[1] = &pEntry->Value;
	Outputs[2] = &pEntry->Domain;
	Outputs[3] = &pEntry->Path;
	Outputs[4] = &pEntry->PartitionKey;
	Sizes[0] = &pEntry->NameSize;
	Sizes[1] = &pEntry->ValueSize;
	Sizes[2] = &pEntry->DomainSize;
	Sizes[3] = &pEntry->PathSize;
	Sizes[4] = &pEntry->PartitionKeySize;
	for ( i = 0; i < 5u; i++ ) {
		if ( (Parts[i].Size == SIZE_MAX) ||
			(iTotal > (SIZE_MAX - Parts[i].Size - 1u)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iTotal += Parts[i].Size + 1u;
	}
	pEntry->Storage = (str)xrtMalloc(iTotal);
	if ( pEntry->Storage == NULL ) {
		return false;
	}
	sCurrent = pEntry->Storage;
	for ( i = 0; i < 5u; i++ ) {
		size_t j;

		*Outputs[i] = sCurrent;
		*Sizes[i] = Parts[i].Size;
		for ( j = 0; j < Parts[i].Size; j++ ) {
			sCurrent[j] = (i == 2u) ?
				(char)__xrtHttpAsciiLower(
					(unsigned char)Parts[i].Data[j]
				) : Parts[i].Data[j];
		}
		sCurrent[Parts[i].Size] = '\0';
		sCurrent += Parts[i].Size + 1u;
	}
	return true;
}



/* 安全计算当前时间加固定时长，并钳制到 xtime 上界。 */
static xtime __xrtCookieJarTimeAdd(xtime iNow, xtime iDuration)
{
	if ( iNow > (INT64_MAX - iDuration) ) {
		return INT64_MAX;
	}
	return iNow + iDuration;
}



/* 返回配置允许的最晚持久化时间。 */
static xtime __xrtCookieJarLatestExpiry(xtime iNow)
{
	return __xrtCookieJarTimeAdd(iNow, XCOOKIE_JAR_MAX_LIFETIME);
}



/* 把策略拒绝结果统一写入可选输出。 */
static xcookiestorestatus __xrtCookieJarReject(
	xcookiereject Reason,
	xcookiereject* pReject
)
{
	if ( pReject != NULL ) {
		*pReject = Reason;
	}
	return XCOOKIE_STORE_REJECTED;
}



/* 检查配置中的大小关系和未知标志。 */
static bool __xrtCookieJarConfigValid(const xcookiejarconfig* pConfig)
{
	return ((pConfig->Flags & ~XCOOKIE_JAR_ALLOW_UNVERIFIED_DOMAIN) == 0) &&
		(pConfig->InitialCookies != 0) &&
		(pConfig->MaxCookies != 0) &&
		(pConfig->InitialCookies <= pConfig->MaxCookies) &&
		(pConfig->MaxCookiesPerDomain != 0) &&
		(pConfig->MaxCookiesPerDomain <= pConfig->MaxCookies) &&
		(pConfig->MaxCookieBytes != 0) &&
		(pConfig->MaxNameBytes != 0) &&
		(pConfig->MaxValueBytes != 0) &&
		(pConfig->MaxDomainBytes != 0) &&
		(pConfig->MaxPathBytes != 0) &&
		(pConfig->MaxPartitionKeyBytes != 0) &&
		(pConfig->LaxUnsafeAge >= 0);
}



/* 初始化通用客户端配置。 */
XRT_API void xrtCookieJarConfigInit(xcookiejarconfig* pConfig)
{
	xcookiejarconfig Config;

	if ( pConfig == NULL ) {
		return;
	}
	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.InitialCookies = 16;
	Config.MaxCookies = 3000;
	Config.MaxCookiesPerDomain = 180;
	Config.MaxCookieBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxNameBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxValueBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxDomainBytes = 253;
	Config.MaxPathBytes = 1024;
	Config.MaxPartitionKeyBytes = 1024;
	Config.LaxUnsafeAge = 2 * XRT_TIME_MINUTE;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建 CookieJar 并初始化系统互斥锁。 */
XRT_API xcookiejar* xrtCookieJarCreate(const xcookiejarconfig* pConfig)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;

	xrtCookieJarConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtCookieJarConfigValid(&Config) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pJar = (xcookiejar*)xrtMalloc(sizeof(*pJar));
	if ( pJar == NULL ) {
		return NULL;
	}
	memset(pJar, 0, sizeof(*pJar));
	pJar->RefCount = 1;
	pJar->Config = Config;
	if ( !xrtMutexInit(&pJar->Lock) ) {
		xrtFree(pJar);
		return NULL;
	}
	return pJar;
}



/* 增加 Jar 引用。 */
XRT_API xcookiejar* xrtCookieJarRetain(const xcookiejar* pJar)
{
	if ( (pJar == NULL) ||
		(xrtRefRetain(&((xcookiejar*)pJar)->RefCount) < 0) ) {
		if ( pJar == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	return (xcookiejar*)pJar;
}



/* 释放 Jar 最后一个引用及其拥有资源。 */
XRT_API void xrtCookieJarRelease(xcookiejar* pJar)
{
	size_t i;

	if ( (pJar == NULL) || (xrtRefRelease(&pJar->RefCount) != 0) ) {
		return;
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		__xrtCookieJarEntryClear(&pJar->Cookies[i]);
	}
	xrtFree(pJar->Cookies);
	(void)xrtMutexUnit(&pJar->Lock);
	memset(pJar, 0, sizeof(*pJar));
	xrtFree(pJar);
}



/* 清空 Jar 内容并保留容量。 */
XRT_API void xrtCookieJarClear(xcookiejar* pJar)
{
	size_t i;

	if ( pJar == NULL ) {
		return;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return;
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		__xrtCookieJarEntryClear(&pJar->Cookies[i]);
	}
	pJar->Count = 0;
	(void)xrtMutexUnlock(&pJar->Lock);
}



/* 读取当前条目数。 */
XRT_API size_t xrtCookieJarCount(const xcookiejar* pJar)
{
	size_t iCount;

	if ( pJar == NULL ) {
		return 0;
	}
	if ( !xrtMutexLock(&((xcookiejar*)pJar)->Lock) ) {
		return 0;
	}
	iCount = pJar->Count;
	(void)xrtMutexUnlock(&((xcookiejar*)pJar)->Lock);
	return iCount;
}



/* 清理指定时间已经过期的条目。 */
XRT_API size_t xrtCookieJarPurge(xcookiejar* pJar, xtime iNow)
{
	size_t iRemoved;

	if ( pJar == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return 0;
	}
	iRemoved = __xrtCookieJarPurgeLocked(pJar, iNow);
	(void)xrtMutexUnlock(&pJar->Lock);
	return iRemoved;
}



/* 检查 Set 输入的视图和标志，区分参数错误与策略拒绝。 */
static bool __xrtCookieJarSetInputValid(
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	const xcookiereject* pReject
)
{
	const uint32 iContextFlags = XCOOKIE_STORE_HTTP_API |
		XCOOKIE_STORE_HAS_NOW | XCOOKIE_STORE_SAME_SITE |
		XCOOKIE_STORE_TOP_LEVEL;
	const uint32 iCookieFlags = XSET_COOKIE_HAS_DOMAIN |
		XSET_COOKIE_HAS_PATH | XSET_COOKIE_HAS_EXPIRES |
		XSET_COOKIE_HAS_MAX_AGE | XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_SECURE | XSET_COOKIE_HTTP_ONLY |
		XSET_COOKIE_PARTITIONED | XSET_COOKIE_HAS_PRIORITY;

	if ( (pContext == NULL) || (pCookie == NULL) ||
		((pContext->Flags & ~iContextFlags) != 0) ||
		((pCookie->Flags & ~iCookieFlags) != 0) ||
		!__xrtCookieJarViewValid(pContext->URL) ||
		!__xrtCookieJarPartitionValid(pContext->PartitionKey) ||
		!__xrtCookieJarViewValid(pCookie->Name) ||
		!__xrtCookieJarViewValid(pCookie->Value) ||
		!__xrtCookieJarViewValid(pCookie->Domain) ||
		!__xrtCookieJarViewValid(pCookie->Path) ||
		((pReject != NULL) && (
		 __xrtRangesOverlap(
			pReject, sizeof(*pReject), pContext, sizeof(*pContext)
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject), pCookie, sizeof(*pCookie)
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->URL.Data, pContext->URL.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Name.Data, pCookie->Name.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Value.Data, pCookie->Value.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Domain.Data, pCookie->Domain.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Path.Data, pCookie->Path.Size
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 判断非安全来源是否会覆盖已有 Secure Cookie。 */
static bool __xrtCookieJarSecureOverlay(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pNew
)
{
	size_t i;

	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pOld = &pJar->Cookies[i];
		bool bDomainOverlap;
		bool bPathOverlap;

		if ( ((pOld->Flags & XCOOKIE_INFO_SECURE) == 0) ||
			!__xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(pOld->Name, pOld->NameSize),
				__xrtCookieJarEntryView(pNew->Name, pNew->NameSize)
			) || !__xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(
					pOld->PartitionKey, pOld->PartitionKeySize
				),
				__xrtCookieJarEntryView(
					pNew->PartitionKey, pNew->PartitionKeySize
				)
			) ) {
			continue;
		}
		bDomainOverlap = __xrtCookieJarDomainMatch(
			__xrtCookieJarEntryView(pOld->Domain, pOld->DomainSize),
			__xrtCookieJarEntryView(pNew->Domain, pNew->DomainSize)
		) || __xrtCookieJarDomainMatch(
			__xrtCookieJarEntryView(pNew->Domain, pNew->DomainSize),
			__xrtCookieJarEntryView(pOld->Domain, pOld->DomainSize)
		);
		bPathOverlap = __xrtCookieJarPathMatch(
			__xrtCookieJarEntryView(pOld->Path, pOld->PathSize),
			__xrtCookieJarEntryView(pNew->Path, pNew->PathSize)
		) || __xrtCookieJarPathMatch(
			__xrtCookieJarEntryView(pNew->Path, pNew->PathSize),
			__xrtCookieJarEntryView(pOld->Path, pOld->PathSize)
		);
		if ( bDomainOverlap && bPathOverlap ) {
			return true;
		}
	}
	return false;
}



/* 使用已解析 Set-Cookie 执行完整用户代理存储算法。 */
XRT_API xcookiestorestatus xrtCookieJarSet(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	xcookiereject* pReject
)
{
	xrt_cookie_origin Origin;
	xrt_cookie_entry Entry;
	xstrview Host;
	xstrview Domain;
	xstrview Path;
	xstrview PartitionKey = { NULL, 0 };
	xstrview PrefixSubject;
	xcookiesamesite SameSite;
	char sDomain[254];
	xtime iNow;
	xtime iLatest;
	bool bHostOnly = true;
	bool bDelete = false;
	size_t iExisting;
	size_t iDomainCount = 0;
	size_t i;

	if ( (pJar == NULL) ||
		!__xrtCookieJarSetInputValid(pContext, pCookie, pReject) ) {
		return XCOOKIE_STORE_ERROR;
	}
	if ( pReject != NULL ) {
		*pReject = XCOOKIE_REJECT_NONE;
	}
	if ( !__xrtCookieOriginParse(pContext->URL, &Origin) ) {
		return XCOOKIE_STORE_ERROR;
	}
	memset(&Entry, 0, sizeof(Entry));
	Host = (xstrview){ Origin.Host, Origin.HostSize };
	iNow = ((pContext->Flags & XCOOKIE_STORE_HAS_NOW) != 0) ?
		pContext->Now : xrtNow();
	iLatest = __xrtCookieJarLatestExpiry(iNow);
	SameSite = ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) ?
		pCookie->SameSite : XCOOKIE_SAME_SITE_DEFAULT;

	if ( !__xrtCookieJarPairValid(pCookie) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		 ((SameSite < XCOOKIE_SAME_SITE_LAX) ||
		  (SameSite > XCOOKIE_SAME_SITE_NONE))) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) == 0) &&
		 (pCookie->SameSite != XCOOKIE_SAME_SITE_DEFAULT)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) &&
		 ((pCookie->Priority < XCOOKIE_PRIORITY_LOW) ||
		  (pCookie->Priority > XCOOKIE_PRIORITY_HIGH))) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) == 0) &&
		 (pCookie->Priority != XCOOKIE_PRIORITY_UNSPECIFIED)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SYNTAX, pReject);
	}
	if ( ((pCookie->Name.Size == 0) && (pCookie->Value.Size == 0)) ||
		((pCookie->Name.Size == 0) &&
		 (memchr(pCookie->Value.Data, '=', pCookie->Value.Size) != NULL)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_EMPTY, pReject);
	}
	if ( (pCookie->Name.Size > pJar->Config.MaxNameBytes) ||
		(pCookie->Value.Size > pJar->Config.MaxValueBytes) ||
		(pCookie->Name.Size > pJar->Config.MaxCookieBytes) ||
		(pCookie->Value.Size >
		 (pJar->Config.MaxCookieBytes - pCookie->Name.Size)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_LIMIT, pReject);
	}

	Domain = Host;
	if ( (pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0 ) {
		Domain = pCookie->Domain;
		if ( (Domain.Size > pJar->Config.MaxDomainBytes) ||
			!__xrtCookieJarDomainValid(Domain) || Origin.IpHost ||
			!__xrtCookieJarDomainMatch(Host, Domain) ) {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_DOMAIN, pReject
			);
		}
		for ( i = 0; i < Domain.Size; i++ ) {
			sDomain[i] = (char)__xrtHttpAsciiLower(
				(unsigned char)Domain.Data[i]
			);
		}
		Domain = (xstrview){ sDomain, Domain.Size };
		if ( pJar->Config.IsPublicSuffix != NULL ) {
			if ( pJar->Config.IsPublicSuffix(
				pJar->Config.PublicSuffixContext, Domain
			) ) {
				if ( !__xrtCookieJarAsciiEqual(Host, Domain) ) {
					__xrtCookieOriginClear(&Origin);
					return __xrtCookieJarReject(
						XCOOKIE_REJECT_PUBLIC_SUFFIX, pReject
					);
				}
				bHostOnly = true;
			} else {
				bHostOnly = false;
			}
		} else if ( __xrtCookieJarAsciiEqual(Host, Domain) ) {
			bHostOnly = true;
		} else if ( (pJar->Config.Flags &
			XCOOKIE_JAR_ALLOW_UNVERIFIED_DOMAIN) != 0 ) {
			bHostOnly = false;
		} else {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_PUBLIC_SUFFIX, pReject
			);
		}
	}
	Path = (((pCookie->Flags & XSET_COOKIE_HAS_PATH) != 0) &&
		(pCookie->Path.Size != 0) && (pCookie->Path.Data[0] == '/')) ?
		pCookie->Path : __xrtCookieJarDefaultPath(Origin.Path);
	if ( Path.Size > pJar->Config.MaxPathBytes ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_LIMIT, pReject);
	}

	if ( ((pCookie->Flags & XSET_COOKIE_SECURE) != 0) &&
		!Origin.Secure ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SECURE, pReject);
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_STORE_HTTP_API) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_HTTP_ONLY, pReject);
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		(pCookie->SameSite == XCOOKIE_SAME_SITE_NONE) &&
		((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SAME_SITE, pReject);
	}
	if ( (SameSite != XCOOKIE_SAME_SITE_NONE) &&
		((pContext->Flags & XCOOKIE_STORE_SAME_SITE) == 0) &&
		((pContext->Flags & XCOOKIE_STORE_TOP_LEVEL) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SAME_SITE, pReject);
	}
	if ( (pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0 ) {
		if ( ((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
			(pContext->PartitionKey.Size == 0) ||
			(pContext->PartitionKey.Size >
			 pJar->Config.MaxPartitionKeyBytes) ) {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_PARTITION, pReject
			);
		}
		PartitionKey = pContext->PartitionKey;
	}
	PrefixSubject = (pCookie->Name.Size != 0) ?
		pCookie->Name : pCookie->Value;
	if ( __xrtCookieJarPrefix(
		PrefixSubject, "__Secure-", 9u
	) && (((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
		 !Origin.Secure) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_PREFIX, pReject);
	}
	if ( __xrtCookieJarPrefix(
		PrefixSubject, "__Host-", 7u
	) && (((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
		 !Origin.Secure || !bHostOnly ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_PATH) == 0) ||
		 (Path.Size != 1u) || (Path.Data[0] != '/')) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_PREFIX, pReject);
	}

	if ( !__xrtCookieJarEntryCopy(
		&Entry, pCookie->Name, pCookie->Value, Domain, Path, PartitionKey
	) ) {
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_ERROR;
	}
	if ( bHostOnly ) {
		Entry.Flags |= XCOOKIE_INFO_HOST_ONLY;
	}
	if ( (pCookie->Flags & XSET_COOKIE_SECURE) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_SECURE;
	}
	if ( (pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_HTTP_ONLY;
	}
	if ( (pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PARTITIONED;
	}
	Entry.SameSite = SameSite;
	Entry.Priority = ((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) ?
		pCookie->Priority : XCOOKIE_PRIORITY_UNSPECIFIED;
	Entry.Created = iNow;
	Entry.Accessed = iNow;
	if ( (pCookie->Flags & XSET_COOKIE_HAS_MAX_AGE) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PERSISTENT;
		if ( pCookie->MaxAge <= 0 ) {
			Entry.Expires = iNow;
			bDelete = true;
		} else {
			xtime iDuration = (pCookie->MaxAge >
				(INT64_MAX / XRT_TIME_SECOND)) ? INT64_MAX :
				(pCookie->MaxAge * XRT_TIME_SECOND);

			Entry.Expires = __xrtCookieJarTimeAdd(iNow, iDuration);
			if ( Entry.Expires > iLatest ) {
				Entry.Expires = iLatest;
			}
		}
	} else if ( (pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PERSISTENT;
		Entry.Expires = pCookie->Expires;
		if ( Entry.Expires > iLatest ) {
			Entry.Expires = iLatest;
		}
		bDelete = Entry.Expires <= iNow;
	}

	if ( !xrtMutexLock(&pJar->Lock) ) {
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_ERROR;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( !Origin.Secure &&
		((Entry.Flags & XCOOKIE_INFO_SECURE) == 0) &&
		__xrtCookieJarSecureOverlay(pJar, &Entry) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(
			XCOOKIE_REJECT_SECURE_OVERWRITE, pReject
		);
	}
	iExisting = __xrtCookieJarFindLocked(pJar, &Entry);
	if ( (iExisting != XRT_NPOS) &&
		((pJar->Cookies[iExisting].Flags & XCOOKIE_INFO_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_STORE_HTTP_API) == 0) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(
			XCOOKIE_REJECT_HTTP_ONLY, pReject
		);
	}
	if ( bDelete ) {
		if ( iExisting != XRT_NPOS ) {
			__xrtCookieJarRemoveAt(pJar, iExisting);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_REMOVED;
		}
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_IGNORED;
	}

	if ( iExisting != XRT_NPOS ) {
		Entry.Created = pJar->Cookies[iExisting].Created;
		Entry.CreationOrder =
			pJar->Cookies[iExisting].CreationOrder;
		__xrtCookieJarEntryClear(&pJar->Cookies[iExisting]);
		pJar->Cookies[iExisting] = Entry;
	} else {
		xstrview EntryDomain = __xrtCookieJarEntryView(
			Entry.Domain, Entry.DomainSize
		);

		if ( (pJar->Count < pJar->Config.MaxCookies) &&
			!__xrtCookieJarReserve(pJar, pJar->Count + 1u) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_ERROR;
		}
		for ( i = 0; i < pJar->Count; i++ ) {
			if ( __xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(
					pJar->Cookies[i].Domain,
					pJar->Cookies[i].DomainSize
				), EntryDomain
			) ) {
				iDomainCount++;
			}
		}
		while ( iDomainCount >= pJar->Config.MaxCookiesPerDomain ) {
			if ( !__xrtCookieJarEvictLocked(
				pJar, EntryDomain, true
			) ) {
				break;
			}
			iDomainCount--;
		}
		while ( pJar->Count >= pJar->Config.MaxCookies ) {
			if ( !__xrtCookieJarEvictLocked(
				pJar, (xstrview){ NULL, 0 }, false
			) ) {
				break;
			}
		}
		if ( !__xrtCookieJarReserve(pJar, pJar->Count + 1u) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_ERROR;
		}
		pJar->Sequence++;
		if ( pJar->Sequence == 0 ) {
			pJar->Sequence = 1;
		}
		Entry.CreationOrder = pJar->Sequence;
		pJar->Cookies[pJar->Count++] = Entry;
	}
	(void)xrtMutexUnlock(&pJar->Lock);
	__xrtCookieOriginClear(&Origin);
	return XCOOKIE_STORE_STORED;
}



/* 解析字段值后调用结构化存储路径。 */
XRT_API xcookiestorestatus xrtCookieJarStore(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	xstrview SetCookie,
	xcookiereject* pReject
)
{
	xsetcookie Cookie;

	if ( (pJar == NULL) || (pContext == NULL) ||
		!__xrtCookieJarViewValid(SetCookie) ||
		((pReject != NULL) && (
		 __xrtRangesOverlap(
			pReject, sizeof(*pReject), pContext, sizeof(*pContext)
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject), SetCookie.Data, SetCookie.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->URL.Data, pContext->URL.Size
		 ) || __xrtRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return XCOOKIE_STORE_ERROR;
	}
	if ( pReject != NULL ) {
		*pReject = XCOOKIE_REJECT_NONE;
	}
	if ( !xrtSetCookieParse(SetCookie, &Cookie) ) {
		return __xrtCookieJarReject(XCOOKIE_REJECT_SYNTAX, pReject);
	}
	return xrtCookieJarSet(pJar, pContext, &Cookie, pReject);
}



/* 提供普通 HTTP 响应的低心智负担存储路径。 */
XRT_API xcookiestorestatus xrtCookieJarStoreUrl(
	xcookiejar* pJar,
	xstrview URL,
	xstrview SetCookie,
	xcookiereject* pReject
)
{
	xcookiestorecontext Context;

	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Context.URL = URL;
	return xrtCookieJarStore(pJar, &Context, SetCookie, pReject);
}



/* 判断 SameSite 模式是否允许当前请求发送。 */
static bool __xrtCookieJarSameSiteAllowed(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pCookie,
	const xcookierequestcontext* pContext,
	xtime iNow
)
{
	bool bSameSite =
		(pContext->Flags & XCOOKIE_REQUEST_SAME_SITE) != 0;
	bool bTopLevel =
		(pContext->Flags & XCOOKIE_REQUEST_TOP_LEVEL) != 0;
	bool bSafe =
		(pContext->Flags & XCOOKIE_REQUEST_SAFE_METHOD) != 0;

	if ( bSameSite || (pCookie->SameSite == XCOOKIE_SAME_SITE_NONE) ) {
		return true;
	}
	if ( pCookie->SameSite == XCOOKIE_SAME_SITE_STRICT ) {
		return false;
	}
	if ( bTopLevel && bSafe ) {
		return true;
	}
	if ( (pCookie->SameSite == XCOOKIE_SAME_SITE_DEFAULT) &&
		bTopLevel && (pJar->Config.LaxUnsafeAge != 0) &&
		(iNow >= pCookie->Created) &&
		(((uint64)iNow - (uint64)pCookie->Created) <=
		 (uint64)pJar->Config.LaxUnsafeAge) ) {
		return true;
	}
	return false;
}



/* 判断一个条目是否可用于当前请求。 */
static bool __xrtCookieJarEligible(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pCookie,
	const xrt_cookie_origin* pOrigin,
	const xcookierequestcontext* pContext,
	xtime iNow
)
{
	xstrview Host = { pOrigin->Host, pOrigin->HostSize };
	xstrview Domain = { pCookie->Domain, pCookie->DomainSize };

	if ( ((pCookie->Flags & XCOOKIE_INFO_PERSISTENT) != 0) &&
		(pCookie->Expires <= iNow) ) {
		return false;
	}
	if ( ((pCookie->Flags & XCOOKIE_INFO_SECURE) != 0) &&
		!pOrigin->Secure ) {
		return false;
	}
	if ( ((pCookie->Flags & XCOOKIE_INFO_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_REQUEST_HTTP_API) == 0) ) {
		return false;
	}
	if ( (pCookie->Flags & XCOOKIE_INFO_HOST_ONLY) != 0 ) {
		if ( !__xrtCookieJarTextEqual(Host, Domain) ) {
			return false;
		}
	} else if ( !__xrtCookieJarDomainMatch(Host, Domain) ) {
		return false;
	}
	if ( !__xrtCookieJarPathMatch(
		pOrigin->Path,
		__xrtCookieJarEntryView(pCookie->Path, pCookie->PathSize)
	) ) {
		return false;
	}
	if ( (pCookie->Flags & XCOOKIE_INFO_PARTITIONED) != 0 ) {
		if ( !__xrtCookieJarTextEqual(
			pContext->PartitionKey,
			__xrtCookieJarEntryView(
				pCookie->PartitionKey, pCookie->PartitionKeySize
			)
		) ) {
			return false;
		}
	}
	return __xrtCookieJarSameSiteAllowed(
		pJar, pCookie, pContext, iNow
	);
}



/* qsort 比较器实现最长 Path 优先、较早创建优先。 */
static int __xrtCookieJarSelectionCompare(const void* pLeft, const void* pRight)
{
	const xrt_cookie_entry* pA = *(xrt_cookie_entry* const*)pLeft;
	const xrt_cookie_entry* pB = *(xrt_cookie_entry* const*)pRight;

	if ( pA->PathSize != pB->PathSize ) {
		return (pA->PathSize > pB->PathSize) ? -1 : 1;
	}
	if ( pA->CreationOrder == pB->CreationOrder ) {
		return 0;
	}
	return (pA->CreationOrder < pB->CreationOrder) ? -1 : 1;
}



/* 在锁内选择、排序并精确计算一个 Cookie 字段值。 */
static bool __xrtCookieJarSelectLocked(
	xcookiejar* pJar,
	const xrt_cookie_origin* pOrigin,
	const xcookierequestcontext* pContext,
	xtime iNow,
	xrt_cookie_selection* pSelection
)
{
	size_t i;

	memset(pSelection, 0, sizeof(*pSelection));
	if ( pJar->Count != 0 ) {
		if ( pJar->Count >
			(SIZE_MAX / sizeof(*pSelection->Items)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pSelection->Items = (xrt_cookie_entry**)xrtMalloc(
			pJar->Count * sizeof(*pSelection->Items)
		);
		if ( pSelection->Items == NULL ) {
			return false;
		}
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		if ( __xrtCookieJarEligible(
			pJar, &pJar->Cookies[i], pOrigin, pContext, iNow
		) ) {
			pSelection->Items[pSelection->Count++] = &pJar->Cookies[i];
		}
	}
	if ( pSelection->Count > 1u ) {
		qsort(
			pSelection->Items, pSelection->Count,
			sizeof(*pSelection->Items), __xrtCookieJarSelectionCompare
		);
	}
	for ( i = 0; i < pSelection->Count; i++ ) {
		xrt_cookie_entry* pCookie = pSelection->Items[i];
		size_t iExtra;

		if ( (pCookie->ValueSize == SIZE_MAX) ||
			(pCookie->NameSize >
			 (SIZE_MAX - pCookie->ValueSize - 1u)) ) {
			xrtFree(pSelection->Items);
			memset(pSelection, 0, sizeof(*pSelection));
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iExtra = pCookie->NameSize + pCookie->ValueSize + 1u;
		if ( i != 0 ) {
			if ( iExtra > (SIZE_MAX - 2u) ) {
				xrtFree(pSelection->Items);
				memset(pSelection, 0, sizeof(*pSelection));
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iExtra += 2u;
		}
		if ( pSelection->Size > (SIZE_MAX - iExtra) ) {
			xrtFree(pSelection->Items);
			memset(pSelection, 0, sizeof(*pSelection));
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pSelection->Size += iExtra;
	}
	return true;
}



/* 写出已经选择的条目并更新实际发送条目的访问时间。 */
static void __xrtCookieJarSelectionWrite(
	const xrt_cookie_selection* pSelection,
	str sOutput,
	xtime iNow
)
{
	size_t i;
	size_t iOffset = 0;

	for ( i = 0; i < pSelection->Count; i++ ) {
		xrt_cookie_entry* pCookie = pSelection->Items[i];

		if ( i != 0 ) {
			memcpy(sOutput + iOffset, "; ", 2u);
			iOffset += 2u;
		}
		memcpy(sOutput + iOffset, pCookie->Name, pCookie->NameSize);
		iOffset += pCookie->NameSize;
		sOutput[iOffset++] = '=';
		memcpy(sOutput + iOffset, pCookie->Value, pCookie->ValueSize);
		iOffset += pCookie->ValueSize;
		pCookie->Accessed = iNow;
	}
}



/* 共用单次加锁的写入和分配构建路径。 */
static bool __xrtCookieJarRender(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	str* psAllocated
)
{
	const uint32 iFlags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE |
		XCOOKIE_REQUEST_TOP_LEVEL | XCOOKIE_REQUEST_SAFE_METHOD;
	xrt_cookie_origin Origin;
	xrt_cookie_selection Selection;
	xtime iNow;
	str sOutput = (str)pOutput;

	if ( (pJar == NULL) || (pContext == NULL) || (pSize == NULL) ||
		((pContext->Flags & ~iFlags) != 0) ||
		!__xrtCookieJarViewValid(pContext->URL) ||
		!__xrtCookieJarPartitionValid(pContext->PartitionKey) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((psAllocated != NULL) && (pOutput != NULL)) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pContext, sizeof(*pContext)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pContext->URL.Data, pContext->URL.Size
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		) || ((pOutput != NULL) && (
		 __xrtRangesOverlap(pOutput, iCapacity, pSize, sizeof(*pSize)) ||
		 __xrtRangesOverlap(
			pOutput, iCapacity, pContext, sizeof(*pContext)
		 ) || __xrtRangesOverlap(
			pOutput, iCapacity,
			pContext->URL.Data, pContext->URL.Size
		 ) || __xrtRangesOverlap(
			pOutput, iCapacity,
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pSize = 0;
	if ( psAllocated != NULL ) {
		*psAllocated = NULL;
	}
	if ( !__xrtCookieOriginParse(pContext->URL, &Origin) ) {
		return false;
	}
	iNow = ((pContext->Flags & XCOOKIE_REQUEST_HAS_NOW) != 0) ?
		pContext->Now : xrtNow();
	if ( !xrtMutexLock(&pJar->Lock) ) {
		__xrtCookieOriginClear(&Origin);
		return false;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( !__xrtCookieJarSelectLocked(
		pJar, &Origin, pContext, iNow, &Selection
	) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieOriginClear(&Origin);
		return false;
	}
	*pSize = Selection.Size;
	if ( psAllocated != NULL ) {
		if ( Selection.Size == SIZE_MAX ) {
			xrtFree(Selection.Items);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieOriginClear(&Origin);
			__xrtErrorSetSizeOverflow();
			return false;
		}
		sOutput = (str)xrtMalloc(Selection.Size + 1u);
		if ( sOutput == NULL ) {
			xrtFree(Selection.Items);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieOriginClear(&Origin);
			return false;
		}
	} else if ( (pOutput == NULL) || (iCapacity < Selection.Size) ) {
		xrtFree(Selection.Items);
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieOriginClear(&Origin);
		if ( pOutput != NULL ) {
			__xrtErrorSetRange();
		}
		return pOutput == NULL;
	}
	__xrtCookieJarSelectionWrite(&Selection, sOutput, iNow);
	if ( psAllocated != NULL ) {
		sOutput[Selection.Size] = '\0';
		*psAllocated = sOutput;
	}
	xrtFree(Selection.Items);
	(void)xrtMutexUnlock(&pJar->Lock);
	__xrtCookieOriginClear(&Origin);
	return true;
}



/* 写出请求 Cookie 字段值。 */
XRT_API bool xrtCookieJarWrite(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtCookieJarRender(
		pJar, pContext, pOutput, iCapacity, pSize, NULL
	);
}



/* 分配请求 Cookie 字段值。 */
XRT_API str xrtCookieJarBuild(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	size_t* pSize
)
{
	str sOutput = NULL;
	size_t iLocalSize;

	if ( pSize == NULL ) {
		pSize = &iLocalSize;
	}
	if ( !__xrtCookieJarRender(
		pJar, pContext, NULL, 0, pSize, &sOutput
	) ) {
		return NULL;
	}
	return sOutput;
}



/* 初始化普通 URL 快捷请求上下文。 */
static void __xrtCookieJarUrlRequest(
	xcookierequestcontext* pContext,
	xstrview URL
)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE | XCOOKIE_REQUEST_SAFE_METHOD;
	pContext->URL = URL;
}



/* 写出普通同站 HTTP 请求 Cookie。 */
XRT_API bool xrtCookieJarWriteUrl(
	xcookiejar* pJar,
	xstrview URL,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xcookierequestcontext Context;

	__xrtCookieJarUrlRequest(&Context, URL);
	return xrtCookieJarWrite(
		pJar, &Context, pOutput, iCapacity, pSize
	);
}



/* 构建普通同站 HTTP 请求 Cookie。 */
XRT_API str xrtCookieJarBuildUrl(
	xcookiejar* pJar,
	xstrview URL,
	size_t* pSize
)
{
	xcookierequestcontext Context;

	__xrtCookieJarUrlRequest(&Context, URL);
	return xrtCookieJarBuild(pJar, &Context, pSize);
}



/* 创建拥有全部文本的并发稳定快照。 */
XRT_API xcookiesnapshot* xrtCookieJarSnapshot(
	xcookiejar* pJar,
	xtime iNow
)
{
	xcookiesnapshot* pSnapshot;
	size_t iBase;
	size_t iText = 0;
	size_t iTotal;
	size_t i;
	str sText;

	if ( pJar == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return NULL;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( pJar->Count >
		((SIZE_MAX - offsetof(xcookiesnapshot, Cookies)) /
		 sizeof(xcookieinfo)) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBase = offsetof(xcookiesnapshot, Cookies) +
		(pJar->Count * sizeof(xcookieinfo));
	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pCookie = &pJar->Cookies[i];
		size_t Sizes[5];
		size_t iCookie = 0;
		size_t j;

		Sizes[0] = pCookie->NameSize;
		Sizes[1] = pCookie->ValueSize;
		Sizes[2] = pCookie->DomainSize;
		Sizes[3] = pCookie->PathSize;
		Sizes[4] = pCookie->PartitionKeySize;
		for ( j = 0; j < 5u; j++ ) {
			if ( (Sizes[j] == SIZE_MAX) ||
				(iCookie > (SIZE_MAX - Sizes[j] - 1u)) ) {
				(void)xrtMutexUnlock(&pJar->Lock);
				__xrtErrorSetSizeOverflow();
				return NULL;
			}
			iCookie += Sizes[j] + 1u;
		}

		if ( iCookie > (SIZE_MAX - iText) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iText += iCookie;
	}
	if ( iText > (SIZE_MAX - iBase) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = iBase + iText;
	pSnapshot = (xcookiesnapshot*)xrtMalloc(
		(iTotal == 0) ? 1u : iTotal
	);
	if ( pSnapshot == NULL ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		return NULL;
	}
	pSnapshot->Count = pJar->Count;
	sText = (str)((uint8*)pSnapshot + iBase);
	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pSource = &pJar->Cookies[i];
		xcookieinfo* pTarget = &pSnapshot->Cookies[i];
		xstrview* Views[5];
		cstr Sources[5];
		size_t Sizes[5];
		size_t j;

		memset(pTarget, 0, sizeof(*pTarget));
		pTarget->Flags = pSource->Flags;
		pTarget->SameSite = pSource->SameSite;
		pTarget->Priority = pSource->Priority;
		pTarget->Expires = pSource->Expires;
		pTarget->Created = pSource->Created;
		pTarget->Accessed = pSource->Accessed;
		Views[0] = &pTarget->Name;
		Views[1] = &pTarget->Value;
		Views[2] = &pTarget->Domain;
		Views[3] = &pTarget->Path;
		Views[4] = &pTarget->PartitionKey;
		Sources[0] = pSource->Name;
		Sources[1] = pSource->Value;
		Sources[2] = pSource->Domain;
		Sources[3] = pSource->Path;
		Sources[4] = pSource->PartitionKey;
		Sizes[0] = pSource->NameSize;
		Sizes[1] = pSource->ValueSize;
		Sizes[2] = pSource->DomainSize;
		Sizes[3] = pSource->PathSize;
		Sizes[4] = pSource->PartitionKeySize;
		for ( j = 0; j < 5u; j++ ) {
			Views[j]->Data = sText;
			Views[j]->Size = Sizes[j];
			memcpy(sText, Sources[j], Sizes[j]);
			sText[Sizes[j]] = '\0';
			sText += Sizes[j] + 1u;
		}
	}
	(void)xrtMutexUnlock(&pJar->Lock);
	return pSnapshot;
}



/* 释放快照单分配块。 */
XRT_API void xrtCookieSnapshotDestroy(xcookiesnapshot* pSnapshot)
{
	xrtFree(pSnapshot);
}



/* 返回快照数量。 */
XRT_API size_t xrtCookieSnapshotCount(const xcookiesnapshot* pSnapshot)
{
	return (pSnapshot == NULL) ? 0 : pSnapshot->Count;
}



/* 返回快照中的稳定借用条目。 */
XRT_API const xcookieinfo* xrtCookieSnapshotAt(
	const xcookiesnapshot* pSnapshot,
	size_t iIndex
)
{
	if ( (pSnapshot == NULL) || (iIndex >= pSnapshot->Count) ) {
		return NULL;
	}
	return &pSnapshot->Cookies[iIndex];
}

#endif
