#ifndef XRT_ACME_STORE_H
#define XRT_ACME_STORE_H

#include <xrt/core.h>
#include <xrt/error.h>

#include <xrt/acme.h>

#if defined(XACME_FEATURE_ACME_STORE) && ( \
	!defined(XACME_FEATURE_ACME_CORE) || \
	!defined(XRT_FEATURE_FILE) || \
	!defined(XRT_FEATURE_FILE_WHOLE) || \
	!defined(XRT_FEATURE_FILE_LOCK) || \
	!defined(XRT_FEATURE_DIR_TEMP) || \
	!defined(XRT_FEATURE_X509_PARSE) || \
	!defined(XRT_FEATURE_CRYPTO_SHA256) || \
	!defined(XRT_FEATURE_PEM) || \
	!defined(XRT_FEATURE_CODEC_BASE64) || \
	!defined(XRT_FEATURE_TIME) || \
	!defined(XRT_FEATURE_BUFFER) || \
	!defined(XRT_FEATURE_DIR) )
	#error "XACME_FEATURE_ACME_STORE requires acme core, file, file lock, dir temp, x509, pem, base64, time and buffer"
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
	  <root>/certs/<domain>/current        当前版本目录名（原子提交点）
	  <root>/certs/<domain>/current.lock   跨进程写者锁（运行期间勿删除）
	  <root>/certs/<domain>/.grant-<16hex>/key.pem
	  <root>/certs/<domain>/.grant-<16hex>/fullchain.pem
	  <root>/certs/<domain>/.grant-<16hex>/meta.txt
	通配符 *.example.com 在磁盘上使用 %2A.example.com；ListDomains 返回
	原始 *.example.com。POSIX 上旧的 *.example.com 目录仍可读，新写入使用
	%2A 映射目录；两者并存时优先读取已提交的新目录。
	旧布局的直属 key.pem/fullchain.pem/meta.txt 在 current 缺失时仍可读取。
	SaveGrant 只发布版本目录，不更新旧布局；宿主须经 API 取产物，
	或读取 current 后固定同一版本目录，不能分别读取旧版直属文件。
	旧版与未引用的完整版本不会自动删除；停用全部读写方后可用仓库的
	tools/prune_acme_store.py 预览及回收，运行期间不能删除 current.lock。
	<ca16> 为 directory URL 的 SHA-256 hex 前 16 字符，多 CA 并存互不污染。
	POSIX 私钥临时文件从创建起使用 0600；Windows 上宿主应限制 root 的 ACL。
	域名必须是 ASCII DNS 名或开头为 *. 的通配符名。
	存储层自身的参数、容量和结构错误使用 xrt.acme.store 错误域。
	文件系统、分配、PEM 与 X509 子操作失败保留其原始错误；尤其不得把
	XERR_MEMORY 包装成 IO、NOT_FOUND 或重新签发的条件。诊断分配失败
	也交付内存错误，调用方不能只按存储层域名判断失败。
*/

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_STORE)

/*
	保存账户密钥 PEM（按 CA 隔离；原子替换）。POSIX 同步目录项；
	发布后同步失败时可能返回 false 但新密钥已可见，须重新读取核对。
*/
XRT_API bool xrtAcmeStoreSaveAccount(
	cstr sRoot, cstr sDirectoryUrl, cstr sAccountPem);

/* 读取账户密钥 PEM（xrtMalloc/xrtFree）；未注册返回 NULL 且置
   XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadAccount(cstr sRoot, cstr sDirectoryUrl);

/* 保存旧布局的独立证书链与 CA 溯源；已有 current 的域名拒绝此操作。 */
XRT_API bool xrtAcmeStoreSaveCert(
	cstr sRoot, cstr sPrimaryDomain, cstr sChainPem, cstr sDirectoryUrl);

/* 读取证书链 PEM；不存在返回 NULL 且置 XERR_NOT_FOUND。 */
XRT_API str xrtAcmeStoreLoadCert(cstr sRoot, cstr sPrimaryDomain);

/* 读取签发时使用的 CA directory（meta.txt）；无溯源返回 NULL。 */
XRT_API str xrtAcmeStoreLoadCertCa(cstr sRoot, cstr sPrimaryDomain);

/*
	签发产物先写入独立的 0700 版本目录，三文件齐备后原子替换 current。
	失败或并发续签不会让 LoadGrant 读取混合版本；写者通过 current.lock
	串行发布，崩溃留下的未引用目录
	可由宿主在停用读取方后清理。POSIX 发布前同步版本目录及其祖先，
	发布后同步 current 所在目录；不支持目录 fsync 的文件系统会拒绝发布。
	若发布后的同步失败，函数返回 false，但新版本可能已经可见；宿主应通过
	LoadGrant 核对当前状态。Windows 的 current 使用同目录临时文件与
	重命名替换；重命名失败后也保留可能已发布的版本供核查。其持久化仍依赖
	系统和卷的刷新/替换语义。
	pGrant 借用；sDirectoryUrl 可为空（meta 溯源留空）。
*/
XRT_API bool xrtAcmeStoreSaveGrant(
	cstr sRoot, cstr sPrimaryDomain,
	const xacmeissuegrant* pGrant, cstr sDirectoryUrl);

/*
	固定一次 current 再读取同一版本的链与私钥。key.pem 或 fullchain.pem
	缺失即失败（XERR_NOT_FOUND），输出清零。两段均 xrtFree。
*/
XRT_API bool xrtAcmeStoreLoadGrant(
	cstr sRoot, cstr sPrimaryDomain, xacmeissuegrant* pOut);

/*
	枚举 <root>/certs/ 下的域名（续签守护遍历用）；%2A 通配符目录
	解码为 *.，新旧目录并存时只返回一次。
	每元素 256 字节；容量不足时返回 false 并置 XERR_RANGE。
*/
XRT_API bool xrtAcmeStoreListDomains(
	cstr sRoot, char (*sOutDomains)[256],
	size_t iCapacity, size_t* pOutCount);

/*
	续签判定：解析本地链叶证书的 notAfter，剩余寿命不足
	iRenewalDays 天时 *pbNeed=true。本地证书缺失同样 *pbNeed=true。
*/
XRT_API bool xrtAcmeStoreNeedRenew(
	cstr sRoot, cstr sPrimaryDomain, int iRenewalDays, bool* pbNeed);

#endif

XRT_EXTERN_C_END

#endif
