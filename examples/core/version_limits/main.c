/*
 * 范例：core/version_limits —— 版本串与资源边界默认值
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtVersion              当前 XRT 版本字符串
 *   xrtResourceLimitsInit   处理不受信任输入的通用资源边界
 * 模块宏：XRT_MODULE_CORE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/core/version_limits/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   version=2.0.0-dev
 *   limits: depth=128 entries=100000 input=268435456
 *
 * 资源边界是解析器防 DoS 的公共语言：嵌套深度、条目数、
 *   总字节数三道闸——XRT 内 JSON/XSON/HTTP 解析全部
 *   消费同一组语义（值可能随版本调整，以运行输出为准）。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	xrtresourcelimits Limits;

	printf("version=%s\n", xrtVersion());
	xrtResourceLimitsInit(&Limits);
	printf("limits: depth=%u entries=%llu input=%llu\n",
		(unsigned)Limits.iMaxDepth,
		(unsigned long long)Limits.iMaxEntries,
		(unsigned long long)Limits.iMaxInputBytes);
	return 0;
}
