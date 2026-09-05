/*
 * 范例：core/error_format —— printf 风格构造结构化错误
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSetErrorFormat  按类别/域/码 + printf 格式串直接构造错误，
 *                       一步完成"格式化 + 放入线程错误槽"
 *   xrtErrorMessage    读取错误消息（借用，永不为 NULL）
 *   xrtGetError        借用线程错误槽
 *   xrtClearError      清空线程错误槽
 * 模块宏：XRT_MODULE_ERROR_FORMAT（依赖 ERROR，闭包自动展开）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/core/error_format/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   file does not exist: app.json
 *
 * 与 xrtErrorCreate 的区别：
 *   Create 接受整条消息字符串；Format 接受 printf 格式串 + 可变参数，
 *   内部用 xrtFormatV 渲染后再创建——错误消息可以携带运行时上下文，
 *   不需要调用方先自己拼缓冲区。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/*
	 * 一步构造并放入线程错误槽：
	 *   类别  XERR_NOT_FOUND —— 跨模块稳定的粗分类，供机器判断；
	 *   域    "example.config" —— 模块命名空间，模块内错误码从 1 起编号；
	 *   格式  与 printf 同义（内部走 xrtFormatV，非标准库 vsnprintf），
	 *         "app.json" 填充 %s，最终消息为
	 *         "file does not exist: app.json"。
	 * 失败（如 OOM）时线程槽保持原状并返回 false，本例从简不检查。
	 */
	xrtSetErrorFormat(
		XERR_NOT_FOUND,
		"example.config",
		1,
		"file does not exist: %s",
		"app.json"
	);

	/* 借用读取：消息已含动态上下文；未设置错误时返回 "(no error)"。 */
	printf("%s\n", xrtErrorMessage(xrtGetError()));

	/* 清空线程槽，避免错误泄漏到后续逻辑。 */
	xrtClearError();
	return 0;
}
