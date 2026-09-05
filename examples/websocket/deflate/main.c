#include <stdio.h>

#include <xrt.h>



/*
 * 范例：websocket/deflate —— permessage-deflate 协商：offer → 响应
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsExtensionNext        从扩展列表取一项（名字+参数）
 *   xrtWsDeflateOfferParse    解析客户端 offer 参数
 *   xrtWsDeflateAccept        生成服务端接受配置
 *   xrtWsDeflateResponseWrite 把配置写成响应扩展值
 * 模块宏：XRT_MODULE_WEBSOCKET（DEFLATE 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/deflate/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   response=permessage-deflate; server_max_window_bits=10
 *
 * 协商读法：offer 提议 server_max_window_bits=10（服务端
 *   压缩窗口 1KB）+ 通知客户端可带 client_max_window_bits；
 *   Accept 采纳 10 并按最小合规子集回写——只回必要的参数，
 *   剩余按协议默认。窗口位数直接影响内存占用（连接数大的
 *   服务端压低它省内存）。
 */


/* 展示解析客户端 offer 并构造最小合规服务端响应。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"permessage-deflate; server_max_window_bits=10; "
		"client_max_window_bits"
	);
	xwsextension Extension;
	xwsdeflate Offer;
	xwsdeflate Response;
	char Output[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t iOffset = 0;
	size_t iSize;

	if ( xrtWsExtensionNext(
		Text,
		&iOffset,
		&Extension
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	if ( !xrtWsDeflateOfferParse(
		&Extension,
		&Offer
	) || !xrtWsDeflateAccept(
		&Offer,
		&Response
	) || !xrtWsDeflateResponseWrite(
		&Response,
		Output,
		XWS_DEFLATE_MAX_SIZE,
		&iSize
	) ) {
		return 2;
	}
	Output[iSize] = '\0';
	printf("response=%s\n", Output);
	return 0;
}
