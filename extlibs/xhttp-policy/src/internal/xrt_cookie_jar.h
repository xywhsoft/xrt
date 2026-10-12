#ifndef XRT_INTERNAL_COOKIE_JAR_H
#define XRT_INTERNAL_COOKIE_JAR_H

#include "xrt_cookie.h"
#include "xrt_url.h"

#include <xrt/cookie_jar.h>



#if defined(XHTTP_FEATURE_COOKIE_JAR)

/* CookieJar 条目拥有全部文本，不向可变存储之外暴露借用视图。 */
typedef struct xrt_cookie_entry {
	str Storage;
	str Name;
	str Value;
	str Domain;
	str Path;
	str PartitionKey;
	size_t NameSize;
	size_t ValueSize;
	size_t DomainSize;
	size_t PathSize;
	size_t PartitionKeySize;
	xtime Expires;
	xtime Created;
	xtime Accessed;
	uint64 CreationOrder;
	uint32 Flags;
	xcookiesamesite SameSite;
	xcookiepriority Priority;
} xrt_cookie_entry;



/* 解析后的请求源只在一次公开调用期间借用 URL path。 */
typedef struct xrt_cookie_origin {
	str Host;
	size_t HostSize;
	xstrview Path;
	bool Secure;
	bool IpHost;
} xrt_cookie_origin;



/* CookieJar 使用系统 mutex，不再自行睡眠轮询原子锁。 */
struct xcookiejar {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_cookie_entry* Cookies;
	size_t Count;
	size_t Capacity;
	uint64 Sequence;
	xcookiejarconfig Config;
};



/* 快照把 info 数组和文本放在一个分配块中。 */
struct xcookiesnapshot {
	size_t Count;
	xcookieinfo Cookies[1];
};



/* 验证并规范化 Cookie URL，Host 由调用方释放。 */
bool __xrtCookieOriginParse(xstrview URL, xrt_cookie_origin* pOrigin);



/* 释放规范化 URL 中拥有的 Host。 */
void __xrtCookieOriginClear(xrt_cookie_origin* pOrigin);

#endif

#endif

