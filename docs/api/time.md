# Time API

时间库使用两种表示：`xtime` 保存公元 UTC 毫秒；`xrtTimer()` 返回用于测量经过时间的 `double` 秒。`xdatetime` 提供日历字段。等待接口接收相对毫秒，不提供独立的截止时间类型。

## 表示与纪年

`xtime` 是有符号 `int64`，零点为公元 1 年 1 月 1 日 00:00:00.000 UTC。`xtime == -1` 是公元前 1 年 12 月 31 日 23:59:59.999。年份使用 `… -2, -1, 1, 2 …`，没有零年；负数的绝对值是公元前年份。日历按外推 Gregorian 规则计算，公元前 1 年对应天文纪年的零年。

全部毫秒值从 `INT64_MIN` 到 `INT64_MAX` 都能按 UTC 或固定偏移分解、原样重建，跨度约为零点前后各 2.92 亿年。UTC 偏移使用以东为正的秒数，范围为 -86399 到 86399。系统本地时区、文件系统和文本协议有各自的可表示范围；超出范围会报告错误。

`xdatetime.Millisecond` 的范围为 0 到 999；其余字段为 `Year`、`Month`、`Day`、`Hour`、`Minute`、`Second`、`Offset`、`Weekday`、`YearDay`、`IsDST`。星期日为 `XTIME_SUNDAY == 0`，年内日从 1 开始，`IsDST` 为 1、0 或未知值 -1。

| 常量 | 毫秒数 |
|---|---:|
| `XRT_TIME_MILLISECOND` | 1 |
| `XRT_TIME_SECOND` | 1000 |
| `XRT_TIME_MINUTE` | 60000 |
| `XRT_TIME_HOUR` | 3600000 |
| `XRT_TIME_DAY` | 86400000 |
| `XRT_TIME_WEEK` | 604800000 |
| `XRT_TIME_UNIX_EPOCH` | 62135596800000 |

## 计时与等待

```c
double Start = xrtTimer();
xrtSleep(10);                        /* 相对毫秒 */
double Seconds = xrtTimer() - Start;  /* double 秒 */
xtime EventTime = xrtNow();           /* 公元 UTC 毫秒 */
```

Timer 使用平台高精度单调时钟。读数的原点没有日历意义，只用两次读数相减；系统日历校时不改变它。`xrtTimer` 读取失败返回 NaN 并报告错误。`xrtNow` 用于记录事件；它会受到系统日历校时影响。`xrtSleep(0)` 让出执行机会，负数睡眠参数无效。

[Wait API](wait.md) 使用 `int64` 相对毫秒，`XRT_WAIT_FOREVER == -1` 表示无限等待，0 表示不阻塞，其余负值是参数错误。跨步骤共享预算由库内部处理。

## Unix 双向转换

| 格式 | 从 Unix 构造 | 转成 Unix |
|---|---|---|
| 有符号 32 位秒 | `xrtTimeFromUnix32` | `xrtTimeToUnix32` |
| 有符号 64 位秒 | `xrtTimeFromUnix` | `xrtTimeUnix` |
| 有符号 64 位毫秒 | `xrtTimeFromUnixMs` | `xrtTimeToUnixMs` |

Unix 零点仍是 1970-01-01 UTC。秒转换对负绝对时间向负无穷取整：Unix 零点前 1 毫秒转成 Unix 秒是 -1。`xrtTimeUnix` 的结果对全部 `xtime` 都可表示；其余双向转换检查目标范围。失败不会修改调用方的输出，包括 Unix 32 位溢出和 Unix 毫秒减去纪元偏移后的溢出。

```c
xtime Moment;
int32 Unix32;
int64 UnixMs;
xrtTimeFromUnix32(0, &Moment);   /* Moment == XRT_TIME_UNIX_EPOCH */
xrtTimeToUnix32(Moment, &Unix32);
xrtTimeToUnixMs(Moment, &UnixMs);
```

## 日期计算

`xrtTimeAdd` 以 `xtimeunit` 指定单位。毫秒、秒、分钟、小时、日、周是固定长度；月、季度、年按日历进位，月末钳制到目标月份末日，例如 2024-01-31 加一个月为 2024-02-29。跨公元前后会跳过不存在的零年。

`xrtDateDiff(Start, End, Unit, &Result)` 返回指定维度的 `int64` 差值，不生成浮点时长：

- 固定单位是经过的毫秒差除以对应单位，向零截断；23 小时差按日计算为 0。
- 月差按连续月份序号计算，2024-01-31 到 2024-02-01 为 1，反向为 -1。
- 年差按连续年份序号计算，公元前 1 年到公元 1 年为 1。
- 季度差为月差除以 3，向零截断。

单位枚举为 `XTIME_UNIT_MILLISECOND`、`XTIME_UNIT_SECOND`、`XTIME_UNIT_MINUTE`、`XTIME_UNIT_HOUR`、`XTIME_UNIT_DAY`、`XTIME_UNIT_WEEK`、`XTIME_UNIT_MONTH`、`XTIME_UNIT_QUARTER`、`XTIME_UNIT_YEAR`。

计算检查最终结果，避免中间乘法溢出错误拒绝可表示结果。两端毫秒差可能超出 `int64`；此时 `xrtDateDiff` 返回 false，输出保持原值。原始 `End - Start` 只有在差值可表示时才能直接使用 C 有符号减法。

`xrtTimePart` 返回日内毫秒；`xrtDatePart` 返回当日 UTC 零点。极端时间的零点可能不可表示，此时后者返回 0 并报告 `XTIME_ERROR_OVERFLOW`。月、年、周区间是 `[Start, End)`；ISO 周编号使用星期一为 1、星期日为 7。

## 本地时区与文本

`time_local` 根据操作系统历史时区规则转换。DST gap 报告 `XTIME_ERROR_LOCAL_GAP`；fold 通过 `XTIME_FOLD_REJECT`、`XTIME_FOLD_EARLIER`、`XTIME_FOLD_LATER` 明确选择。固定偏移转换不猜测 DST。

`time_text` 的 `%f` 写入或解析三位毫秒。RFC 3339 可以接受更多小数位，丢弃毫秒以下尾数，写入时删除小数尾零；协议中的四位年份 0000 映射到公元前 1 年。HTTP-date 使用 GMT 和秒精度，支持正公元四位年份。通用格式按公元纪年拒绝零年。`xrtTimeTryParseHTTPDate` 失败时同时保留输出和已有线程错误。

格式化支持 `%Y %y %m %d %e %H %I %M %S %f %p %P %a %A %b %B %w %j %q %z %F %T %R %%` 和支持的无填充数字字段，例如 `%-m`。缓冲写入返回所需长度；拥有型字符串由 `xrtFree` 释放。

## 裁剪与错误

`XRT_FEATURE_TIME` 提供核心；`XRT_FEATURE_TIME_LOCAL` 和 `XRT_FEATURE_TIME_TEXT` 独立依赖核心。稳定错误为 `XTIME_ERROR_RANGE`、`XTIME_ERROR_OVERFLOW`、`XTIME_ERROR_FORMAT`、`XTIME_ERROR_PARSE`、`XTIME_ERROR_LOCAL_GAP`、`XTIME_ERROR_LOCAL_FOLD`、`XTIME_ERROR_LOCAL_UNSUPPORTED`，枚举类型为 `xtimeerror`、`xtimeweekday`、`xtimefold`。

## 完整函数声明

以下声明以 [time.h](../../include/xrt/time.h) 为准。

```c
XRT_API double xrtTimer(void);
```

```c
XRT_API xtime xrtNow(void);
```

```c
XRT_API void xrtSleep(int64 iMilliseconds);
```

```c
XRT_API bool xrtIsLeapYear(int64 iYear);
```

```c
XRT_API int xrtDaysInMonth(int64 iYear, int iMonth);
```

```c
XRT_API int xrtDaysInYear(int64 iYear);
```

```c
XRT_API bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime);
```

```c
XRT_API bool xrtDateTime(int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMillisecond, xtime* pTime);
```

```c
XRT_API bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime);
```

```c
XRT_API bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime);
```

```c
XRT_API bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime);
```

```c
XRT_API bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime);
```

```c
XRT_API bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime);
```

```c
XRT_API int64 xrtTimeUnix(xtime iTime);
```

```c
XRT_API bool xrtTimeToUnixMs(xtime iTime, int64* pMilliseconds);
```

```c
XRT_API bool xrtTimeFromUnix32(int32 iSeconds, xtime* pTime);
```

```c
XRT_API bool xrtTimeToUnix32(xtime iTime, int32* pSeconds);
```

```c
XRT_API int64 xrtYear(xtime iTime);
```

```c
XRT_API int xrtMonth(xtime iTime);
```

```c
XRT_API int xrtDay(xtime iTime);
```

```c
XRT_API int xrtHour(xtime iTime);
```

```c
XRT_API int xrtMinute(xtime iTime);
```

```c
XRT_API int xrtSecond(xtime iTime);
```

```c
XRT_API int xrtMillisecond(xtime iTime);
```

```c
XRT_API int xrtWeekday(xtime iTime);
```

```c
XRT_API int xrtDayOfYear(xtime iTime);
```

```c
XRT_API int xrtQuarter(xtime iTime);
```

```c
XRT_API xtime xrtDatePart(xtime iTime);
```

```c
XRT_API xtime xrtTimePart(xtime iTime);
```

```c
XRT_API bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance);
```

```c
XRT_API bool xrtTimeSameDay(xtime iLeft, xtime iRight);
```

```c
XRT_API bool xrtTimeSameMonth(xtime iLeft, xtime iRight);
```

```c
XRT_API bool xrtTimeSameYear(xtime iLeft, xtime iRight);
```

```c
XRT_API bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd);
```

```c
XRT_API bool xrtTimeOverlap(xtime iStart1, xtime iEnd1,
	xtime iStart2, xtime iEnd2);
```

```c
XRT_API bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult);
```

```c
XRT_API bool xrtDateDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult);
```

```c
XRT_API bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd);
```

```c
XRT_API bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd);
```

```c
XRT_API bool xrtWeekRange(xtime iTime, int iFirstWeekday, xtime* pStart, xtime* pEnd);
```

```c
XRT_API bool xrtISOWeek(xtime iTime, int64* pWeekYear, int* pWeek, int* pWeekday);
```

```c
XRT_API bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime);
```

```c
XRT_API bool xrtTimeFromLocal(const xdatetime* pDateTime, xtimefold Fold, xtime* pTime);
```

```c
XRT_API size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity,
	const xdatetime* pDateTime, xstrview Format);
```

```c
XRT_API str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format);
```

```c
XRT_API bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime);
```

```c
XRT_API size_t xrtTimeWrite(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset, xstrview Format);
```

```c
XRT_API str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format);
```

```c
XRT_API bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime);
```

```c
XRT_API size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset);
```

```c
XRT_API str xrtTimeRFC3339(xtime iTime, int iOffset);
```

```c
XRT_API bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime);
```

```c
XRT_API size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime);
```

```c
XRT_API str xrtTimeHTTPDate(xtime iTime);
```

```c
XRT_API bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime);
```

```c
XRT_API bool xrtTimeTryParseHTTPDate(xstrview Text, xtime* pTime);
```

```c
XRT_API bool xrtTimeParseAny(xstrview Text, xtime* pTime);
```

星期枚举按 `XTIME_SUNDAY`、`XTIME_MONDAY`、`XTIME_TUESDAY`、`XTIME_WEDNESDAY`、`XTIME_THURSDAY`、`XTIME_FRIDAY`、`XTIME_SATURDAY` 排列，取值 0 至 6。
