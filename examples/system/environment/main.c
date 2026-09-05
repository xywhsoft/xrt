/*
 * 范例：system/environment —— 环境变量：设置、读取（拥有式）与删除
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEnvSet     设置环境变量（进程内生效，跨平台收口）
 *   xrtEnvGet     读取为拥有式字符串副本（xrtFree 释放）
 *   xrtEnvRemove  删除变量
 * 模块宏：XRT_MODULE_ENVIRONMENT
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/system/environment/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value=hello
 *
 * 三种取值路径的语义（本例演示拥有式）：
 *   xrtEnvGet        返回副本（NULL = 变量不存在），最常用；
 *   xrtEnvGetView    借用视图，零分配但生命周期跟随进程环境块；
 *   xrtEnvGetDefault 不存在时返回默认值——配置读取一步到位。
 * Set/Remove 返回 bool：Windows 上 putenv 语义差异已由实现抹平。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sValue;

	/* 设置：真实进程环境（子进程会继承）。 */
	if ( !xrtEnvSet("XRT_ENVIRONMENT_EXAMPLE", "hello") ) {
		return 1;
	}

	/*
	 * 读取：返回拥有式副本——之后即使环境被改，
	 * 这份字符串也不受影响（这也是它比 getenv 安全的原因）。
	 */
	sValue = xrtEnvGet("XRT_ENVIRONMENT_EXAMPLE");
	if ( sValue == NULL ) {
		return 2;
	}
	printf("value=%s\n", sValue);
	xrtFree(sValue);

	/* 收尾删除，范例不留下全局副作用。 */
	return xrtEnvRemove("XRT_ENVIRONMENT_EXAMPLE") ? 0 : 3;
}
