#ifndef XRT_ACME_DNS_TENCENT_H
#define XRT_ACME_DNS_TENCENT_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_TENCENT) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_TENCENT requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)

/*
	腾讯云 DNSPod provider（API 3.0，TC3-HMAC-SHA256 签名）。
	凭据为 SecretId/SecretKey；Endpoint 默认 dnspod.tencentcloudapi.com。
*/
typedef struct xacmednstencentconfig {
	cstr sSecretId;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednstencentconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_TENCENT)

/* 全零初始化；SecretId/SecretKey 必填。 */
XRT_API void xrtAcmeDnsTencentConfigInit(xacmednstencentconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 已拥有同名同值的重复 Add 核对原 Domain/DomainId/RecordId；身份匹配
 * 且 Enabled=1 时不新建或占槽，禁用返回 XERR_STATE。无法读取原 ID
 * 不释放或重建；仅此只读失败不会标记删除待确认，可再次核对原 ID。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * 每次创建从完整 TXT 属主逐级查询 DescribeDomain，固定已验证的 DomainId。
 * 属主本身为托管域名时以 SubDomain=@ 创建和核验顶点记录。
 * Remove 在该 ID 下读取原 RecordId，核对域名 ID、属主、TXT 值、默认
 * 线路及 Enabled 类型。删除 ACK 要求 HTTP 200、非空 RequestId 且无
 * 矛盾字段；结果未知只读对账，一次 Remove 不重放删除，句柄仍保留时
 * 同名同值 Add 拒绝。RecordIdInvalid 或索引库存中的缺失不能证明
 * 记录已消失；该状态保留句柄并阻止重复 Add。创建后至少 30 秒才
 * 查询当前域名 ID 及最多 3000 条完整无过滤库存，用于诊断而非释放。
 * 官方 30 秒重查建议不是索引延迟上限。未获严格删除 ACK 时，后续
 * 仍只核对原 ID；能重新读取且身份匹配时可恢复清理。始终无法确认
 * 的缺失（含已提交删除的丢失应答）返回错误，宿主须核对云端原操作
 * 后决定何时重建实例。宿主仍须协调查询与删除之间的外部写者。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例；Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsTencent(
	const xacmednstencentconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsTencentProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
