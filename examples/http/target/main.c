#include <stdio.h>
#include <xrt.h>



/*
 * 范例：http/target —— 请求目标四形态与有效 authority 选择
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpTargetParse        按方法解析请求目标
 *   xrtHttpTargetAuthority    取"有效 authority"（目标优先，Host 兜底）
 *   xhttptarget               结果：Host/Port/Path/Query 视图
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/target/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   host=example.test port=8443 path=/items
 *
 * absolute-form 的价值：代理收到的请求行是完整 URL
 *   （https://example.test:8443/items?q=1）——目标里的
 *   authority 优先于 Host 头（第二个参数被忽略正是演示此规则）。
 * 四形态（origin-form/absolute/authority/asterisk）由 Parse 统一处理，
 *   方法参与校验（CONNECT 的目标必须是 authority-form）。
 */


/* 演示解析 absolute-form 并选择 target 中的有效 authority。 */
int main(void)
{
	xhttptarget Target;
	xhttpauthority Authority;

	if ( !xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/items?q=1"
		),
		&Target
	) || !xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
		&Authority
	) ) {
		return 1;
	}
	printf(
		"host=%.*s port=%u path=%.*s\n",
		(int)Authority.Host.Size,
		Authority.Host.Data,
		(unsigned)Authority.Port,
		(int)Target.Path.Size,
		Target.Path.Data
	);
	return 0;
}
