/*
 * 范例：id/xid —— 192 位时间有序唯一标识：生成、文本化与解析
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXidMake    生成新 XID（时间有序，无全局锁）
 *   xrtXidWrite   写入调用方文本缓冲（无分配，长度恒为 XID_TEXT_SIZE）
 *   xrtXidParse   从文本解析回二进制 XID（严格定长）
 *   xrtXidTime    从 XID 提取生成时刻（Unix 微秒）
 *   XID_TEXT_CAPACITY / XID_TEXT_SIZE  缓冲容量与规范文本长度
 * 模块宏：XRT_MODULE_XID
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/id/xid/main.c -lws2_32 -liphlpapi
 * 预期输出（时间随运行时刻变化，前缀递增有序）：
 *   XID: V-OPfAm6...（32 字符定长文本，示例）
 *   Unix microseconds: 17...(微秒时间戳)
 *
 * XID 结构（192 位 = 24 字节）：
 *   64 位微秒时间戳 + 48 位随机 + 80 位序列/主机相关位。
 *   时间在前 ⇒ 字典序即生成序：数据库索引友好、日志可排序。
 *   全程零分配：文本路径使用调用方缓冲，二进制路径值语义。
 */

#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xid Value;
	xid Parsed;
	char arrText[XID_TEXT_CAPACITY];   /* 定长文本缓冲（含结尾零） */
	xtime iTime;

	/*
	 * 一条龙验证：生成 → 文本化 → 解析回二进制 → 取时间。
	 * Parse 输入用 XID_TEXT_SIZE 显式长度（不含结尾零），
	 * 任何格式偏差（长度/字符集）都会失败并设置错误。
	 */
	if ( !xrtXidMake(&Value) ||
		 !xrtXidWrite(&Value, arrText, sizeof(arrText)) ||
		 !xrtXidParse((xstrview){ arrText, XID_TEXT_SIZE }, &Parsed) ||
		 !xrtXidTime(&Parsed, &iTime) ) {
		return 1;
	}

	/* 文本恒定 32 字符（base62 风格），可直接进 URL/日志/主键。 */
	printf("XID: %s\n", arrText);

	/* 时间提取：不需要查表或解码整个标识，O(1) 位运算。 */
	printf("Unix microseconds: %lld\n", (long long)iTime);
	return 0;
}
