#ifndef XRT_ACME_DNS_HUAWEI_H
#define XRT_ACME_DNS_HUAWEI_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_HUAWEI) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_HUAWEI requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)

/*
	华为云 DNS provider（API v2，SDK-HMAC-SHA256 签名）。
	凭据为 AK/SK；Endpoint 默认 dns.myhuaweicloud.com。
	注意 recordset 的 name 带尾点、records 值需内嵌双引号。
*/
typedef struct xacmednshuaaweiconfig {
	cstr sAccessKey;
	cstr sSecretKey;
	cstr sEndpoint;
} xacmednshuaaweiconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_HUAWEI)

/* 全零初始化；AccessKey/SecretKey 必填。 */
XRT_API void xrtAcmeDnsHuaweiConfigInit(xacmednshuaaweiconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 新记录从完整属主逐级查询最近的区域，每次重新探测；只有有效空列表
 * 才查父区域，查询错误立即停止。无托管区域返回 XACME_DNS_ERROR_ZONE。
 * 区域列表只接受 HTTP 200；401/403 返回 XERR_PERMISSION，429/5xx 返回
 * XERR_AGAIN，其余异常返回 XERR_PROTOCOL；解析 OOM 保留首个内存诊断。
 * 已拥有同名同值的重复 Add 先读取原 ID；身份完整且 ACTIVE 时不新建
 * 或占槽。创建/更新中返回 XERR_AGAIN，不可用状态返回 XERR_STATE。
 * 删除中保留待确认状态，后续 Add 不发请求；严格缺失才释放并重建。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例。Remove 先读取原 zone/recordset ID，核对 TXT 身份、
 * 唯一值、default=false 和状态；只以 404/DNS.0313 确认记录消失。
 * 202/PENDING_DELETE 仅表示受理，最多四次读取（间隔 0.5/1/2 秒）
 * 确认消失才释放句柄。轮询耗尽返回 XERR_AGAIN；读取/内存失败也
 * 保留句柄。删除中的同名同值 Add 拒绝；后续 Remove 继续对账，
 * 已核验受理的删除不重发，未知结果仅在再次核对原记录后可重试。
 * 需要 dns:recordset:get/delete 权限；宿主须协调同一记录的外部写者。
 * Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsHuawei(
	const xacmednshuaaweiconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsHuaweiProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
