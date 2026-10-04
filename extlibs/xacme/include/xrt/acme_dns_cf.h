#ifndef XRT_ACME_DNS_CF_H
#define XRT_ACME_DNS_CF_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_DNS_CF) && (\
	!defined(XACME_FEATURE_ACME_DNS) || \
	!defined(XACME_FEATURE_ACME_HTTP) || \
	!defined(XRT_FEATURE_JSON) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_MUTEX))
	#error "XACME_FEATURE_DNS_CF requires acme dns, acme http transport and signing primitives"
#endif

#if defined(XACME_FEATURE_DNS_CF)

/*
	Cloudflare DNS provider（API v4，Bearer API Token）。
	Token 建议只授予目标 zone 的 Zone.DNS Edit 权限；均为借用视图。
*/
typedef struct xacmednscfconfig {
	cstr sApiToken;
	cstr sEndpoint;
} xacmednscfconfig;

#endif

struct xnetengine;

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_DNS_CF)

/* 全零初始化；ApiToken 必填。 */
XRT_API void xrtAcmeDnsCfConfigInit(xacmednscfconfig* pConfig);

/* 构造 provider；内部上下文由 xrtMalloc 分配，宿主用 Unit 归还。
 * 新记录从完整属主逐级查询最近的区域，每次重新探测；只有有效空列表
 * 才查父区域，查询错误立即停止。无托管区域返回 XACME_DNS_ERROR_ZONE。
 * 区域列表只接受 HTTP 200；401/403 返回 XERR_PERMISSION，429/5xx 返回
 * XERR_AGAIN，其余异常返回 XERR_PROTOCOL；解析 OOM 保留首个内存诊断。
 * 已拥有同名同值的重复 Add 先读取原 ID 核对身份；匹配时不新建或占槽。
 * 只有严格确认原 ID 不存在才释放并重新发现区域；其他失败保留句柄。
 * Add/Remove 在实例内串行。创建结果未知（含 5xx、畸形或丢失应答）
 * 保留固定槽，后续同名同值 Add/Remove 拒绝且不发请求；未知与已确认
 * 记录共用八槽。JSON/诊断分配失败保留内存错误，不能重放创建。
 * Remove 先按登记时的 zone/record ID 读取并核对 TXT 属主和值；
 * 身份变化或读取失败保留句柄，不发删除。删除应答必须可核验，
 * 未知结果只读对账，确认原 ID 不存在才清空；内存失败保留诊断和
 * 句柄供下次 Remove 对账。一次 Remove 不重复发送删除。
 * GET 与 DELETE 不是原子操作；宿主须协调同一记录的外部写者。
 * 属主按 ASCII 小写登记，TXT 逐字节比较。核对原操作后由宿主决定
 * 何时重建实例；Unit 前须等待调用结束，Unit 不删除云端记录。 */
XRT_API bool xrtAcmeDnsCf(
	const xacmednscfconfig* pConfig,
	struct xnetengine* pBorrowedEngine,
	xacmednsprovider* pProvider
);

/* 释放构造时分配的内部上下文。 */
XRT_API void xrtAcmeDnsCfProviderUnit(xacmednsprovider* pProvider);

#endif

XRT_EXTERN_C_END

#endif
