#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/link —— 硬链接：共享同一文件对象
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLinkHard      创建硬链接
 *   xrtPathStat      路径元数据（Identity / LinkCount）
 *   XFILE_EXCLUSIVE  创建时要求不存在（防覆盖）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/link/main.c -lws2_32 -liphlpapi
 * 预期输出（identity 为文件系统卷内编号，每机不同）：
 *   identity=NNNNNNNNNN links=2
 *
 * links=2 的证据：两个名字指向同一 inode（Identity 相同）——
 *   删除任一名字只是引用减一，最后一个名字删除后数据
 *   才真正释放。原子替换（WriteAtomic 的 rename）依赖
 *   同一机制实现"读者无感切换"。
 */


/* 展示硬链接共享同一文件对象。 */
int main(void)
{
	static const char sSource[] = "xrt-link-example-source.tmp";
	static const char sLink[] = "xrt-link-example-hard.tmp";
	xfile File;
	xfileinfo Info;

	(void)xrtFileDelete(sLink);
	(void)xrtFileDelete(sSource);
	xrtClearError();
	File = xrtOpen(sSource,
		XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File == NULL) || !xrtWriteFull(File, "link", 4, NULL) ||
		 !xrtClose(File) || !xrtLinkHard(sSource, sLink) ||
		 !xrtPathStat(sLink, true, &Info) ) {
		return 1;
	}
	printf("identity=%llu links=%llu\n",
		(unsigned long long)Info.Identity,
		(unsigned long long)Info.LinkCount);
	return (xrtFileDelete(sLink) && xrtFileDelete(sSource)) ? 0 : 1;
}
