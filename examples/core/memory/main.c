/*
 * 范例：core/memory —— 统一内存收口：分配、扩容、复制与安全清零
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtCalloc     按元素数×大小分配并清零（拥有式）
 *   xrtRealloc    扩容/缩容已有分配（拥有式，失败时原块不变）
 *   xrtMemDup     复制一段内存为新的拥有式分配
 *   xrtSecureZero 清零敏感内存（不会被编译器优化掉）
 *   xrtFree       释放（允许传 NULL）
 * 模块宏：XRT_MODULE_MEMORY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/core/memory/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value=42 copied=yes
 *
 * 为什么要收口：
 *   全库动态内存都走这五个入口（而非直接 malloc），
 *   因此 xrtSetAllocator 可以整体替换底层分配器，
 *   memory_stats / memory_debug 才能观察到每一笔分配。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	/* 被复制源的只读字节（static const：不占用栈也不可写）。 */
	static const unsigned char Source[] = { 1, 2, 3, 4 };
	unsigned char* pValues;
	unsigned char* pCopy;

	/* 分配 4 字节并清零：Calloc 语义与标准一致，返回拥有式指针。 */
	pValues = (unsigned char*)xrtCalloc(4, sizeof(unsigned char));
	if ( pValues == NULL ) {
		return 1;
	}

	/* 写入一个标记值，稍后验证 Realloc 保留了旧内容。 */
	pValues[0] = 42;

	/*
	 * 扩容到 16 字节：Realloc 语义与标准一致——
	 * 成功返回新指针（内容前 4 字节保留），失败返回 NULL 且原块仍有效。
	 * 注意失败分支这里直接退出了程序；健壮写法应保留旧指针再赋值。
	 */
	pValues = (unsigned char*)xrtRealloc(pValues, 16);
	if ( pValues == NULL ) {
		return 2;
	}

	/* 把 4 字节源复制为独立的拥有式缓冲区（分配 + memcpy 一步完成）。 */
	pCopy = (unsigned char*)xrtMemDup(Source, sizeof(Source));
	if ( pCopy == NULL ) {
		xrtFree(pValues);   /* 失败路径也要释放已持有的资源 */
		return 3;
	}

	/* 验证：扩容后旧值仍在；复制内容与源逐字节一致。 */
	printf(
		"value=%u copied=%s\n",
		(unsigned int)pValues[0],
		memcmp(pCopy, Source, sizeof(Source)) == 0 ? "yes" : "no"
	);

	/*
	 * 安全清零：用于密钥、令牌等敏感缓冲区。
	 * 普通 memset 可能被编译器判定"死存储"而删除；
	 * SecureZero 保证清零动作真实发生（易失性写或屏障实现）。
	 */
	xrtSecureZero(pValues, 16);
	xrtSecureZero(pCopy, sizeof(Source));

	/* 释放顺序与持有顺序相反；xrtFree 允许 NULL，无需判空。 */
	xrtFree(pCopy);
	xrtFree(pValues);
	return 0;
}
