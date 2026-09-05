#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/temp —— 安全临时文件：随机名 + 排他创建
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileTemp   前缀 + 后缀 + 随机中段，排他创建
 *                 出参返回拥有式路径（xrtFree 释放）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/temp/main.c -lws2_32 -liphlpapi
 * 预期输出（路径随机）：
 *   C:${BS}${BS}Users${BS}${BS}...${BS}${BS}Temp${BS}${BS}xrt-example-XXXXXXXX.tmp
 *
 * "安全"的含义：O_EXCL 排他创建——预测/竞猜文件名的
 *   符号链接攻击（返回已被攻击者替换的文件）在此失效；
 *   随机名避免碰撞。临时文件用完必须显式删除
 *   （库不替你清理，路径所有权在调用方）。
 */


/* 创建、使用并显式清理一个安全临时文件。 */
int main(void)
{
	str sPath = NULL;
	xfile File = xrtFileTemp(NULL, "xrt-example-", ".tmp", &sPath);
	bool bResult;

	if ( File == NULL ) {
		return 1;
	}
	printf("%s\n", sPath);
	bResult = xrtWriteFull(File, "temporary", 9u, NULL);
	if ( !xrtClose(File) ) {
		bResult = false;
	}
	if ( !xrtFileDelete(sPath) ) {
		bResult = false;
	}
	xrtFree(sPath);
	return bResult ? 0 : 1;
}
