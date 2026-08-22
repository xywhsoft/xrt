#ifndef XHTTP_TEST_PUBLIC_STRUCT_ABI_H
#define XHTTP_TEST_PUBLIC_STRUCT_ABI_H

#include <stddef.h>



/* 每个编号对应一个必须跨裁剪配置保持稳定的 xhttp 公开结构。 */
typedef enum xhttpabitesttype {
	XHTTP_ABI_CALL_INFO = 0,
	XHTTP_ABI_CLIENT_STATS,
	XHTTP_ABI_CLIENT_CONFIG,
	XHTTP_ABI_CALL_OPTIONS,
	XHTTP_ABI_CALL_RESULT,
	XHTTP_ABI_HTTP1_CALL_RESULT,
	XHTTP_ABI_TYPE_COUNT
} xhttpabitesttype;



/* 返回完整功能编译单元观察到的结构尺寸或关键字段偏移。 */
size_t xhttpAbiFullLayout(xhttpabitesttype Type, size_t iField);



/* 按完整功能编译单元观察到的尺寸覆盖结构，用于检查尾部越界。 */
void xhttpAbiFullWrite(xhttpabitesttype Type, void* pData);

#endif
