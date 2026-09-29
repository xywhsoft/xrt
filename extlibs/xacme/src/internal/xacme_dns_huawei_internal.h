#ifndef XACME_DNS_HUAWEI_INTERNAL_H
#define XACME_DNS_HUAWEI_INTERNAL_H

#include <xrt/buffer.h>

/* 构造 recordsets 创建请求，输出缓冲由调用方初始化并释放。 */
bool xacmeDnsHuaweiBuildCreateBody(
	xbuffer* pBody, xstrview sFqdn, xstrview sTxt);

#endif
