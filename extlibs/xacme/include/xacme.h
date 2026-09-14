/*
	xacme —— 构建在 xrt 核心之上的 ACME (RFC 8555) 客户端扩展库。

	模块选择见 <xacme/features.h>：默认全量内建 DNS provider，
	XACME_NO_DNS_<厂> 排除个别，或点名 XACME_MODULE_DNS_<厂> 白名单。
*/
#ifndef XACME_H
#define XACME_H

#include <xacme/features.h>

#include <xrt/core.h>

#if defined(XACME_FEATURE_ACME_DNS)
	#include <xrt/acme_dns.h>
#endif

#if defined(XACME_FEATURE_ACME_CORE)
	#include <xrt/acme.h>
#endif

#if defined(XACME_FEATURE_DNS_ALI)
	#include <xrt/acme_dns_ali.h>
#endif

#endif
