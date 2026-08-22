#ifndef XRT_INTERNAL_HTTP_STATIC_H
#define XRT_INTERNAL_HTTP_STATIC_H

#include "xrt_http.h"

#include <xrt/http_static.h>



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)
	#include <xrt/charset.h>
	#include <xrt/codec.h>
	#include <xrt/path.h>
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)
	#include "xrt_http_semantics.h"
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)
	#include "xrt_http_semantics.h"
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)
	#include "xrt_http_body_file.h"

	#include <xrt/atomic.h>

	#define XRT_HTTP_STATIC_ETAG_CAPACITY 72u



	/* 静态文件对象把可消费文件资源和不可变表示元数据放在同一生命周期内。 */
	struct xhttpstaticfile {
		volatile int32 RefCount;
		xatomicptr File;
		xfileinfo Info;
		xhttprepresentation Representation;
		char ETag[XRT_HTTP_STATIC_ETAG_CAPACITY];
	};
#endif

#endif
