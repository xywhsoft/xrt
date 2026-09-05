/*
 * 范例：id/xid_batch —— 批量生成、便利层、排序比较与错误偏移
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXidMakeMany    一次生成多个 XID（连续存储，无逐个分配）
 *   xrtXidFormat      二进制 XID → 拥有式文本（xrtFree 释放）
 *   xrtXidMakeString  一步"生成 + 文本化"的便捷入口
 *   xrtXidEqual       两个 XID 逐位相等判断
 *   xrtXidCompare     三态比较（字典序 = 生成时间序）
 *   xrtXidIsZero      判断是否全零标识（零值哨兵）
 *   xrtXidErrorOffset 从解析错误中取出首个非法字节位置
 * 模块宏：XRT_MODULE_XID
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/id/xid_batch/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   batch order: 1
 *   invalid offset: 3
 *
 * 读数解释：
 *   batch order = 1：本批次中 Values[0] 与 Values[1] 的三态比较
 *   结果——Compare 提供稳定全序（升序排序/去重的基础原语）；
 *   invalid offset = 3：解析 "bad" 时，第 3 字节（长度不足处）
 *   是首个非法位置——错误自带定位，无需解析错误消息字符串。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xid Values[4] = { XID_ZERO };
	xid Parsed = XID_ZERO;
	xid Zero = XID_ZERO;
	str sFormatted = NULL;
	str sGenerated = NULL;
	size_t iOffset = XRT_NPOS;
	bool bValid = false;

	/*
	 * 批量生成 4 个：调用方给连续存储，函数内部一次完成，
	 * 不为每个标识走一遍分配路径。
	 */
	if ( !xrtXidMakeMany(Values, 4u) || xrtXidIsZero(&Values[0]) ) {
		goto cleanup;
	}

	/*
	 * 便利层（拥有式，需要 xrtFree）：
	 *   Format     已有二进制值 → 文本；
	 *   MakeString 生成新值并直接文本化，一步到位。
	 * 验证往返：Format 的文本 Parse 回来必须与原值 Equal。
	 */
	sFormatted = xrtXidFormat(&Values[0]);
	sGenerated = xrtXidMakeString();
	if ( (sFormatted == NULL) || (sGenerated == NULL) ||
		 !xrtXidParse(
			(xstrview){ sFormatted, XID_TEXT_SIZE }, &Parsed
		 ) || !xrtXidEqual(&Values[0], &Parsed) ||
		 !xrtXidIsZero(&Zero) ) {
		goto cleanup;
	}

	/*
	 * 错误定位：Parse("bad") 必须失败；
	 * 错误对象上能直接取到首个非法偏移（3 = 长度不足处）。
	 * 用完清掉错误，保持线程槽干净。
	 */
	if ( xrtXidParse(XRT_STR_LITERAL("bad"), &Parsed) ||
		 !xrtXidErrorOffset(xrtGetError(), &iOffset) ||
		 (iOffset != 3u) ) {
		goto cleanup;
	}
	xrtClearError();
	bValid = true;

cleanup:
	/* 三态比较：为排序/去重提供稳定全序（本批次结果为 1）。 */
	printf("batch order: %d\n", xrtXidCompare(&Values[0], &Values[1]));
	printf("invalid offset: %zu\n", iOffset);
	xrtFree(sFormatted);
	xrtFree(sGenerated);
	return bValid ? 0 : 1;
}
