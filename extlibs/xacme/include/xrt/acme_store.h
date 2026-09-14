#ifndef XRT_ACME_STORE_H
#define XRT_ACME_STORE_H

#include <xrt/core.h>
#include <xrt/error.h>

#if defined(XACME_FEATURE_ACME_STORE) && !defined(XRT_FEATURE_FILE_WHOLE)
	#error "XRT acme store requires whole-file support"
#endif

#if defined(XACME_FEATURE_ACME_STORE) && !defined(XRT_FEATURE_X509_PARSE)
	#error "XRT acme store requires X.509 parsing"
#endif

/* store 模块稳定错误码（错误域 "xrt.acme.store"）。 */
typedef enum xacmestoreerror {
	XACME_STORE_ERROR_ARGUMENT = 1,
	XACME_STORE_ERROR_IO,
	XACME_STORE_ERROR_NOT_FOUND,
	XACME_STORE_ERROR_PARSE
} xacmestoreerror;

/*
	磁盘布局（root 由宿主显式指定，库不猜家目录）：
	  <root>/accounts/<ca16>/account.pem   账户密钥（PKCS#8 PEM）
	  <root>/certs/<domain>/fullchain.pem  证书链
	  <root>/certs/<domain>/meta.txt       "directory=<CA directory URL>"
	<ca16> 为 directory URL 的 SHA-256 hex 前 16 字符，多 CA 并存互不污染。
*/

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_STORE)

/* 保存账户密钥 PEM（按 CA 隔离；原子写）。 */
XRT_API bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem);

/* 读取账户密钥 PEM（xrtMalloc/xrtFree）；未注册返回 NULL 且置
   XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl);

/* 保存证书链与 CA 溯源（原子写）。 */
XRT_API bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl);

/* 读取证书链 PEM；不存在返回 NULL 且置 XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain);

/* 读取签发时使用的 CA directory（meta.txt）；无溯源返回 NULL。 */
XRT_API str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain);

/*
	续签判定：解析本地链叶证书的 notAfter，剩余寿命不足
	iRenewalDays 天时 *pbNeed=true。本地证书缺失同样 *pbNeed=true。
*/
XRT_API bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed);

#endif

XRT_EXTERN_C_END

#endif
