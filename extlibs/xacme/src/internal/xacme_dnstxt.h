#ifndef XACME_DNSTXT_H
#define XACME_DNSTXT_H

#include <xrt/core.h>
#include <xrt/error.h>

struct xnetengine;

#if defined(XACME_FEATURE_ACME_DNS)

/* dns_txt 模块稳定错误码（错误域 "xrt.acme.dns.txt"）。 */
typedef enum xacmednstxterror {
	XACME_TXT_ERROR_ARGUMENT = 1,
	XACME_TXT_ERROR_NETWORK,
	XACME_TXT_ERROR_PROTOCOL,
	XACME_TXT_ERROR_TIMEOUT
} xacmednstxterror;

/* TXT 记录值上限：ACME dns-01 的 base64url(SHA-256) 恒为 43 字节。 */
#define XACME_TXT_RECORD_MAX 256u

/* 迷你 DNS 客户端：仅实现 TXT 查询所需的 UDP 报文收发与解析。 */
typedef struct xacmedns {
	struct xnetengine* pEngine;
	bool bEngineOwned;
} xacmedns;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_DNS)

/* pBorrowedEngine 为空时自建引擎。 */
bool xacmeDnsInit(xacmedns* pDns, struct xnetengine* pBorrowedEngine);
void xacmeDnsUnit(xacmedns* pDns);

/*
	一次 TXT 查询。记录值拷入调用方数组（每元素 XACME_TXT_RECORD_MAX
	字节），无记录时成功且计数为零。失败设置线程错误。
	sResolver 为 IP 字面量（v1 不解析解析器域名）。
*/
bool xacmeDnsTxtQuery(
	xacmedns* pDns,
	cstr sResolver,
	uint16 iPort,
	cstr sFqdn,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX],
	size_t iCapacity,
	size_t* pOutCount
);

/*
	解析完整 DNS 响应报文为 TXT 记录集合（不可信网络输入的唯一
	消化口，fuzz 目标）。QR 位缺失/结构损坏 → false；RCODE 非零
	→ true 且零记录。每条记录严格小于 XACME_TXT_RECORD_MAX。
*/
bool xacmeTxtParseResponse(
	const uint8* pData,
	size_t iSize,
	uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX],
	size_t iCapacity,
	size_t* pOutCount
);

/* 轮询直到期望 TXT 值在解析器可见；超时返回 false。 */
bool xacmeDnsTxtWait(
	xacmedns* pDns,
	cstr sResolver,
	uint16 iPort,
	cstr sFqdn,
	cstr sExpected,
	uint64 uTimeoutMs
);

#endif

XRT_EXTERN_C_END

#endif
