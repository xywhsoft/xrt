#ifndef XRT_ACME_DNS_H
#define XRT_ACME_DNS_H

#include <xrt/core.h>
#include <xrt/error.h>




/* DNS provider 模块稳定错误码（错误域 "xrt.acme.dns"）。 */
typedef enum xacmednserror {
	XACME_DNS_ERROR_ARGUMENT = 1,
	XACME_DNS_ERROR_CREDENTIAL,
	XACME_DNS_ERROR_ZONE,
	XACME_DNS_ERROR_PROTOCOL,
	XACME_DNS_ERROR_NETWORK
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
	自定义 provider 由宿主直接填写字段，无注册、无全局状态。

	Add/Remove 契约：
	- sFqdn 恒为 _acme-challenge.<domain> 全称，sTxt 为 base64url 文本；
	- 同一 Fqdn 会多条 TXT 并存，Add 必须可叠加；
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
