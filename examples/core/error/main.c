/*
 * 范例：core/error —— 结构化错误对象与原因链
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtErrorCreate   按类别/域/码/消息创建拥有式错误对象
 *   xrtErrorWrap     把下层错误包装为新的上层错误，形成原因链
 *   xrtSetError      把错误放入当前线程的错误槽（移交所有权）
 *   xrtGetError      借用线程错误槽中的错误对象（不取得所有权）
 *   xrtErrorMessage  读取人类可读消息（永不返回 NULL）
 *   xrtErrorIs       沿原因链查找指定类别，命中返回借用的错误
 *   xrtClearError    清空线程错误槽
 *   xrtErrorFree     释放错误对象
 * 模块宏：XRT_MODULE_ERROR
 * 编译（单头形态，Windows）：
 *   gcc -O1 -I single impl.c examples/core/error/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   error: request failed
 *   timeout cause: yes
 *
 * 所有权模型：
 *   - Create/Wrap 返回拥有式指针，最终必须正好一次 Free；
 *   - SetError 语义是"移交"——放进线程槽后仍需自己 Free；
 *   - GetError 只借用，严禁 Free 或长期持有（别的代码可能清槽）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/*
	 * 下层错误：网络超时。四个参数分别是
	 * 跨模块稳定类别（XERR_TIMEOUT）、模块域（"example.net"）、
	 * 模块内错误码（1，各域自行编号）、人类可读消息。
	 * 返回拥有式对象，失败（如 OOM）返回 NULL。
	 */
	xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "example.net", 1, "connect timeout");
	xerror* pError;

	if ( pCause == NULL ) {
		return 1;
	}

	/*
	 * 上层错误：把下层原因"包"进新错误，形成原因链
	 * request failed -> connect timeout。
	 * Wrap 内部会增加原因的引用计数，因此这里必须 Free 自己的引用，
	 * 否则原因对象永远不会归零释放。
	 */
	pError = xrtErrorWrap(pCause, XERR_IO, "example.client", 2, "request failed");
	xrtErrorFree(pCause);
	if ( pError == NULL ) {
		return 2;
	}

	/*
	 * 放入线程错误槽并立即释放自己的引用：
	 * 槽只保存借用，不接管所有权，所以 Free 的时机由创建方决定。
	 * 这是 XRT 全库统一的"SetError 不偷引用"约定。
	 */
	xrtSetError(pError);
	xrtErrorFree(pError);

	/* 借用读取：消息永不为 NULL，未设置错误时返回 "(no error)"。 */
	printf("error: %s\n", xrtErrorMessage(xrtGetError()));

	/*
	 * 沿原因链查找：顶层是 XERR_IO，原因是 XERR_TIMEOUT。
	 * 命中返回那条错误的借用指针（本例不使用），未命中返回 NULL——
	 * 判断"这个错误是不是超时引起的"不需要手工遍历链。
	 */
	printf("timeout cause: %s\n",
		xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ? "yes" : "no");

	/* 清空线程槽，避免错误泄漏到后续逻辑。 */
	xrtClearError();
	return 0;
}
