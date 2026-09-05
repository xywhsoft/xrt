#include <stdio.h>

#include <xrt.h>



/*
 * 范例：http/base —— 传输无关地基：字段解析、token 迭代与回写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpFieldParse      严格解析单个头字段（名: 值，零分配借用）
 *   xrtHttpTokenNext       按"，/;"切分 token-list 的迭代器
 *   xrtHttpFieldBlockWrite 把字段视图原样回写成字节（容量原子性）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/base/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   keep-alive
 *   Upgrade
 *   Connection: keep-alive, Upgrade
 *
 * 这组 API 是全部 http* 模块的地基：解析结果是借用视图
 *   （零拷贝），validate/parse/write 三段式可往返——
 *   代理、中间件"改一个头再转发"的标准姿势。
 */


/* 展示传输无关的字段解析与 token-list 迭代。 */
int main(void)
{
	xhttpfield Field;
	xhttpnext Next;
	xstrview Token;
	char Output[128];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtHttpFieldParse(
		XRT_STR_LITERAL("Connection: keep-alive, Upgrade"), &Field
	) ) {
		return 1;
	}
	while ( (Next = xrtHttpTokenNext(
		Field.Value, &iOffset, &Token
	)) == XHTTP_NEXT_ITEM ) {
		printf("%.*s\n", (int)Token.Size, Token.Data);
	}
	if ( (Next != XHTTP_NEXT_END) || !xrtHttpFieldBlockWrite(
		&Field, 1, Output, sizeof(Output), &iSize
	) ) {
		return 2;
	}
	printf("%.*s", (int)iSize, Output);
	return 0;
}
