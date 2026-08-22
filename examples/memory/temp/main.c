#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示默认 arena 与显式 arena 的完整临时内存路径。 */
int main(void)
{
	const unsigned char arrData[] = { 1, 2, 3, 4 };
	xtemparena tArena;
	xtemparena* pArena = xrtTempCurrent();
	xtempconfig tConfig = { 1024, 512, 2048 };
	xtempinfo tInfo;
	xtempmark tScope;
	ptr pData;
	char* sOuter;
	char* sInner;
	char* sPromoted;

	if ( pArena == NULL ) {
		return 1;
	}

	/* 默认 arena 适合当前线程或协程中的短生命周期结果。 */
	sOuter = (char*)xrtTemp(32);
	if ( sOuter == NULL ) {
		return 2;
	}
	memcpy(sOuter, "outer", 6);
	tScope = xrtTempBegin(pArena);
	sInner = (char*)xrtTemp(32);
	if ( sInner == NULL ) {
		return 3;
	}
	memcpy(sInner, "inner", 6);
	printf("outer=%s inner=%s\n", sOuter, sInner);
	if ( !xrtTempEnd(&tScope) ) {
		return 4;
	}
	printf("outer_after_scope=%s\n", sOuter);

	/* 结果提升会回收子作用域，只把最终字符串保留在父作用域。 */
	tScope = xrtTempBegin(pArena);
	sInner = xrtTempStr(pArena, XRT_STR_LITERAL("promoted"));
	if ( sInner == NULL ) {
		return 5;
	}
	sPromoted = xrtTempEndStr(&tScope, (xstrview){ sInner, 8 });
	if ( sPromoted == NULL ) {
		return 6;
	}
	printf("promoted=%s\n", sPromoted);
	if ( !xrtTempClear() ) {
		return 7;
	}

	/* 显式 arena 可以嵌入请求、解析器或其他长生命周期对象。 */
	memset(&tArena, 0, sizeof(tArena));
	if ( !xrtTempInit(&tArena, &tConfig) ) {
		return 8;
	}
	if ( xrtTempAlloc(&tArena, 64) == NULL ) {
		xrtTempUnit(&tArena);
		return 9;
	}
	pData = xrtTempDup(&tArena, arrData, sizeof(arrData));
	if ( (pData == NULL) || (memcmp(pData, arrData, sizeof(arrData)) != 0) ) {
		xrtTempUnit(&tArena);
		return 10;
	}

	/* 二进制结果也可以在结束子作用域时提升到父作用域。 */
	tScope = xrtTempBegin(&tArena);
	pData = xrtTempEndDup(&tScope, arrData, sizeof(arrData));
	if ( (pData == NULL) || (memcmp(pData, arrData, sizeof(arrData)) != 0) ) {
		xrtTempUnit(&tArena);
		return 11;
	}
	xrtTempGet(&tArena, &tInfo);
	printf(
		"blocks=%zu retained=%zu current=%zu peak=%zu\n",
		tInfo.BlockCount,
		tInfo.RetainedBytes,
		tInfo.CurrentBytes,
		tInfo.PeakBytes
	);

	/* reset 复用常规块，trim 则在空闲边界主动归还容量。 */
	if ( !xrtTempReset(&tArena) || !xrtTempTrim(&tArena, 0) ) {
		xrtTempUnit(&tArena);
		return 12;
	}
	xrtTempUnit(&tArena);
	return 0;
}
