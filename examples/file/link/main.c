#include <stdio.h>

#include <xrt.h>



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
