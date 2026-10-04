#ifndef XACME_DNS_HUAWEI_INTERNAL_H
#define XACME_DNS_HUAWEI_INTERNAL_H

#include <xrt/buffer.h>
#include <xrt/time.h>

/* 拼接签名请求的 HTTPS URL；空间不足时清空输出并失败。 */
bool xacmeDnsHuaweiBuildUrl(
	char* sOutput, size_t iCapacity, cstr sEndpoint, cstr sPathAndQuery);

/* 构造 recordsets 创建请求，输出缓冲由调用方初始化并释放。 */
bool xacmeDnsHuaweiBuildCreateBody(
	xbuffer* pBody, xstrview sFqdn, xstrview sTxt);

/* 按华为云 AK/SK 规则分离 URI、排序 query 并生成签名头。 */
bool xacmeDnsHuaweiCanonicalTarget(cstr sTarget,
	char* sUri, size_t iUriCapacity, char* sQuery, size_t iQueryCapacity);
bool xacmeDnsHuaweiAuthorization(
	cstr sAk, cstr sSk, cstr sEndpoint, cstr sMethod,
	cstr sPathAndQuery, cstr sBody, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sStamp, size_t iStampCapacity);

#endif
