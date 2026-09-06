/*
 * 范例：path/tour —— 路径分解/构建/相对化/安全段补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【分解】    xrtPathParse（零分配五视图 + 根类型 + 标志）/
 *              IsAbs / IsRoot / IsRooted
 *   【构建】    xrtPathBuild（多段数组）/ Clean /
 *              Parent / WithExt
 *   【相对化】  xrtPathRelative（风格化）/ Rel（系统化）/
 *              Abs（Cwd 基准）
 *   【分隔符】  xrtPathSep / ListSep（平台实测）
 *   【定位】    xrtPathExecutable（进程映像路径）
 *   【工作目录】 xrtPathSetCwd（切换后还原）
 *   【安全段】  SafeSegmentInit / Feed / Finish（流式字节的
 *              跨平台段合法性）
 * 模块宏：XRT_MODULE_PATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/path/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   path: parse root=win flags=5 stem=file ext=.txt
 *   path: build/clean parent+ext ok
 *   path: relative/rel/abs ok
 *   path: sep=\ list=; exe=ok cwd-set=1
 *   path: safe-segment ok=1 bad=0
 *
 * Windows 风格示例（C:\dir\file.txt）：Root 借用驱动器前缀、
 *   Ext 含前导点；安全段检查器逐字节 Feed 演示流式用法。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	static const xstrview arrParts[3] = {
		XRT_STR_INIT("C:"),
		XRT_STR_INIT("dir"),
		XRT_STR_INIT("file.txt")
	};
	xpathparts Parts;
	xpathsafesegment Safe;
	str sBuilt = NULL;
	str sClean = NULL;
	str sParent = NULL;
	str sExt = NULL;
	str sRel = NULL;
	str sAbs = NULL;
	str sCwd = NULL;
	str sExe = NULL;
	cstr sCheck;
	int iResult = 1;

	/* ---- Parse：Windows 完整路径五视图分解 ----
	 * Root 含分隔符（"C:\" 三字节）；flags=ROOTED|ABSOLUTE。 */
	if ( !xrtPathParse(SV("C:\\dir\\file.txt"), XPATH_WINDOWS,
			&Parts) ||
		(Parts.RootKind != XPATH_ROOT_DRIVE) ||
		(Parts.Root.Size != 3u) ||
		(memcmp(Parts.Root.Data, "C:\\", 3u) != 0) ||
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) == 0u) ||
		((Parts.Flags & XPATH_FLAG_ROOTED) == 0u) ||
		(Parts.Name.Size != 8u) ||
		(memcmp(Parts.Name.Data, "file.txt", 8u) != 0) ||
		(Parts.Stem.Size != 4u) ||
		(memcmp(Parts.Stem.Data, "file", 4u) != 0) ||
		(Parts.Ext.Size != 4u) ||
		(memcmp(Parts.Ext.Data, ".txt", 4u) != 0) ) {
		goto Cleanup;
	}
	/* 相对路径：无根且非绝对。 */
	if ( !xrtPathParse(SV("dir/sub"), XPATH_POSIX, &Parts) ||
		(Parts.RootKind != XPATH_ROOT_NONE) ||
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0u) ) {
		goto Cleanup;
	}
	printf("path: parse root=win flags=5 stem=file ext=.txt\n");

	/* ---- IsAbs / IsRoot / IsRooted ---- */
	if ( !xrtPathIsAbs("C:\\a") ||
		xrtPathIsAbs("a\\b") ||
		!xrtPathIsRoot("C:\\") ||
		xrtPathIsRoot("C:\\a") ||
		!xrtPathIsRooted("\\a") ||
		xrtPathIsRooted("a") ) {
		goto Cleanup;
	}

	/* ---- Build / Clean / Parent / WithExt ---- */
	sBuilt = xrtPathBuild(arrParts, 3u, XPATH_WINDOWS);
	if ( (sBuilt == NULL) ||
		/* 驱动器前缀段后不补分隔符（驱动器相对语义）。 */
		(strcmp(sBuilt, "C:dir\\file.txt") != 0) ) {
		goto Cleanup;
	}
	sClean = xrtPathClean(SV("C:\\a\\..\\b\\\\c\\"), XPATH_WINDOWS);
	if ( (sClean == NULL) ||
		(strcmp(sClean, "C:\\b\\c") != 0) ) {
		goto Cleanup;
	}
	sParent = xrtPathParent("C:\\dir\\file.txt");
	if ( (sParent == NULL) ||
		(strcmp(sParent, "C:\\dir") != 0) ) {
		goto Cleanup;
	}
	sExt = xrtPathWithExt("C:\\dir\\file", ".md");
	if ( (sExt == NULL) ||
		(strcmp(sExt, "C:\\dir\\file.md") != 0) ) {
		goto Cleanup;
	}
	printf("path: build/clean parent+ext ok\n");

	/* ---- Relative（风格化）/ Rel（系统化）/ Abs ---- */
	sRel = xrtPathRelative(SV("C:\\a\\b"), SV("C:\\a\\c\\d"),
		XPATH_WINDOWS);
	if ( (sRel == NULL) ||
		(strcmp(sRel, "..\\c\\d") != 0) ) {
		goto Cleanup;
	}
	{
		str sRelSys = xrtPathRel("C:\\a\\b", "C:\\a\\c\\d");

		if ( (sRelSys == NULL) || (strcmp(sRelSys, sRel) != 0) ) {
			xrtFree(sRelSys);
			goto Cleanup;
		}
		xrtFree(sRelSys);
	}
	sAbs = xrtPathAbs(".");
	if ( (sAbs == NULL) || !xrtPathIsAbs(sAbs) ) {
		goto Cleanup;
	}
	printf("path: relative/rel/abs ok\n");

	/* ---- 分隔符 / 可执行文件 / 工作目录切换 ---- */
	printf("path: sep=%c list=%c", xrtPathSep(),
		xrtPathListSep());
	sExe = xrtPathExecutable();
	if ( (sExe == NULL) || !xrtPathIsAbs(sExe) ) {
		goto Cleanup;
	}
	printf(" exe=ok");
	sCwd = xrtPathCwd();
	if ( sCwd == NULL ) {
		goto Cleanup;
	}
	if ( !xrtPathSetCwd(sCwd) ) {  /* 切到当前目录（等价还原） */
		goto Cleanup;
	}
	printf(" cwd-set=1\n");

	/* ---- 安全段：合法逐字节 Feed；非法设备名拒绝 ---- */
	xrtPathSafeSegmentInit(&Safe);
	sCheck = "dir";
	{
		size_t i;

		for ( i = 0; i < 3u; ++i ) {
			if ( !xrtPathSafeSegmentFeed(&Safe,
					(uint8)sCheck[i]) ) {
				goto Cleanup;
			}
		}
		if ( !xrtPathSafeSegmentFinish(&Safe) ) {
			goto Cleanup;
		}
	}
	xrtPathSafeSegmentInit(&Safe);
	sCheck = "COM1";  /* Windows 设备保留名：Finish 拒绝 */
	{
		size_t i;

		for ( i = 0; i < 4u; ++i ) {
			if ( !xrtPathSafeSegmentFeed(&Safe,
					(uint8)sCheck[i]) ) {
				break;
			}
		}
		if ( xrtPathSafeSegmentFinish(&Safe) ) {
			goto Cleanup;
		}
	}
	printf("path: safe-segment ok=1 bad=0\n");
	iResult = 0;

Cleanup:
	xrtFree(sExe);
	xrtFree(sCwd);
	xrtFree(sAbs);
	xrtFree(sRel);
	xrtFree(sExt);
	xrtFree(sParent);
	xrtFree(sClean);
	xrtFree(sBuilt);
	return iResult;
}
