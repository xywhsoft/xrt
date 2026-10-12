#ifndef XRT_INTERNAL_HTTP_BODY_STREAM_H
#define XRT_INTERNAL_HTTP_BODY_STREAM_H

#include "xrt_http_body.h"
#include <xrt/http_body_stream.h>
#include <xrt/http_bridge.h>



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)

/* 内部构建器把精确长度输出直接写入单分配队列节点。 */




/* 计量完成后以一次载荷分配构建并提交 Chunk。 */
xhttpbodystreamresult xrtHttpBridgeBodyStreamBuild(
	xhttpbodystream* pStream,
	size_t iSize,
	xhttpbodyfillproc pFill,
	ptr pData
);

#endif

#endif

