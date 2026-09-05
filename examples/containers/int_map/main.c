/*
 * 范例：containers/int_map —— 整数键映射：稀疏键、零初始化槽与键序迭代
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtIntMapInit        按值大小初始化（键固定为 int64）
 *   xrtIntMapGetOrAdd    取值槽；键不存在则零初始化插入
 *   xrtIntMapIterBegin/Next/End  按"键升序"迭代
 * 模块宏：XRT_MODULE_MAP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/int_map/main.c -lws2_32 -liphlpapi
 * 预期输出（键升序：-9 < 1000001，与插入顺序相反）：
 *   session=-9 requests=3 authenticated=no
 *   session=1000001 requests=1 authenticated=yes
 *
 * 与字节键 xmap 的区别：
 *   - 键是原生 int64（无哈希计算，直接位映射），更快；
 *   - 支持任意稀疏/负数键（本例 -9 与 1000001），
 *     连续小键场景它是最快的"稀疏数组"；
 *   - 迭代顺序是键序（非插入序）——适合"按 ID 顺序导出"类需求。
 */

#include <stdio.h>
#include <xrt.h>



/* 稀疏会话状态直接保存在整数键对应的内联值槽中。 */
typedef struct sessionstate {
	unsigned Requests;
	bool Authenticated;
} sessionstate;



int main(void)
{
	xintmap tSessions;
	xintmapiter tIterator;
	sessionstate* pState;
	int64 iSessionId;       /* 借用：迭代时交出的键 */
	bool bNew;

	if ( !xrtIntMapInit(&tSessions, sizeof(sessionstate)) ) {
		return 1;
	}

	/*
	 * 会话 1000001 首次访问：新槽零初始化（bNew=true），
	 * 直接累加、置认证标记——无需预检查三步曲。
	 */
	pState = (sessionstate*)xrtIntMapGetOrAdd(&tSessions, 1000001, &bNew);
	if ( pState == NULL ) {
		xrtIntMapUnit(&tSessions);
		return 2;
	}
	pState->Requests++;
	pState->Authenticated = true;

	/* 负数键同样合法；新槽 Authenticated 保持零值 false。 */
	pState = (sessionstate*)xrtIntMapGetOrAdd(&tSessions, -9, &bNew);
	if ( pState == NULL ) {
		xrtIntMapUnit(&tSessions);
		return 3;
	}
	pState->Requests = 3;

	/*
	 * 键序迭代：输出 -9 在前、1000001 在后（与插入顺序无关）。
	 * IterNext 同时交出键与值指针，遍历即导出。
	 */
	if ( !xrtIntMapIterBegin(&tSessions, &tIterator) ) {
		xrtIntMapUnit(&tSessions);
		return 4;
	}
	while ( (pState = (sessionstate*)xrtIntMapIterNext(&tIterator, &iSessionId)) != NULL ) {
		printf(
			"session=%lld requests=%u authenticated=%s\n",
			(long long)iSessionId,
			pState->Requests,
			pState->Authenticated ? "yes" : "no"
		);
	}
	xrtIntMapIterEnd(&tIterator);
	xrtIntMapUnit(&tSessions);
	return 0;
}
