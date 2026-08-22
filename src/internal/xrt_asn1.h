#ifndef XRT_INTERNAL_ASN1_H
#define XRT_INTERNAL_ASN1_H

#include "xrt_internal.h"

#include <xrt/asn1.h>
#include <xrt/buffer.h>



#if defined(XRT_FEATURE_ASN1_DER)

/* 设置带输入偏移的 ASN.1 结构化错误；SIZE_MAX 表示没有适用偏移。 */
void __xrtAsn1Error(
	xerrkind Kind,
	xasn1error Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
);

#endif

#endif
