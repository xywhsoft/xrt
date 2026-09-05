/*
 * 范例：path/system —— 系统路径查询：cwd/home/temp/app 与真实化
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPathCwd / Home / Temp   当前目录 / 用户主目录 / 临时目录
 *   xrtPathAppDir              可执行文件所在目录（日志/配置定位）
 *   xrtPathReal                符号链接与相对段的真实化（触文件系统）
 * 模块宏：XRT_MODULE_PATH（依赖 FILE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/path/system/main.c -lws2_32 -liphlpapi
 * 预期输出（值随机器与运行位置变化）：
 *   cwd=D:\GIT\xrt
 *   real=\\?\D:\GIT\xrt
 *   home=C:\Users\Administrator
 *   temp=C:\Users\...\Temp
 *   app=C:\Users\...\Temp\exver
 *
 * 与 path/basic 的分界：basic 全部词法操作（纯字符串），
 *   本例全部查询真实系统状态——AppDir 常用于
 *   "可执行文件旁边找配置/日志"，Cwd 会随 chdir 变化，
 *   定位资源要用 AppDir 而不是 Cwd。
 * real 的 \\?\ 前缀：Windows 长路径规范形式（>260 字符也安全）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sCwd = NULL;
	str sHome = NULL;
	str sTemp = NULL;
	str sApp = NULL;
	str sReal = NULL;
	int iResult = 1;

	/* 五项查询全部拥有式返回，任何一项失败统一走清理。 */
	sCwd = xrtPathCwd();
	sHome = xrtPathHome();
	sTemp = xrtPathTemp();
	sApp = xrtPathAppDir();
	sReal = xrtPathReal(".");
	if ( (sCwd == NULL) || (sHome == NULL) ||
		 (sTemp == NULL) || (sApp == NULL) || (sReal == NULL) ) {
		goto cleanup;
	}
	printf("cwd=%s\nreal=%s\nhome=%s\ntemp=%s\napp=%s\n",
		sCwd, sReal, sHome, sTemp, sApp);
	iResult = 0;

cleanup:
	xrtFree(sCwd);
	xrtFree(sHome);
	xrtFree(sTemp);
	xrtFree(sApp);
	xrtFree(sReal);
	return iResult;
}
