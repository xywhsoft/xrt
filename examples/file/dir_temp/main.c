#include <stdio.h>

#include <xrt.h>



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
