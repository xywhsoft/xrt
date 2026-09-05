/*
 * 范例：file/link_tour —— 链接与路径属性全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLinkCreate / LinkRead / LinkDelete   符号链接三件套
 *   xrtPathExists / xrtPathRename           存在性 / 同卷重命名
 *   xrtPathSetTimes / SetMode / SetAttributes   属性三件套（平台差异）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows；符号链接创建可能需要开发者模式）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/file/link_tour/main.c -lws2_32 -liphlpapi
 * 预期输出（无开发者模式的 Windows 在 link-create 处降级）：
 *   exists=1 renamed=1
 *   link-read=origin.txt
 *   times=1 mode-or-attrs=1
 *   link-delete=1 gone=1
 *
 * 平台差异的诚实展示：SetMode 是 POSIX 权限（Windows 返回
 *   不支持）；SetAttributes 是 Windows 原生属性（POSIX 返回
 *   不支持）——能力探测范式（见 file/fifo）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const char sOrigin[] = "xrt-link-tour-origin.tmp";
	static const char sRenamed[] = "xrt-link-tour-renamed.tmp";
	static const char sLink[] = "xrt-link-tour-link.tmp";
	xtime iNow = xrtNow();
	int iModeOrAttrs = 0;

	(void)xrtFileDelete(sOrigin);
	(void)xrtFileDelete(sRenamed);
	(void)xrtLinkDelete(sLink);
	xrtClearError();

	if ( !xrtFileWriteAll(sOrigin, XRT_BYTES_LITERAL("origin")) ) {
		return 1;
	}
	printf("exists=%d", xrtPathExists(sOrigin) ? 1 : 0);

	/* Rename：同卷重命名（bReplace=false 目标必须不存在）。 */
	if ( !xrtPathRename(sOrigin, sRenamed, false) ) {
		return 2;
	}
	printf(" renamed=%d\n", xrtPathExists(sRenamed) ? 1 : 0);

	/* 符号链接：Create（目录提示 false = 文件目标）/ Read / Delete。 */
	if ( !xrtLinkCreate(sRenamed, sLink, false) ) {
		printf("link-create=unsupported-needs-dev-mode\n");
		(void)xrtFileDelete(sRenamed);
		return 0;
	}
	{
		str sTarget = xrtLinkRead(sLink);

		printf("link-read=%s\n", sTarget ? sTarget : "(null)");
		xrtFree(sTarget);
	}

	/* 属性三件套：Times 跨平台；Mode/Attributes 各归一家。 */
	printf("times=%d", xrtPathSetTimes(sRenamed, true, &iNow, &iNow) ? 1 : 0);
	if ( xrtPathSetMode(sRenamed, true, 0600u) ) {
		iModeOrAttrs = 1;
	} else if ( xrtPathSetAttributes(sRenamed, 0u) ) {
		iModeOrAttrs = 1;
	}
	printf(" mode-or-attrs=%d\n", iModeOrAttrs);

	/* LinkDelete 只删链接本身；目标不受影响。 */
	if ( !xrtLinkDelete(sLink) ) {
		return 4;
	}
	printf("link-delete=1 gone=%d target-still=%d\n",
		xrtPathExists(sLink) ? 0 : 1,
		xrtPathExists(sRenamed) ? 1 : 0);
	(void)xrtFileDelete(sRenamed);
	return 0;
}
