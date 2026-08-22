#ifndef XRT_INTERNAL_IO_H
#define XRT_INTERNAL_IO_H

#include "xrt_internal.h"

#include <xrt/io.h>



#if defined(XRT_FEATURE_IO)

/* Reader 保存回调副本，内建适配器可以把紧凑上下文放在对象尾部。 */
struct xreader {
	xreaderops Ops;
	ptr Context;
	bool AtEnd;
};



/* Writer 保存回调副本，内建适配器可以把紧凑上下文放在对象尾部。 */
struct xwriter {
	xwriterops Ops;
	ptr Context;
};



/* 创建带有内联上下文的 Reader 或 Writer。 */
xreader* __xrtReaderCreateInline(
	const xreaderops* pOps,
	size_t iContextSize,
	ptr* ppContext
);

xwriter* __xrtWriterCreateInline(
	const xwriterops* pOps,
	size_t iContextSize,
	ptr* ppContext
);



/* 设置稳定的 xrt.io 错误。 */
void __xrtIoError(
	xerrkind Kind,
	xioerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 在有限无符号位置范围内执行带符号相对移动。 */
bool __xrtIoMove(
	uint64 iBase,
	int64 iOffset,
	uint64 iLimit,
	uint64* pPosition
);

#endif

#endif
