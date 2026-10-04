#ifndef XACME_DNS_ALI_INTERNAL_H
#define XACME_DNS_ALI_INTERNAL_H

#include <xrt/time.h>
#include <xrt/value.h>

/* 按 ACS3 规则校验并排序当前 provider 支持的 ASCII query 参数。 */
bool xacmeDnsAliCanonicalQuery(
	cstr sQuery, char* sOut, size_t iCapacity);

/* 固定时间和 nonce 的离线签名入口；实际请求需生成独立随机 nonce。 */
bool xacmeDnsAliAuthorization(
	cstr sKeyId, cstr sSecret, cstr sEndpoint, cstr sAction,
	cstr sQuery, cstr sNonce, xtime iNow,
	char* sAuth, size_t iAuthCapacity,
	char* sDate, size_t iDateCapacity,
	char* sCanonicalQuery, size_t iQueryCapacity);

/* 校验创建响应中的 RecordId 可安全用于后续删除 query。 */
bool xacmeDnsAliCreateResponseId(
	cstr sBody, char* sRecordId, size_t iCapacity);

typedef enum xacmednsalizoneoutcome {
	XACME_ALI_ZONE_ERROR = -1,
	XACME_ALI_ZONE_MISSING = 0,
	XACME_ALI_ZONE_FOUND = 1
} xacmednsalizoneoutcome;

/* Validate the one-record discovery page; only explicit absence permits fallback. */
xacmednsalizoneoutcome xacmeDnsAliZoneResponse(
	uint16 iStatus, cstr sBody, cstr sDomain);

typedef enum xacmednsalirecordoutcome {
	XACME_ALI_RECORD_ERROR = -1,
	XACME_ALI_RECORD_MISSING = 0,
	XACME_ALI_RECORD_FOUND = 1
} xacmednsalirecordoutcome;

/* Confirm the tracked id and DNS tuple; absence never establishes ownership. */
xacmednsalirecordoutcome xacmeDnsAliRecordResponse(
	uint16 iStatus, cstr sBody, cstr sId, cstr sFqdn, cstr sTxt, bool* pEnabled);

#endif
