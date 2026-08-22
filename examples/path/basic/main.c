#include <stdio.h>

#include <xrt.h>



/* 展示路径分解、拼接、清理和改名的常用路径。 */
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

	/* 构建并分解常用路径。 */
	sJoined = xrtPathJoin("project", "src/../include/xrt.h");
	if ( sJoined == NULL ) {
		goto cleanup;
	}
	sName = xrtPathName(sJoined);
	sStem = xrtPathStem(sJoined);
	sExt = xrtPathExt(sJoined);
	sRenamed = xrtPathWithName(sJoined, "runtime.h");
	if ( (sName == NULL) || (sStem == NULL) ||
		 (sExt == NULL) || (sRenamed == NULL) ) {
		goto cleanup;
	}

	/* 逐段读取无需再次分配路径字符串。 */
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
