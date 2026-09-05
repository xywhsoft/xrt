/*
 * 范例：string/iterators —— 零分配迭代器：Lines / Fields 双子星
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrLinesInit / LinesNext   行迭代器（\n 分段，保留 \r 处理见输出）
 *   xrtStrFieldsInit / FieldsNext 字段迭代器（连续 ASCII 空白拆分）
 *   xrtStrSplitLines              按行一次性切分（列表形态）
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/iterators/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   line=first
 *   line=second
 *   field=user
 *   field=port
 *   field=443
 *   fields-list=3
 *   split-lines=3
 *
 * Lines/Fields 迭代器是 io/line（面向 Reader）的纯字符串版：
 *   结构体就在栈上，Next 交出借用视图。命令行解析、
 *   配置行读取的标准前哨。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xstrlines Lines;
	xstrview Line;
	xstrfields Fields;
	xstrview Field;

	/* 行迭代：两行文本（第二行带尾随 \r 也算一行内容的一部分）。 */
	if ( xrtStrLinesInit(&Lines, SV("first\nsecond\r\n")) ) {
		while ( xrtStrLinesNext(&Lines, &Line) ) {
			printf("line=%.*s\n", (int)Line.Size, Line.Data);
		}
	}

	/* 字段迭代：连续空白（空格/制表）整体当作一个分隔。 */
	if ( xrtStrFieldsInit(&Fields, SV("  user\tport  443 ")) ) {
		while ( xrtStrFieldsNext(&Fields, &Field) ) {
			printf("field=%.*s\n", (int)Field.Size, Field.Data);
		}
	}

	/* 一次性字段切分（列表版）：对比 FieldsNext 的流式消费。 */
	xstrlist* pFields = xrtStrFields(SV("one two  three"));
	if ( pFields != NULL ) {
		printf("fields-list=%zu\n", pFields->Count);
		xrtStrListFree(pFields);
	}

	/* 一次性按行切分：拿列表与计数（对比迭代器的流式消费）。 */
	xstrlist* pList = xrtStrSplitLines(SV("a\nb\nc"));
	if ( pList != NULL ) {
		printf("split-lines=%zu\n", pList->Count);
		xrtStrListFree(pList);
	}
	return 0;
}
