#ifndef XRT_TEST_PUBLIC_STRUCT_ABI_H
#define XRT_TEST_PUBLIC_STRUCT_ABI_H

#include <stddef.h>



/* 每个编号对应一个必须跨裁剪配置保持稳定的公开结构。 */
typedef enum xrtabitesttype {
	XRT_ABI_TLS_CLIENT_CONFIG = 0,
	XRT_ABI_TLS_SERVER_CONFIG,
	XRT_ABI_TLS_STREAM_CONFIG,
	XRT_ABI_TLS_STREAM_EVENTS,
	XRT_ABI_WS_STREAM_CONFIG,
	XRT_ABI_WS_UPGRADE_SERVER_CONFIG,
	XRT_ABI_WS_UPGRADE_CLIENT_CONFIG,
	XRT_ABI_WS_UPGRADE_RESULT,
	XRT_ABI_TYPE_COUNT
} xrtabitesttype;



/* 返回完整功能编译单元观察到的结构尺寸或关键字段偏移。 */
size_t xrtAbiFullLayout(xrtabitesttype Type, size_t iField);



/* 按完整功能编译单元观察到的尺寸覆盖结构，用于检查尾部越界。 */
void xrtAbiFullWrite(xrtabitesttype Type, void* pData);

#endif
