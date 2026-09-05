#include <xrt/http_encoding.h>

#include <stdio.h>



/*
 * 范例：http/encoding —— 内容编码协商：Accept 选择与响应解码计划
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpAcceptEncodingInit / Add   累积解析 Accept-Encoding
 *   xrtHttpAcceptEncodingSelect       按 q 值 + 服务器能力选编码
 *   xrtHttpCodingName                 编码枚举 → 名字视图
 *   xrtHttpContentEncodingPlan        响应头 → 解码器序列
 * 模块宏：XRT_MODULE_HTTP_ENCODING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/encoding/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   coding=gzip decoders=2
 *
 * 两端对称：请求侧 Select（gzip q=0.8 最高且服务器支持 → gzip）；
 *   响应侧 Plan（"gzip, identity" + "deflate" 两字段 →
 *   解码链 deflate→gzip 共 2 级，按应用逆序解码）。
 *   重复字段与大小写差异都被 Plan 正确合并。
 */


/* 展示零分配解析与服务器可用编码选择。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, identity")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("deflate")
		}
	};
	xhttpacceptencoding Accept;
	xhttpcontentencodingplan Plan;
	xhttpcoding Coding;
	xstrview Name;

	xrtHttpAcceptEncodingInit(&Accept);
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL(
			"gzip;q=0.8, deflate;q=0.4, identity;q=0.1"
		)
	) ) {
		return 1;
	}
	Coding = xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	);
	Name = xrtHttpCodingName(Coding);
	if ( !xrtHttpContentEncodingPlan(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Plan
	) ) {
		return 1;
	}
	printf(
		"coding=%.*s decoders=%zu\n",
		(int)Name.Size,
		Name.Data,
		Plan.DecoderCount
	);
	return (Coding == XHTTP_CODING_GZIP) &&
		(Plan.DecoderCount == 2) ? 0 : 1;
}
