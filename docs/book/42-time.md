---
num: 42
slug: time
title: 时间与时区
volume: 卷五 系统服务
type: practice
lead: 公元毫秒表示事件，double 单调秒测量耗时；日历、Unix 与时区转换保持明确边界。
api: time, math
---

## 时间表示

`xtime` 是公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。公元前用负年份，没有零年。`xdatetime.Millisecond` 保存 0 到 999 的毫秒；日历字段与固定偏移转换覆盖全部 int64 时间范围。

`xrtTimer()` 是高精度单调计时器的 double 秒，读数原点没有日历意义。两次相减测耗时，日历和 Unix 转换使用 xtime。

```c
xtime Event = xrtNow();
double Start = xrtTimer();
xrtSleep(10);
double ElapsedSeconds = xrtTimer() - Start;
```

## 日期运算

`xrtTimeAdd` 的月、季度和年按日历进位，月末钳制。`xrtDateDiff` 获取指定维度的整数差；固定单位向零截断，年月按连续日历序号计算。跨公元前后不会计算不存在的零年。

```c
xtime Start, End;
int64 Months;
xrtDate(2024, 1, 31, &Start);
xrtDate(2024, 2, 1, &End);
xrtDateDiff(Start, End, XTIME_UNIT_MONTH, &Months); /* 1 */
```

## Unix、时区和文本

Unix 32 位秒、64 位秒、64 位毫秒都有双向转换，目标溢出时返回失败并保持输出。`xrtTimeSplitAt` 使用显式 UTC 偏移秒；本地时区层明确处理 DST gap/fold。文本层的 `%f` 为三位毫秒，RFC 3339 多余小数尾数会被截断。

完整接口及边界契约见 [Time API](../api/time.md) 和 [Wait API](../api/wait.md)。
