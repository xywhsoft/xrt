#ifndef XRT_ACME_DNS_H
#define XRT_ACME_DNS_H

#include <xrt/core.h>
#include <xrt/error.h>





#if defined(XACME_FEATURE_ACME_DNS) && (\
	!defined(XRT_FEATURE_NET_ENGINE) || \
	!defined(XRT_FEATURE_NET_UDP) || \
	!defined(XRT_FEATURE_NET_UDP_SYNC) || \
	!defined(XRT_FEATURE_THREAD) || \
	!defined(XRT_FEATURE_RANDOM) || \
	!defined(XRT_FEATURE_RANDOM_DEFAULT) || \
	!defined(XRT_FEATURE_RANDOM_SECURE) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_ARRAY))
	#error "XACME_FEATURE_ACME_DNS requires net engine, UDP, secure random, time, buffer and array"
#endif

/* DNS provider 模块稳定错误码（错误域 "xrt.acme.dns"）。 */
typedef enum xacmednserror {
	XACME_DNS_ERROR_ARGUMENT = 1,
	XACME_DNS_ERROR_CREDENTIAL,
	XACME_DNS_ERROR_ZONE,
	XACME_DNS_ERROR_PROTOCOL,
	XACME_DNS_ERROR_NETWORK,
	/* A write was sent but record ownership could not be confirmed. Do not replay. */
	XACME_DNS_ERROR_UNCERTAIN,
	/* Built-in provider context was released or transport cleanup has begun. */
	XACME_DNS_ERROR_STATE
} xacmednserror;



#if defined(XACME_FEATURE_ACME_DNS)

/* provider 能力位：PROPAGATE 表示自带传播确认回调。 */
#define XACME_DNS_CAP_PROPAGATE UINT32_C(0x00000001)

typedef struct xacmednsprovider xacmednsprovider;

/* 铺设一条 _acme-challenge TXT 记录；失败时设置线程错误。 */
typedef bool (*xacmednsaddproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	移除一条 TXT 记录；记录已不存在视为成功（幂等）。
	重签中断后的清理路径依赖该语义。
*/
typedef bool (*xacmednsremoveproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	可选：provider 自带传播确认；为空时由库内 TXT 探测器统一确认。
	返回 true 表示记录已在权威侧可见。
*/
typedef bool (*xacmednspropagateproc)(
	xacmednsprovider* pProvider,
	xstrview sFqdn,
	xstrview sTxt
);

/*
	DNS-01 provider 是值语义小结构：内建实现由各家构造函数填充，
	自定义 provider 由宿主直接填写字段，无全局注册；请求状态由
	实例持有。内建构造失败的未交付上下文若尚未退休，会转移至
	跨实例待清理队列；宿主用 xrt/acme_http.h 中的
	xrtAcmeCleanupPending 重试，并在退出/卸载前确认完成。
	内建 ProviderUnit 等待私有引擎退休；完成后 pContext 为 NULL。
	超时或失败时保留上下文供重试，此后实例只能继续 ProviderUnit。
	保留调用前诊断，因此以 pContext 是否为 NULL 判断释放结果。
	宿主须先停止新回调并等待已有调用结束，再串行执行 ProviderUnit；
	ProviderUnit 不可与回调或另一次 ProviderUnit 并发。

	Add/Remove 契约：
	- sFqdn 恒为 _acme-challenge.<domain> 全称，sTxt 为 base64url 文本；
	- 同一 Fqdn 会多条 TXT 并存，Add 必须可叠加；
	- 内建 Add/Remove 的 provider 为空时返回 false 并设置
	  XERR_ARGUMENT/XACME_DNS_ERROR_ARGUMENT；上下文已释放或清理
	  未完成时设置 XERR_STATE/XACME_DNS_ERROR_STATE，错误域均为
	  xrt.acme.dns。该状态检查先于记录操作；自定义 provider 可使用
	  空上下文，最小契约校验不检查其上下文；
	- Add 写入已发送但结果未知时必须返回明确错误；流程不会重放该次
	  Add。无法证明记录所有权时，不得按同名同值查询结果自动删除；
	  提供商层结果未知使用 xrt.acme.dns/XACME_DNS_ERROR_UNCERTAIN；
	- 回调为同步契约且必须可重入；zone 归属解析是 provider 内部职责。
*/
struct xacmednsprovider {
	const char* sId;
	uint32 iCaps;
	ptr pContext;
	xacmednsaddproc Add;
	xacmednsremoveproc Remove;
	xacmednspropagateproc Propagate;
};

#endif



XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_ACME_DNS)

/* 校验 provider 满足签发路径的最小契约（id 与 Add/Remove 非空）。 */
XRT_API bool xrtAcmeDnsProviderValidate(const xacmednsprovider* pProvider);

/*
	内建 provider 枚举；实现在组合根模块 xacme 中，随三态选择结果变化。
	仅枚举 id 字符串；构造各家 provider 仍调用各自的构造函数。
*/
XRT_API size_t xrtAcmeDnsProviderCount(void);
XRT_API const char* xrtAcmeDnsProviderId(size_t iIndex);

#endif



XRT_EXTERN_C_END

#endif
