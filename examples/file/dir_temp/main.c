#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/dir_temp —— 临时目录：排他创建 + 调用方拥有
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDirTemp   前缀 + 随机名创建临时目录（拥有式路径）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/dir_temp/main.c -lws2_32 -liphlpapi
 * 预期输出（路径随机）：
 *   C:${BS}${BS}...${BS}${BS}xrt-example-dir-XXXXXXXX
 *
 * 与 file/temp 对称的目录版：排他创建防符号链接抢占。
 *   典型用法是"工作区目录"——解包、编译、缓存的落点，
 *   用完 DirRemove 整体清场（内容清理由调用方负责）。
 */


/* 创建并清理一个调用方拥有的临时目录。 */
int main(void)
{
	str sPath = xrtDirTemp(NULL, "xrt-example-dir-", NULL);
	bool bResult;

	if ( sPath == NULL ) {
		return 1;
	}
	printf("%s\n", sPath);
	bResult = xrtDirRemove(sPath);
	xrtFree(sPath);
	return bResult ? 0 : 1;
}
