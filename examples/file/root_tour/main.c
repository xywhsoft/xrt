/*
 * 范例：file/root_tour —— 沙箱根补遗：链接/FIFO/模式/原生/统计
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRootPath / xrtRootNative   诊断路径与原生目录句柄
 *   xrtRootStat                   根内元数据查询
 *   xrtRootLinkCreate / LinkRead / LinkHard   根内链接三件套
 *   xrtRootFifoCreate             根内 FIFO（平台支持时）
 *   xrtRootSetMode                根内 POSIX 模式（Windows 不支持）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/file/root_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   root=. diag=. native=1
 *   stat=1 link-read=target.txt
 *   fifo=unsupported(windows) mode=unsupported(windows)
 *
 * 与 file/root 范例（基础开门/关门）互补：这里覆盖根内
 *   链接/FIFO/模式等"平台敏感"入口——每个失败都用
 *   XERR_UNSUPPORTED 区分"沙箱拒绝"与"平台没有"。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	xroot Root = xrtRootOpen(".");
	xfileinfo Info;
	bool bOk;
	str sTarget;

	if ( Root == NULL ) {
		return 1;
	}
	printf("root=%s diag=%s native=%d\n",
		xrtRootPath(Root), xrtRootPath(Root),
		xrtRootNative(Root) != 0 ? 1 : 0);

	/* Stat：相对名在根内解析。 */
	(void)xrtFileWriteAll("xrt-root-tour-target.tmp",
		XRT_BYTES_LITERAL("t"));
	bOk = xrtRootStat(Root, "xrt-root-tour-target.tmp", true, &Info);
	printf("stat=%d size=%llu\n", bOk ? 1 : 0,
		bOk ? (unsigned long long)Info.Size : 0ull);

	/* LinkCreate + LinkRead：目标按原文本保存。 */
	bOk = xrtRootLinkCreate(Root, "target.txt", "xrt-root-tour-link", false);
	if ( bOk ) {
		sTarget = xrtRootLinkRead(Root, "xrt-root-tour-link");
		printf("link-read=%s\n", sTarget ? sTarget : "(null)");
		xrtFree(sTarget);
		(void)xrtRootRemove(Root, "xrt-root-tour-link");
	} else {
		printf("link-create=failed (dev mode)\n");
	}

	/* LinkHard：根内硬链接（源和目标都在根内解析）。 */
	bOk = xrtRootLinkHard(Root, "xrt-root-tour-target.tmp",
		"xrt-root-tour-hard");
	printf("link-hard=%d\n", bOk ? 1 : 0);
	if ( bOk ) {
		(void)xrtRootRemove(Root, "xrt-root-tour-hard");
	}

	/* FifoCreate / SetMode：平台敏感入口的能力探测。 */
	bOk = xrtRootFifoCreate(Root, "xrt-root-tour-fifo", 0600u);
	if ( !bOk && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED ) {
		printf("fifo=unsupported(windows)\n");
	} else {
		printf("fifo=%d\n", bOk ? 1 : 0);
		if ( bOk ) {
			(void)xrtRootRemove(Root, "xrt-root-tour-fifo");
		}
	}
	bOk = xrtRootSetMode(Root, "xrt-root-tour-target.tmp", true, 0600u);
	if ( !bOk && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED ) {
		printf("mode=unsupported(windows)\n");
	} else {
		printf("mode=%d\n", bOk ? 1 : 0);
	}

	(void)xrtRootRemove(Root, "xrt-root-tour-target.tmp");
	(void)xrtRootClose(Root);
	return 0;
}
