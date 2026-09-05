/*
 * 范例：memory/temp —— 临时内存（arena）：作用域、提升与显式竞技场
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTempCurrent     取当前线程/协程的默认 arena
 *   xrtTemp            从默认 arena 分配（谁 reset 谁负责）
 *   xrtTempBegin/End   子作用域：End 整体回收区间内全部分配
 *   xrtTempStr/EndStr  字符串"提升"——子作用域结果存活到父级
 *   xrtTempEndDup      二进制提升（同上）
 *   xrtTempClear       清空默认 arena（成对管理者的收尾）
 *   xrtTempInit/Alloc/Dup/Reset/Trim/Unit   显式 arena 全家
 *   xrtTempSecureReset / xrtTempSecureUnit   安全擦除版收尾（密钥残留清理）
 *   xtempconfig        三元组：块大小/保留水位/上限
 * 模块宏：XRT_MODULE_TEMP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/temp/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   outer=outer inner=inner
 *   outer_after_scope=outer
 *   promoted=promoted
 *   blocks=1 retained=1024 current=96 peak=96
 *
 * arena 心智模型：分配 = 指针前移；释放 = 不做任何事；
 *   回收 = 整段退回（End/Reset）。中间结果天然零碎片、
 *   零逐块释放——解析器每层作用域、HTTP 每个请求的标配。
 * "提升"解决 arena 的经典难题（结果要活得比作用域久）：
 *   EndStr/EndDup 在回收区间前把指定内容复制到父作用域。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	const unsigned char arrData[] = { 1, 2, 3, 4 };
	xtemparena tArena;
	xtemparena* pArena = xrtTempCurrent();   /* 线程默认 arena */
	xtempconfig tConfig = { 1024, 512, 2048 };
	xtempinfo tInfo;
	xtempmark tScope;                        /* 作用域书签 */
	ptr pData;
	char* sOuter;
	char* sInner;
	char* sPromoted;

	if ( pArena == NULL ) {
		return 1;
	}

	/* 父级分配：作用域结束后仍存活。 */
	sOuter = (char*)xrtTemp(32);
	if ( sOuter == NULL ) {
		return 2;
	}
	memcpy(sOuter, "outer", 6);

	/* 子作用域：Begin 打书签 → 分配 inner → End 整段回收。 */
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

	/* 关键验证：inner 的空间已被回收复用，outer 完好无损。 */
	printf("outer_after_scope=%s\n", sOuter);

	/*
	 * 字符串提升：子作用域里拼好 "promoted"，
	 * EndStr 在回收区间前把它复制进父作用域——
	 * 结果存活、其余中间量全部退回，一步两得。
	 */
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

	/*
	 * 显式 arena：嵌入请求/解析器等长生命周期对象，
	 * 三元组配置 = 常规块 1024 / 保留水位 512 / 总上限 2048。
	 */
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

	/* 二进制提升：EndDup 与 EndStr 同构，面向字节块。 */
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

	/*
	 * Secure 系：先把用户区字节安全擦除（SecureZero 级），
	 *   再执行普通 Reset/Unit——arena 里待过密钥/令牌时用这组。
	 * */
	tScope = xrtTempBegin(&tArena);
	(void)xrtTempAlloc(&tArena, 32);
	if ( !xrtTempEnd(&tScope) ) {
		xrtTempUnit(&tArena);
		return 12;
	}
	if ( !xrtTempSecureReset(&tArena) ) {
		xrtTempUnit(&tArena);
		return 13;
	}
	if ( !xrtTempReset(&tArena) || !xrtTempTrim(&tArena, 0) ) {
		return 14;
	}
	xrtTempSecureUnit(&tArena);
	return 0;
}
