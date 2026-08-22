#ifndef XRT_TEST_H
#define XRT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xrt.h>



/* 测试布局时读取目标 ABI 的真实类型对齐。 */
#if defined(_MSC_VER)
	#define TEST_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define TEST_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define TEST_ALIGNOF(Type) alignof(Type)
#else
	#define TEST_ALIGNOF(Type) _Alignof(Type)
#endif



/* 测试断言失败时立即结束当前独立测试。 */
static void testRequire(bool bCondition, cstr sMessage)
{
	if ( !bCondition ) {
		fprintf(stderr, "[FAIL] %s\n", sMessage != NULL ? sMessage : "unknown failure");
		exit(1);
	}
}



/*
	排空内存调试器有意保留的隔离块。
	Reset 在仍有逻辑活动分配时失败，因此不会掩盖真实泄漏。
*/
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	#define testMemoryDebugDrain(sMessage) \
		do { \
			testRequire(xrtMemDebugReset(), (sMessage)); \
		} while ( 0 )
#else
	#define testMemoryDebugDrain(sMessage) \
		do { \
			(void)(sMessage); \
		} while ( 0 )
#endif

#endif
