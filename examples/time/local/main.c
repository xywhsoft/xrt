/*
 * 范例：time/local —— 系统本地时区：分解、往返与 DST fold 策略
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTimeLocal      按系统时区分解（字段含 Offset/IsDST）
 *   xrtTimeFromLocal  本地字段 → xtime（须声明 fold 策略）
 *   XTIME_FOLD_EARLIER 夏令时重叠期取较早的那一次解释
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows，东八区运行）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/time/local/main.c -lws2_32 -liphlpapi
 * 预期输出（东八区无 DST，offset=+28800，dst=-1 表示不适用）：
 *   local=2026-09-05 10:02:40.356413 offset=+28800 dst=-1
 *   roundtrip=yes
 *
 * fold 是什么：夏令时回拨时本地时刻出现两次（如 2:30×2），
 *   FromLocal 必须声明取"较早/较晚"那个解释——
 *   这是 time.h 结构体 tm 表达不了的语义，XRT 显式建模。
 *   无 DST 的时区（如东八区）两种 fold 结果相同，往返恒等。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xtime iNow = xrtNow();
	xtime iRoundtrip;
	xdatetime tLocal;

	/*
	 * 本地分解：Offset 给出秒偏移（+28800 = UTC+8），
	 * IsDST 三态：1 夏令时 / 0 标准时间 / -1 该时区不适用。
	 */
	if ( !xrtTimeLocal(iNow, &tLocal) ||
		 !xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iRoundtrip) ) {
		return 1;
	}
	printf("local=%lld-%02d-%02d %02d:%02d:%02d.%06d offset=%+d dst=%d\n",
		(long long)tLocal.Year, tLocal.Month, tLocal.Day,
		tLocal.Hour, tLocal.Minute, tLocal.Second, tLocal.Microsecond,
		tLocal.Offset, tLocal.IsDST);

	/* 往返校验：本地字段 → 绝对时刻 → 应回到原值。 */
	printf("roundtrip=%s\n", iRoundtrip == iNow ? "yes" : "no");
	return iRoundtrip == iNow ? 0 : 1;
}
