#ifndef XACME_DNS_TENCENT_INTERNAL_H
#define XACME_DNS_TENCENT_INTERNAL_H

#include <xrt/core.h>
#include <xrt/time.h>

/* API 3.0 在 HTTP 2xx 内仍可能返回 Response.Error。 */
bool xacmeDnsTencentResponseSuccess(xstrview sBody);

/* 固定一次 UTC 时间，生成与 X-TC-Timestamp 一致的 TC3 Authorization。 */
bool xacmeDnsTencentAuthorization(
	cstr sId, cstr sKey, cstr sEndpoint, cstr sAction, cstr sBody,
	xtime iNow, char* sAuth, size_t iAuthCapacity,
	char* sTimestamp, size_t iTimestampCapacity
);

#endif
