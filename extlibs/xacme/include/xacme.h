/*
	xacme —— 构建在 xrt 核心之上的 ACME (RFC 8555) 客户端扩展库。

	模块选择见 <xacme/features.h>（由 tools/generate_extension_features.py
	按清单生成）：定义 XACME_MODULE_<名> 点名模块，或不定义任何宏取全量。
*/
#ifndef XACME_H
#define XACME_H

#include <xacme/features.h>

#include <xrt.h>

#if defined(XACME_FEATURE_ACME_DNS)
	#include <xrt/acme_dns.h>
#endif

#if defined(XACME_FEATURE_ACME_CORE)
	#include <xrt/acme.h>
#endif

#if defined(XACME_FEATURE_ACME_JOSE)
	#include <xrt/acme_jose.h>
#endif

#if defined(XACME_FEATURE_ACME_CSR)
	#include <xrt/acme_csr.h>
#endif

#if defined(XACME_FEATURE_ACME_HTTP)
	#include <xrt/acme_http.h>
#endif

#if defined(XACME_FEATURE_ACME_FLOW)
	#include <xrt/acme_client.h>
#endif

#if defined(XACME_FEATURE_ACME_OBTAIN)
	#include <xrt/acme_obtain.h>
#endif

#if defined(XACME_FEATURE_ACME_STORE)
	#include <xrt/acme_store.h>
#endif

#if defined(XACME_FEATURE_DNS_ALI)
	#include <xrt/acme_dns_ali.h>
#endif

#if defined(XACME_FEATURE_DNS_CF)
	#include <xrt/acme_dns_cf.h>
#endif

#if defined(XACME_FEATURE_DNS_TENCENT)
	#include <xrt/acme_dns_tencent.h>
#endif

#if defined(XACME_FEATURE_DNS_AWS)
	#include <xrt/acme_dns_aws.h>
#endif

#if defined(XACME_FEATURE_DNS_HUAWEI)
	#include <xrt/acme_dns_huawei.h>
#endif

#endif
