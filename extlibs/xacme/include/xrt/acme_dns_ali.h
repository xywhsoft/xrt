#ifndef XRT_ACME_DNS_ALI_H
#define XRT_ACME_DNS_ALI_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_ALI) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_ALI requires acme dns, acme http transport, signing primitives and mutex"
#endif



#if defined(XACME_FEATURE_DNS_ALI)

/*
	阿里云 DNS（alidns）provider，走 V3 签名（ACS3-HMAC-SHA256）。
	Endpoint 默认 alidns.aliyuncs.com；构造时复制凭据和 Endpoint，
	三者各自必须短于 160 字节，超长直接拒绝。Add 接受 ASCII DNS-01
	属主和未填充的 base64url 摘要。传播确认由签发流程层统一负责
	（provider 只做 Add/Remove）。新增响应丢失、5xx、畸形或没有安全 RecordId 时，
	Add 返回 XACME_DNS_ERROR_UNCERTAIN；错误包装/解析分配失败保留内存错误。
	固定槽保留未知状态，后续同名同值 Add/Remove 均失败且不发请求；
	需外部核对该次操作并重建实例，不按同名同值认领或删除。
	已确认和未知记录共用八槽容量；同实例 Add/Remove 串行保护内部状态。
	创建响应复用实例内已登记的 ID 时，新记录按未知结果保留；原记录归属不变。
	DNS 属主按 ASCII 小写登记和匹配，TXT 摘要字节仍区分大小写。
	重复 Add 与 Remove 先按保存的 RecordId 调用 DescribeDomainRecordInfo，
	核对 ID、属主、TXT 值、类型与默认线路。重复 Add 仅在记录已启用时成功；
	已确认 ID 明确消失时可重新创建，其他错误或记录被修改均保留句柄并失败。
	Remove 接受禁用的本实例记录；明确消失时清除句柄而不再次删除。仅 400/404
	配合有效 RequestId、无 RecordId 字段的 DomainRecordNotBelongToUser 表示消失。
	需授权 DescribeDomainRecords、DescribeDomainRecordInfo、AddDomainRecord 与
	DeleteDomainRecord；外部写入者仍须协调读取和删除之间的修改。
	DescribeDomainRecords/DescribeDomainRecordInfo 为只读 RPC，传输断流允许至多
	三次调用，每次重签名；内存失败立即停止。添加和删除不因只读重试规则重放。
	区域发现仅在 400/404 明确返回 InvalidDomainName.NoExist 或 DomainNotFound
	时继续探测父区域；权限、限流、服务错误与畸形应答立即失败。成功应答须有
	有效的分页、请求标识与记录列表，列表记录必须属于请求区域。四项缓存按
	规范化的完整探测起点匹配，满载后替换旧项，不跳过更具体的子区域。
*/
typedef struct xacmednaliconfig {
	cstr sAccessKeyId;
	cstr sAccessKeySecret;
	cstr sEndpoint;
} xacmednaliconfig;

#endif



#if defined(XACME_FEATURE_DNS_ALI)

struct xnetengine;

#endif

XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_DNS_ALI)

/* 全零初始化；AccessKeyId/Secret 必填。 */
XRT_API void xrtAcmeDnsAliConfigInit(xacmednaliconfig* pConfig);

/*
	构造阿里云 DNS provider。内部上下文由 xrtMalloc 分配，
	宿主用 xrtAcmeDnsAliProviderUnit 归还；凭据缺失返回 false。
	pBorrowedEngine 为空时自建网络引擎。
*/
XRT_API bool xrtAcmeDnsAli(
	const xacmednaliconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文（Add 期间记录的 RecordId 列表等）。 */
XRT_API void xrtAcmeDnsAliProviderUnit(xacmednsprovider* pProvider);

#endif



XRT_EXTERN_C_END

#endif
