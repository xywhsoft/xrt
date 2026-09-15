#ifndef XRT_ACME_H
#define XRT_ACME_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme_dns.h>

#if defined(XACME_FEATURE_ACME_CORE) && !defined(XACME_FEATURE_ACME_DNS)
	#error "XRT acme core requires XACME_FEATURE_ACME_DNS"
#endif



/* acme 核心模块稳定错误码（错误域 "xrt.acme"）。 */
typedef enum xacmeerror {
	XACME_ERROR_ARGUMENT = 1,
	XACME_ERROR_CONFIG,
	XACME_ERROR_DIRECTORY,
	XACME_ERROR_ACCOUNT,
	XACME_ERROR_ORDER,
	XACME_ERROR_CHALLENGE,
	XACME_ERROR_FINALIZE,
	XACME_ERROR_RATE_LIMIT,
	XACME_ERROR_PROTOCOL,
	XACME_ERROR_NETWORK,
	XACME_ERROR_DNS,
	XACME_ERROR_STORE
} xacmeerror;



#if defined(XACME_FEATURE_ACME_CORE)

/*
	内置 CA directory 预设。xacme 对 CA 完全中立：任何 RFC 8555
	directory 都可用（LiteSSL 等控制台发放的地址直接传入即可），
	切换 CA = 换 DirectoryUrl 与 EAB，不换构建。
*/
#define XACME_DIRECTORY_LE \
	"https://acme-v02.api.letsencrypt.org/directory"
#define XACME_DIRECTORY_LE_STAGING \
	"https://acme-staging-v02.api.letsencrypt.org/directory"
#define XACME_DIRECTORY_ZEROSSL \
	"https://acme.zerossl.com/v2/DV90"
#define XACME_DIRECTORY_GOOGLE \
	"https://dv.acme-v02.api.pki.goog/directory"
#define XACME_DIRECTORY_BUYPASS \
	"https://api.buypass.com/acme/directory"
#define XACME_DIRECTORY_BUYPASS_TEST \
	"https://api.test.buypass.com/acme/directory"



/* External Account Binding；Kid 与 Hmac 均为借用视图，Hmac 为 base64url 文本。 */
typedef struct xacmeeab {
	cstr sKid;
	cstr sHmac;
} xacmeeab;

/*
	账户配置全部借用视图，宿主保证存活至调用返回。
	DirectoryUrl 必填（建议用 XACME_DIRECTORY_* 预设）；
	AccountKeyPem 为空时由库生成 ES256 账户密钥并经 store 保存。
*/
typedef struct xacmeaccountconfig {
	cstr sDirectoryUrl;
	cstr sAccountKeyPem;
	xacmeeab Eab;
	cstr sContactEmail;
} xacmeaccountconfig;

/*
	签发产物：证书链 + 配对私钥，两段文本均由 xrtFree 释放。
	私钥为 PKCS#8 PEM（ES256），与链中叶证书配对；没有它证书不可用。
*/
typedef struct xacmeissuegrant {
	str sFullchainPem;
	str sKeyPem;
} xacmeissuegrant;

/* 释放一段签发产物（成员非空即释放并清零）。 */
XRT_API void xrtAcmeGrantUnit(xacmeissuegrant* pGrant);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XACME_FEATURE_ACME_CORE)

/* 全零初始化；指针字段为空表示未设置。 */
XRT_API void xrtAcmeAccountConfigInit(xacmeaccountconfig* pConfig);

#endif



#if defined(XACME_FEATURE_ACME_CORE)

/* 释放一段签发产物（成员非空即释放并清零）；入参可为空。 */
XRT_API void xrtAcmeGrantUnit(xacmeissuegrant* pGrant);

#endif



XRT_EXTERN_C_END

#endif
