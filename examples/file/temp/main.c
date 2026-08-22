#include <stdio.h>

#include <xrt.h>



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
