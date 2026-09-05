#include <stdio.h>

#include <xrt.h>



/*
 * 范例：http/param —— 分号参数列表：迭代与 quoted-string 解码
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpParamNext       逐项迭代（名=值 对，零分配借用）
 *   XHTTP_PARAM_HAS_VALUE  标志：该项是否带值
 *   xrtHttpParamValueWrite 值解码写入缓冲（含 quoted-string 展开）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/param/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   charset = UTF-8
 *   boundary = part;42
 *
 * quoted-string 的坑与解法：boundary="part;42" 的值里含分号——
 *   直接按 ; 切会碎掉；ValueWrite 按 RFC 展开引号与转义，
 *   分号安全还原为 part;42。Content-Type / Content-Disposition
 *   参数全走这一套。
 */


/* 逐项读取参数，并在需要时解码 quoted-string。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"charset=UTF-8; boundary=\"part;42\""
	);
	xhttpparam Param;
	xhttpnext Next;
	char Value[64];
	size_t iOffset = 0;
	size_t iSize;

	while ( (Next = xrtHttpParamNext(
		Text, &iOffset, &Param
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
			printf("%.*s\n", (int)Param.Name.Size, Param.Name.Data);
			continue;
		}
		if ( !xrtHttpParamValueWrite(
			&Param, Value, sizeof(Value), &iSize
		) ) {
			return 1;
		}
		printf("%.*s = %.*s\n",
			(int)Param.Name.Size, Param.Name.Data,
			(int)iSize, Value);
	}
	return (Next == XHTTP_NEXT_END) ? 0 : 2;
}
