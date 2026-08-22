#ifndef XRT_INTERNAL_COOKIE_H
#define XRT_INTERNAL_COOKIE_H

#include "xrt_http.h"

#if defined(XRT_FEATURE_SET_COOKIE)
	#include "xrt_time.h"
#endif

#include <xrt/cookie.h>



#if defined(XRT_FEATURE_COOKIE)

/* 验证借用文本的空值一致性。 */
bool __xrtCookieViewValid(xstrview Text);



/* 判断两个文本是否按 ASCII 规则忽略大小写相等。 */
bool __xrtCookieAsciiEqual(xstrview Left, xstrview Right);



/* 判断一个值是否符合 RFC 10025 严格 cookie-value 语法。 */
bool __xrtCookieValueValid(xstrview Value);



/* 判断一个值是否只包含 Set-Cookie 属性允许的 av-octet。 */
bool __xrtCookieAttributeValueValid(xstrview Value);



/* 判断 Cookie pair 数组的元数据或借用数据是否与指定范围重叠。 */
bool __xrtCookiePairsOverlap(
	const xcookiepair* pPairs,
	size_t iCount,
	const void* pData,
	size_t iSize
);

#endif

#endif
