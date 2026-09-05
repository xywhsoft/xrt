/*
 * 范例：environment/variants —— 环境变量补遗：Lookup 语义
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEnvLookup   读取 UTF-8 副本；"不存在"是成功状态（输出为空）
 * 模块宏：XRT_MODULE_ENVIRONMENT
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/environment/variants/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   missing=ok
 *   present=hello
 *
 * Lookup vs Get：Get 在变量不存在时返回 NULL（两种失败
 *   不可区分）；Lookup 返回 bool——false 才是真失败
 *   （如编码问题），"不存在"是成功且输出置空。
 *   需要默认值一步到位用 xrtEnvGetDefault。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	str sValue = NULL;

	/* 不存在的变量：成功返回 + 空输出。 */
	if ( xrtEnvLookup("XRT_DEFINITELY_MISSING", &sValue) ) {
		printf("missing=ok\n");
	}
	xrtFree(sValue);
	sValue = NULL;

	/* 存在的变量：拥有式副本。 */
	(void)xrtEnvSet("XRT_LOOKUP_EXAMPLE", "hello");
	if ( xrtEnvLookup("XRT_LOOKUP_EXAMPLE", &sValue) && sValue != NULL ) {
		printf("present=%s\n", sValue);
	}
	xrtFree(sValue);
	(void)xrtEnvRemove("XRT_LOOKUP_EXAMPLE");
	return 0;
}
