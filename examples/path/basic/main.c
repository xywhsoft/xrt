/*
 * 范例：path/basic —— 路径主线：拼接清理、四路分解与逐段迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPathJoin     多段拼接 + 词法清理（../ 折叠、分隔符归一）
 *   xrtPathName/Stem/Ext   文件名 / 主干 / 扩展名三路分解
 *   xrtPathWithName 替换文件名（保留目录部分）
 *   xrtPathIterInit/Next    零分配逐段迭代
 *   xrtPathIsLocal 是否相对路径（XPATH_NATIVE 按平台判定）
 * 模块宏：XRT_MODULE_PATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/path/basic/main.c -lws2_32 -liphlpapi
 * 预期输出（Windows 反斜杠；Linux 为正斜杠）：
 *   path=project\include\xrt.h
 *   name=xrt.h
 *   stem=xrt
 *   ext=.h
 *   renamed=project\include\runtime.h
 *   components=3
 *   local=1
 *
 * Join 的词法清理：输入 "project" + "src/../include/xrt.h"——
 *   src/.. 被折叠掉，输出已是干净路径（不触文件系统，纯词法）。
 * 四路分解一次到位：Name 取最后一段，Stem 去扩展名，Ext 含点。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sJoined = NULL;
	str sName = NULL;
	str sStem = NULL;
	str sExt = NULL;
	str sRenamed = NULL;
	xpathiter Iterator;
	xpathcomponent Component;
	size_t iComponents = 0;
	int iResult = 1;

	/* 拼接 + 清理：src/.. 折叠，得到 project\include\xrt.h。 */
	sJoined = xrtPathJoin("project", "src/../include/xrt.h");
	if ( sJoined == NULL ) {
		goto cleanup;
	}

	/* 四路分解：文件名 / 主干 / 扩展名 / 换名拷贝。 */
	sName = xrtPathName(sJoined);
	sStem = xrtPathStem(sJoined);
	sExt = xrtPathExt(sJoined);
	sRenamed = xrtPathWithName(sJoined, "runtime.h");
	if ( (sName == NULL) || (sStem == NULL) ||
		 (sExt == NULL) || (sRenamed == NULL) ) {
		goto cleanup;
	}

	/*
	 * 逐段迭代：只借用原字符串切片，零分配；
	 * 统计段数 = 3（project / include / xrt.h）。
	 */
	if ( !xrtPathIterInit(&Iterator, xrtStrView(sJoined), XPATH_NATIVE) ) {
		goto cleanup;
	}
	while ( xrtPathNext(&Iterator, &Component) ) {
		iComponents++;
	}

	printf("path=%s\nname=%s\nstem=%s\next=%s\nrenamed=%s\n"
		"components=%zu\nlocal=%d\n",
		sJoined, sName, sStem, sExt, sRenamed, iComponents,
		xrtPathIsLocal(xrtStrView(sJoined), XPATH_NATIVE) ? 1 : 0);
	iResult = 0;

cleanup:
	xrtFree(sJoined);
	xrtFree(sName);
	xrtFree(sStem);
	xrtFree(sExt);
	xrtFree(sRenamed);
	return iResult;
}
