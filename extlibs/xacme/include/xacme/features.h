/*
	xacme 模块选择器（当前手工维护；三态语义是 xacme 特有，
	generate_extension_features.py 尚不支持默认全含与排除宏）。

	三种选择方式（宏必须在包含任何 xacme 头之前定义）：

	1. 默认：不定义任何 XACME_MODULE_* —— 全部内建 DNS provider 编入。
	2. 排除：定义 XACME_NO_DNS_<厂> 从默认全量中剔除个别 provider。
	3. 白名单：点名任意 XACME_MODULE_DNS_<厂> —— 自动退出默认全量，
	   只编入点名的 provider（外加常驻的接口层与核心）。

	单头与模块化两条路径统一到 XACME_FEATURE_*：
	构建器按 manifest 闭包直接 -D 各 XACME_FEATURE_*；
	本文件只在缺省时由 MODULE/默认全量推导补齐 FEATURE 与依赖级联。
*/
#ifndef XACME_FEATURES_H
#define XACME_FEATURES_H



/* ------------------------------------------------------------------ */
/* 1. 白名单信号：点名（或构建器传入）任何一个可选 provider。 */
/* ------------------------------------------------------------------ */
#if defined(XACME_MODULE_DNS_ALI) || defined(XACME_MODULE_DNS_CF) || \
	defined(XACME_MODULE_DNS_TENCENT) || defined(XACME_MODULE_DNS_AWS) || \
	defined(XACME_MODULE_DNS_HUAWEI) || defined(XACME_FEATURE_DNS_ALI) || \
	defined(XACME_FEATURE_DNS_CF) || defined(XACME_FEATURE_DNS_TENCENT) || \
	defined(XACME_FEATURE_DNS_AWS) || defined(XACME_FEATURE_DNS_HUAWEI)
#define XACME_DNS_WHITELIST
#endif



/* ------------------------------------------------------------------ */
/* 2. 非白名单即默认全量。 */
/* ------------------------------------------------------------------ */
#if !defined(XACME_DNS_WHITELIST) && !defined(XACME_MODULE_ALL)
#define XACME_MODULE_ALL
#endif



/* ------------------------------------------------------------------ */
/* 3. 冲突检查。 */
/* ------------------------------------------------------------------ */
#if defined(XACME_DNS_WHITELIST) && \
	(defined(XACME_NO_DNS_ALI) || defined(XACME_NO_DNS_CF) || \
	 defined(XACME_NO_DNS_TENCENT) || defined(XACME_NO_DNS_AWS) || \
	 defined(XACME_NO_DNS_HUAWEI))
#error \
	"白名单模式下不能再用 XACME_NO_DNS_*，不点名即是排除"
#endif

#if defined(XACME_MODULE_DNS_ALI) && defined(XACME_NO_DNS_ALI) || \
	defined(XACME_MODULE_DNS_CF) && defined(XACME_NO_DNS_CF) || \
	defined(XACME_MODULE_DNS_TENCENT) && defined(XACME_NO_DNS_TENCENT) || \
	defined(XACME_MODULE_DNS_AWS) && defined(XACME_NO_DNS_AWS) || \
	defined(XACME_MODULE_DNS_HUAWEI) && defined(XACME_NO_DNS_HUAWEI)
#error "同一个 provider 既点名又排除"
#endif



/* ------------------------------------------------------------------ */
/* 4. provider 特性：默认全量剔除式，或白名单点名式。 */
/* ------------------------------------------------------------------ */
#if (defined(XACME_MODULE_ALL) && !defined(XACME_NO_DNS_ALI)) || \
	defined(XACME_MODULE_DNS_ALI)
#ifndef XACME_FEATURE_DNS_ALI
#define XACME_FEATURE_DNS_ALI
#endif
#endif

#if (defined(XACME_MODULE_ALL) && !defined(XACME_NO_DNS_CF)) || \
	defined(XACME_MODULE_DNS_CF)
#ifndef XACME_FEATURE_DNS_CF
#define XACME_FEATURE_DNS_CF
#endif
#endif

#if (defined(XACME_MODULE_ALL) && !defined(XACME_NO_DNS_TENCENT)) || \
	defined(XACME_MODULE_DNS_TENCENT)
#ifndef XACME_FEATURE_DNS_TENCENT
#define XACME_FEATURE_DNS_TENCENT
#endif
#endif

#if (defined(XACME_MODULE_ALL) && !defined(XACME_NO_DNS_AWS)) || \
	defined(XACME_MODULE_DNS_AWS)
#ifndef XACME_FEATURE_DNS_AWS
#define XACME_FEATURE_DNS_AWS
#endif
#endif

#if (defined(XACME_MODULE_ALL) && !defined(XACME_NO_DNS_HUAWEI)) || \
	defined(XACME_MODULE_DNS_HUAWEI)
#ifndef XACME_FEATURE_DNS_HUAWEI
#define XACME_FEATURE_DNS_HUAWEI
#endif
#endif



/* ------------------------------------------------------------------ */
/* 5. 特性依赖级联：provider 与核心都要求接口层。 */
/* ------------------------------------------------------------------ */
#if defined(XACME_MODULE_ACME_DNS) && !defined(XACME_FEATURE_ACME_DNS)
#define XACME_FEATURE_ACME_DNS
#endif

#if defined(XACME_MODULE_ACME_CORE)
#ifndef XACME_FEATURE_ACME_CORE
#define XACME_FEATURE_ACME_CORE
#endif
#ifndef XACME_FEATURE_ACME_DNS
#define XACME_FEATURE_ACME_DNS
#endif
#endif

#if defined(XACME_MODULE_ACME_STORE)
#ifndef XACME_FEATURE_ACME_STORE
#define XACME_FEATURE_ACME_STORE
#endif
#endif

#if defined(XACME_MODULE_DNS_TXT)
#ifndef XACME_FEATURE_DNS_TXT
#define XACME_FEATURE_DNS_TXT
#endif
#endif

#if defined(XACME_MODULE_ACME_FLOW)
#ifndef XACME_FEATURE_ACME_FLOW
#define XACME_FEATURE_ACME_FLOW
#endif
#ifndef XACME_FEATURE_ACME_HTTP
#define XACME_FEATURE_ACME_HTTP
#endif
#ifndef XACME_FEATURE_ACME_CSR
#define XACME_FEATURE_ACME_CSR
#endif
#ifndef XACME_FEATURE_ACME_JOSE
#define XACME_FEATURE_ACME_JOSE
#endif
#ifndef XACME_FEATURE_ACME_DNS
#define XACME_FEATURE_ACME_DNS
#endif
#endif

#if defined(XACME_MODULE_ACME_HTTP)
#ifndef XACME_FEATURE_ACME_HTTP
#define XACME_FEATURE_ACME_HTTP
#endif
#endif

#if defined(XACME_MODULE_ACME_CSR)
#ifndef XACME_FEATURE_ACME_CSR
#define XACME_FEATURE_ACME_CSR
#endif
#ifndef XACME_FEATURE_ACME_JOSE
#define XACME_FEATURE_ACME_JOSE
#endif
#endif

#if defined(XACME_FEATURE_DNS_ALI) || defined(XACME_FEATURE_DNS_CF) || \
	defined(XACME_FEATURE_DNS_TENCENT) || defined(XACME_FEATURE_DNS_AWS) || \
	defined(XACME_FEATURE_DNS_HUAWEI)
#ifndef XACME_FEATURE_ACME_DNS
#define XACME_FEATURE_ACME_DNS
#endif
#endif



#endif
