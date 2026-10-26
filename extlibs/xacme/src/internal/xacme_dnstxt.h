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
/* 等待私有引擎退休（最多 30 秒）；false 保留拥有者供重试。
 * 借用引擎不停止；调用前诊断保留。Init 失败后也须清理非空实例。 */
bool xacmeDnsUnit(xacmedns* pDns);

/*
	一次 TXT 查询。记录值拷入调用方数组（每元素 XACME_TXT_RECORD_MAX
	字节），无记录时成功且计数为零。失败设置线程错误，非空输出计数
	清零；分配失败保留原 MEMORY 诊断，不再重发。
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

/* Query the complete received answer without a fixed text-record window.
 * Success publishes whether the exact nonempty expected value was found.
 * Failure clears pFound and preserves allocator causes. UDP transport only. */
bool xacmeDnsTxtContains(
	xacmedns* pDns, cstr sResolver, uint16 iPort, cstr sFqdn,
	cstr sExpected, bool* pFound);

/*
	解析完整 DNS 响应报文为 TXT 记录集合（不可信网络输入的唯一
	消化口，fuzz 目标）。QR 缺失、TC 截断、结构/压缩损坏 → false；RCODE 非零
	→ true 且零记录。每条记录严格小于 XACME_TXT_RECORD_MAX。
	允许空分段；文本输出拒绝内嵌 NUL 和超长值。所有区段和容量外的
	TXT 仍检查结构。输出计数必须非空；失败时不发布部分记录计数。
*/
bool xacmeTxtParseResponse(
	const uint8* pData,
	size_t iSize,
	uint16 uExpectId,
	char (*sOutRecords)[XACME_TXT_RECORD_MAX],
	size_t iCapacity,
	size_t* pOutCount
);

/* Allocation-free binary comparison over every answer TXT. Validate all
 * sections before publishing; empty strings and segmented values are valid.
 * Text collection separately rejects embedded NUL/unrepresentable values. */
bool xacmeTxtResponseContains(
	const uint8* pData, size_t iSize, uint16 uExpectId,
	xbytesview Expected, bool* pFound);

/* 轮询直到期望 TXT 值在解析器可见；超时返回 false。 */
bool xacmeDnsTxtWait(
	xacmedns* pDns,
	cstr sResolver,
	uint16 iPort,
	cstr sFqdn,
	cstr sExpected,
	int64 uTimeoutMs
);

#endif

XRT_EXTERN_C_END

#endif
