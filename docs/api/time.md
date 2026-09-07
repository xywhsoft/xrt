# Time API

## 设计契约

时间体系分成三个可独立裁剪的层次：

- `XRT_FEATURE_TIME`：Unix Epoch 微秒、单调时钟、Gregorian 日历、显式偏移和日历计算。
- `XRT_FEATURE_TIME_LOCAL`：操作系统本地时区、历史 DST 规则、gap/fold 处理。
- `XRT_FEATURE_TIME_TEXT`：无固定缓冲限制的格式化、严格解析、RFC 3339 和 HTTP-date。

后两层直接依赖 `XRT_FEATURE_TIME`，互不依赖。协议库只需要日期文本时可以启用 `time_text` 而不引入本地时区代码。

`xtime` 是有符号 64 位 Unix Epoch 微秒。它既表示绝对时间，也可表示明确以微秒计量的固定时长；不携带时区、locale 或日历对象。全部转换覆盖 `INT64_MIN` 到 `INT64_MAX`，失败不会修改输出参数。

## 常量与类型

```c
#define XRT_TIME_MICROSECOND  1
#define XRT_TIME_MILLISECOND  1000
#define XRT_TIME_SECOND       1000000
#define XRT_TIME_MINUTE       60000000
#define XRT_TIME_HOUR         3600000000
#define XRT_TIME_DAY          86400000000
#define XRT_TIME_WEEK         604800000000
```

固定时长统一使用整数微秒，不使用浮点数隐式表达单位。

```c
typedef struct xdatetime {
	int64 Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;
	int Microsecond;
	int Offset;
	int Weekday;
	int YearDay;
	int IsDST;
} xdatetime;
```

- `Year` 使用 Gregorian 天文纪年，支持零年和负年份。
- `Offset` 是 UTC 以东秒数，范围 `-23:59:59` 到 `+23:59:59`。
- `Weekday` 从星期日 `XTIME_SUNDAY == 0` 到星期六。
- `YearDay` 从 1 到 365 或 366。
- `IsDST` 为 1、0 或未知值 -1；固定偏移分解不猜测 DST。

`xtimeunit` 定义微秒到周的固定时长单位，以及月、季度、年的日历单位。`xtimefold` 定义重复本地时间的 `REJECT`、`EARLIER`、`LATER` 选择。

### `xtimeweekday`

星期值固定从星期日零开始，便于和 C/POSIX 及 HTTP-date 对接。

```c
typedef enum xtimeweekday {
	XTIME_SUNDAY = 0,
	XTIME_MONDAY,
	XTIME_TUESDAY,
	XTIME_WEDNESDAY,
	XTIME_THURSDAY,
	XTIME_FRIDAY,
	XTIME_SATURDAY
} xtimeweekday;
```

| 值 | 语义 |
|---|---|
| `XTIME_SUNDAY` | SUNDAY |
| `XTIME_MONDAY` | MONDAY |
| `XTIME_TUESDAY` | TUESDAY |
| `XTIME_WEDNESDAY` | WEDNESDAY |
| `XTIME_THURSDAY` | THURSDAY |
| `XTIME_FRIDAY` | FRIDAY |

### `xtimeunit`

日期计算单位；月、季度和年使用日历语义，其余单位使用固定时长。

```c
typedef enum xtimeunit {
	XTIME_UNIT_MICROSECOND = 0,
	XTIME_UNIT_MILLISECOND,
	XTIME_UNIT_SECOND,
	XTIME_UNIT_MINUTE,
	XTIME_UNIT_HOUR,
	XTIME_UNIT_DAY,
	XTIME_UNIT_WEEK,
	XTIME_UNIT_MONTH,
	XTIME_UNIT_QUARTER,
	XTIME_UNIT_YEAR
} xtimeunit;
```

| 值 | 语义 |
|---|---|
| `XTIME_UNIT_MICROSECOND` | MICROSECOND |
| `XTIME_UNIT_MILLISECOND` | MILLISECOND |
| `XTIME_UNIT_SECOND` | SECOND |
| `XTIME_UNIT_MINUTE` | MINUTE |
| `XTIME_UNIT_HOUR` | HOUR |
| `XTIME_UNIT_DAY` | DAY |
| `XTIME_UNIT_WEEK` | WEEK |
| `XTIME_UNIT_MONTH` | MONTH |
| `XTIME_UNIT_QUARTER` | QUARTER |

### `xtimefold`

本地时间在夏令时回拨区间出现两个候选值时的选择规则。

```c
typedef enum xtimefold {
	XTIME_FOLD_REJECT = 0,
	XTIME_FOLD_EARLIER,
	XTIME_FOLD_LATER
} xtimefold;
```

| 值 | 语义 |
|---|---|
| `XTIME_FOLD_REJECT` | REJECT |
| `XTIME_FOLD_EARLIER` | EARLIER |

### `xtimeerror`

时间模块稳定错误代码。

```c
typedef enum xtimeerror {
	XTIME_ERROR_RANGE = 1,
	XTIME_ERROR_OVERFLOW,
	XTIME_ERROR_FORMAT,
	XTIME_ERROR_PARSE,
	XTIME_ERROR_LOCAL_GAP,
	XTIME_ERROR_LOCAL_FOLD,
	XTIME_ERROR_LOCAL_UNSUPPORTED
} xtimeerror;
```

| 值 | 语义 |
|---|---|
| `XTIME_ERROR_RANGE` | 范围越界 |
| `XTIME_ERROR_OVERFLOW` | 溢出 |
| `XTIME_ERROR_FORMAT` | FORMAT |
| `XTIME_ERROR_PARSE` | PARSE |
| `XTIME_ERROR_LOCAL_GAP` | LOCALGAP |
| `XTIME_ERROR_LOCAL_FOLD` | LOCALFOLD |

### `xdatetime`

分解后的 Gregorian 日期时间；Offset 为 UTC 以东秒数。

```c
typedef struct xdatetime {
	int64 Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;
	int Microsecond;
	int Offset;
	int Weekday;
	int YearDay;
	int IsDST;
} xdatetime;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Year` | `int64` | Year |
| `Month` | `int` | Month |
| `Day` | `int` | Day |
| `Hour` | `int` | Hour |
| `Minute` | `int` | Minute |
| `Second` | `int` | Second |
| `Microsecond` | `int` | Microsecond |
| `Offset` | `int` | Offset |
| `Weekday` | `int` | Weekday |
| `YearDay` | `int` | YearDay |
| `IsDST` | `int` | IsDST |

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_TIME_MICROSECOND` | `INT64_C(1)` | xtime 和固定时长统一使用微秒，避免浮点计时和隐式单位换算。 |
| `XRT_TIME_MILLISECOND` | `INT64_C(1000)` | MILLISECOND |
| `XRT_TIME_SECOND` | `INT64_C(1000000)` | SECOND |
| `XRT_TIME_MINUTE` | `INT64_C(60000000)` | MINUTE |
| `XRT_TIME_HOUR` | `INT64_C(3600000000)` | HOUR |
| `XRT_TIME_DAY` | `INT64_C(86400000000)` | DAY |
| `XRT_TIME_WEEK` | `INT64_C(604800000000)` | WEEK |

## 时钟与休眠

```c
uint64 xrtClock(void);
double xrtTimer(void);
xtime xrtNow(void);
void xrtSleep(uint32 iMilliseconds);
void xrtSleepUs(uint64 iMicroseconds);
void xrtSleepUntil(uint64 iDeadline);
```

`xrtClock` 返回单调递增微秒，只用于测量间隔和截止时间，不可转换为日期。`xrtTimer` 是保留旧版使用手感的浮点秒便捷接口。`xrtNow` 返回墙钟 Unix 微秒。

`xrtSleepUntil` 接受 `xrtClock` 域中的截止点；已到期立即返回。休眠保证不主动早于请求时间返回，但操作系统调度可能使实际时间更长。

### `xrtClock`

返回单调递增的微秒时钟，用于测量时间间隔。

```c
uint64 xrtClock(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 单调微秒计数 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[clock](../../examples/time/clock/main.c) · 单调时钟

```c
	uint64 iStart = xrtClock();
```

### `xrtNow`

返回当前墙钟时间的 Unix 微秒表示。

```c
xtime xrtNow(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 时间值 | Unix Epoch 微秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[basic](../../examples/time/basic/main.c) · 当前时间

```c
	xtime iNow = xrtNow();
```

### `xrtTimer`

返回高精度秒计时器，用于基准测量。

```c
double xrtTimer(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| double | 秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[clock](../../examples/time/clock/main.c) · 高精度计时器

```c
	double fStart = xrtTimer();
```

### `xrtSleep`

休眠指定毫秒数。

```c
void xrtSleep(uint32 iMilliseconds)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMilliseconds` | 输入 | — | 毫秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已休眠 | — |

#### 错误

- 无 — 休眠不失败

#### 范例

[clock](../../examples/time/clock/main.c) · 毫秒休眠

```c
	xrtSleep(10);
```

### `xrtSleepUs`

休眠指定微秒数。

```c
void xrtSleepUs(uint64 iMicroseconds)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMicroseconds` | 输入 | — | 微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已休眠 | — |

#### 错误

- 无 — 休眠不失败

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 微秒休眠

```c
	xrtSleepUs(20000u);
```

### `xrtSleepUntil`

休眠到单调时钟截止时间。

```c
void xrtSleepUntil(uint64 iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已休眠 | — |

#### 错误

- 无 — 休眠不失败

#### 范例

[deadline](../../examples/concurrency/deadline/main.c) · 限期休眠

```c
	xrtSleepUntil(iDeadline);
```

## Gregorian 日历

```c
bool xrtIsLeapYear(int64 iYear);
int xrtDaysInMonth(int64 iYear, int iMonth);
int xrtDaysInYear(int64 iYear);

bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime);
bool xrtDateTime(int64 iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond, xtime* pTime);
bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime);
bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime);
bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime);
```

`xrtDate` 和 `xrtDateTime` 构造 UTC 时间。`xrtTimeMake` 按结构中的显式 `Offset` 把本地字段转换为绝对时间。`xrtTimeSplitAt` 使用调用方提供的固定偏移分解，不读取系统时区。

所有构造函数严格拒绝非法日期，不自动把 2 月 30 日滚动到下一月。秒范围是 0 到 59；当前契约不内建闰秒表。

### `xrtIsLeapYear`

判断公历年份是否为闰年。

```c
bool xrtIsLeapYear(int64 iYear)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iYear` | 输入 | — | 年份 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否闰年 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 闰年判断

```c
		xrtIsLeapYear(2000) ? 1 : 0,
```

### `xrtDaysInMonth`

返回指定月份的天数。

```c
int xrtDaysInMonth(int64 iYear, int iMonth)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iYear` | 输入 | — | 年份 |
| `iMonth` | 输入 | 1–12 | 月份 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 28–31 | 当月天数 | — |
| `0` | 月份越界 | `XERR_RANGE` |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 月天数

```c
		xrtDaysInMonth(2024, 2), xrtDaysInMonth(2023, 2));
```

### `xrtDaysInYear`

返回指定年份的天数（365 或 366）。

```c
int xrtDaysInYear(int64 iYear)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iYear` | 输入 | — | 年份 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 365 / 366 | 当年天数 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 年天数

```c
	printf(" year=%d\n", xrtDaysInYear(2024));
```

### `xrtDate`

从年月日构造当日零点的 `xtime`。

```c
bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iYear` | 输入 | — | 年份 |
| `iMonth` | 输入 | 1–12 | 月份 |
| `iDay` | 输入 | 1–31 | 日 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 日期/时间字段组合非法（如 2 月 30 日）

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 构造日期

```c
		if ( !xrtDate(2023, 1, 1, &NewYear2023) ||
			 !xrtISOWeek(NewYear2023, &iWeekYear, &iWeek,
				&iWeekday) ||
			 (iWeekYear != 2022) || (iWeek != 52) ||
			 (iWeekday != 7) ) {
```

### `xrtDateTime`

从完整日期时间字段构造 `xtime`。

```c
bool xrtDateTime(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond, int iMicrosecond, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iYear` | 输入 | — | 年份 |
| `iMonth` | 输入 | 1–12 | 月份 |
| `iDay` | 输入 | 1–31 | 日 |
| `iHour` | 输入 | 0–23 | 时 |
| `iMinute` | 输入 | 0–59 | 分 |
| `iSecond` | 输入 | 0–59 | 秒 |
| `iMicrosecond` | 输入 | — | 微秒 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 日期/时间字段组合非法（如 2 月 30 日）

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 构造时刻

```c
	if ( !xrtDateTime(2024, 3, 10, 12, 34, 56, 789012, &Moment) ) {
```

### `xrtTimeMake`

按 `xdatetime` 字段构造 `xtime`（UTC 语义）。

```c
bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDateTime` | 输入 | 非空 | 字段结构 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 日期/时间字段组合非法（如 2 月 30 日）

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 按字段构造

```c
		if ( !xrtTimeMake(&Parts, &Made) || (Made != Moment) ) {
```

### `xrtWeekday`

返回星期几（0 = 周日）。

```c
int xrtWeekday(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 0–6 | 星期几 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 星期

```c
		 (xrtDay(Moment) != 10) || (xrtWeekday(Moment) != 0) ||
```

### `xrtDayOfYear`

返回年内第几天（1 起）。

```c
int xrtDayOfYear(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 1–366 | 年内天数 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 年内天数

```c
	printf(" yday=%d q=%d\n", xrtDayOfYear(Moment), xrtQuarter(Moment));
```

### `xrtISOWeek`

返回 ISO 8601 周历年、周号和星期。

```c
bool xrtISOWeek(xtime iTime, int64* pWeekYear, int* pWeek, int* pWeekday)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `pWeekYear` | 输出 | 允许空 | 接收周历年 |
| `pWeek` | 输出 | 允许空 | 接收周号 |
| `pWeekday` | 输出 | 允许空 | 接收星期 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · ISO 周号

```c
	if ( !xrtISOWeek(Moment, &iWeekYear, &iWeek, &iWeekday) ||
		 (iWeekYear != 2024) || (iWeek != 10) || (iWeekday != 7) ) {
```

## Unix 单位转换

```c
bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime);
bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime);
int64 xrtTimeUnix(xtime iTime);
int64 xrtTimeUnixMs(xtime iTime);
```

缩小精度时向负无穷取整，因此 `xrtTimeUnix(-1) == -1`，不会把 Epoch 前一微秒错误转换为零。放大精度发生溢出时返回 false。

### `xrtTimeFromUnix`

把 Unix 秒转换为 `xtime`。

```c
bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSeconds` | 输入 | — | Unix 秒 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 秒转时间

```c
	(void)xrtTimeFromUnix(xrtTimeUnix(Moment), &FromS);
```

### `xrtTimeFromUnixMs`

把 Unix 毫秒转换为 `xtime`。

```c
bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMilliseconds` | 输入 | — | Unix 毫秒 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 毫秒转时间

```c
	if ( !xrtTimeFromUnixMs(EXAMPLE_EPOCH_MS, &FromMs) ||
		 (xrtTimeUnixMs(FromMs) != EXAMPLE_EPOCH_MS) ||
		 (xrtTimeUnixMs(Moment) != EXAMPLE_EPOCH_MS) ) {
```

### `xrtTimeUnix`

把 `xtime` 转换为 Unix 秒。

```c
int64 xrtTimeUnix(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| Unix 秒 | 截断到秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 时间转秒

```c
	(void)xrtTimeFromUnix(xrtTimeUnix(Moment), &FromS);
```

### `xrtTimeUnixMs`

把 `xtime` 转换为 Unix 毫秒。

```c
int64 xrtTimeUnixMs(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| Unix 毫秒 | 截断到毫秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 时间转毫秒

```c
		(long long)xrtTimeUnixMs(Midnight) / 1000);
```

## 字段提取

```c
int64 xrtYear(xtime iTime);
int xrtMonth(xtime iTime);
int xrtDay(xtime iTime);
int xrtHour(xtime iTime);
int xrtMinute(xtime iTime);
int xrtSecond(xtime iTime);
int xrtMicrosecond(xtime iTime);
int xrtWeekday(xtime iTime);
int xrtDayOfYear(xtime iTime);
int xrtQuarter(xtime iTime);
xtime xrtDatePart(xtime iTime);
xtime xrtTimePart(xtime iTime);
```

字段提取使用 UTC。`xrtDatePart` 返回当日 UTC 零点；如果极端负值所在日期的零点超出 `xtime`，返回零并报告溢出。`xrtTimePart` 始终位于 `[0, XRT_TIME_DAY)`。

### `xrtTimeSplit`

把 `xtime` 拆分为 UTC `xdatetime` 字段。

```c
bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `pDateTime` | 输出 | 非空 | 接收字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/time/basic/main.c) · 拆分字段

```c
	if ( !xrtTimeSplit(iNow, &tUTC) ||
		 !xrtTimeSplitAt(iNow, 8 * 3600, &tLocalOffset) ||
		 !xrtTimeAdd(iNow, 1, XTIME_UNIT_MONTH, &iNextMonth) ) {
```

### `xrtTimeSplitAt`

把 `xtime` 拆分为指定 UTC 偏移下的字段。

```c
bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `iOffset` | 输入 | — | UTC 偏移分钟 |
| `pDateTime` | 输出 | 非空 | 接收字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[basic](../../examples/time/basic/main.c) · 按偏移拆分

```c
		 !xrtTimeSplitAt(iNow, 8 * 3600, &tLocalOffset) ||
```

### `xrtYear`

提取年份数值。

```c
int64 xrtYear(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 年份 | 公历年 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 年

```c
		(long long)xrtYear(Moment),
```

### `xrtQuarter`

提取季度（1–4）。

```c
int xrtQuarter(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 1–4 | 季度 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 季度

```c
	printf(" yday=%d q=%d\n", xrtDayOfYear(Moment), xrtQuarter(Moment));
```

### `xrtMonth`

提取月份（1–12）。

```c
int xrtMonth(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 1–12 | 月份 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 月

```c
		xrtMonth(Moment), xrtDay(Moment),
```

### `xrtDay`

提取日（1–31）。

```c
int xrtDay(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 1–31 | 日 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 日

```c
		xrtMonth(Moment), xrtDay(Moment),
```

### `xrtHour`

提取小时（0–23）。

```c
int xrtHour(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 0–23 | 小时 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 时

```c
		xrtHour(Moment), xrtMinute(Moment),
```

### `xrtMinute`

提取分钟（0–59）。

```c
int xrtMinute(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 0–59 | 分钟 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 分

```c
		xrtHour(Moment), xrtMinute(Moment),
```

### `xrtSecond`

提取秒（0–59）。

```c
int xrtSecond(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 0–59 | 秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 秒

```c
		xrtSecond(Moment), xrtMicrosecond(Moment));
```

### `xrtMicrosecond`

提取秒内微秒（0–999999）。

```c
int xrtMicrosecond(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 0–999999 | 秒内微秒 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 微秒

```c
		xrtSecond(Moment), xrtMicrosecond(Moment));
```

### `xrtDatePart`

返回当日零点的 `xtime`。

```c
xtime xrtDatePart(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 时间值 | 截断到日 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 日期部分

```c
	Midnight = xrtDatePart(Moment);
```

### `xrtTimePart`

返回当日零点起的微秒偏移。

```c
xtime xrtTimePart(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 时间值 | 当日微秒偏移 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[calendar_tour](../../examples/time/calendar_tour/main.c) · 时间部分

```c
	printf(" time-part=%lld\n", (long long)xrtTimePart(Moment));
```

## 比较、算术与区间

```c
bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance);
bool xrtTimeSameDay(xtime iLeft, xtime iRight);
bool xrtTimeSameMonth(xtime iLeft, xtime iRight);
bool xrtTimeSameYear(xtime iLeft, xtime iRight);
bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd);
bool xrtTimeOverlap(xtime iStart1, xtime iEnd1,
	xtime iStart2, xtime iEnd2);

bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult);
bool xrtTimeDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult);
```

`xrtTimeNear` 的无符号容差覆盖完整 64 位差值。三个 `Same` Helper 按 UTC Gregorian 字段比较，并覆盖 `xtime` 完整范围。`xrtTimeIn` 和 `xrtTimeOverlap` 使用闭区间，反向区间无效。

微秒到周按固定时长计算。`xrtTimeDiff` 直接在无符号差值上缩放，两个端点即使跨越完整 `int64` 域，只要最终单位数可由 `int64` 表示就会成功；只有最终结果不可表示时才报告溢出。月、季度和年按 Gregorian 日历计算，目标月份较短时钳制到月末，例如 2024-01-31 加一个月得到 2024-02-29。`xrtTimeDiff` 返回从起点到终点已经完整经过的单位数，不把不足一个完整月的尾部计入。

```c
bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd);
bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd);
bool xrtWeekRange(xtime iTime, int iFirstWeekday,
	xtime* pStart, xtime* pEnd);
bool xrtISOWeek(xtime iTime, int64* pWeekYear,
	int* pWeek, int* pWeekday);
```

月、年、周范围使用半开区间 `[start, end)`。`xrtISOWeek` 返回 ISO 周年、1 到 53 的周数，以及星期一为 1、星期日为 7 的星期值。

### `xrtTimeAdd`

把指定单位的数值加到时间上。

```c
bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 基准时间 |
| `iValue` | 输入 | — | 增量 |
| `Unit` | 输入 | — | 增量单位 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 单位非法

#### 范例

[basic](../../examples/time/basic/main.c) · 加法

```c
		 !xrtTimeAdd(iNow, 1, XTIME_UNIT_MONTH, &iNextMonth) ) {
```

### `xrtTimeDiff`

计算两个时间在指定单位下的差值。

```c
bool xrtTimeDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iStart` | 输入 | — | 起始时间 |
| `iEnd` | 输入 | — | 结束时间 |
| `Unit` | 输入 | — | 差值单位 |
| `pResult` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 单位非法

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 差值

```c
	if ( !xrtTimeDiff(Base, Day10 + 10 * XRT_TIME_DAY,
		XTIME_UNIT_DAY, &iDiff) || (iDiff != 10) ) {
```

### `xrtTimeIn`

判断时间是否位于闭区间内。

```c
bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 待判断时间 |
| `iStart` | 输入 | — | 区间起点 |
| `iEnd` | 输入 | — | 区间终点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否在区间内 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 区间包含

```c
		xrtTimeIn(Day11, Base, Day11) ? 1 : 0,
```

### `xrtTimeNear`

判断两个时间差是否在容差内。

```c
bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLeft` | 输入 | — | 左时间 |
| `iRight` | 输入 | — | 右时间 |
| `iTolerance` | 输入 | — | 容差微秒 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否近似相等 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 近似相等

```c
		xrtTimeNear(Base, Base + 900, 1000u) ? 1 : 0,
```

### `xrtTimeOverlap`

判断两个闭区间是否重叠。

```c
bool xrtTimeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iStart1` | 输入 | — | 区间一起点 |
| `iEnd1` | 输入 | — | 区间一终点 |
| `iStart2` | 输入 | — | 区间二起点 |
| `iEnd2` | 输入 | — | 区间二终点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否重叠 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 区间重叠

```c
		xrtTimeOverlap(Mar1, Base, Mar1, Apr1) ? 1 : 0,
```

### `xrtTimeSameDay`

判断两个 UTC 时间是否同一天。

```c
bool xrtTimeSameDay(xtime iLeft, xtime iRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLeft` | 输入 | — | 左时间 |
| `iRight` | 输入 | — | 右时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否同日 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 同日判断

```c
		xrtTimeSameDay(Base, Base + 3600 * XRT_TIME_SECOND) ? 1 : 0,
```

### `xrtTimeSameMonth`

判断两个 UTC 时间是否同一月。

```c
bool xrtTimeSameMonth(xtime iLeft, xtime iRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLeft` | 输入 | — | 左时间 |
| `iRight` | 输入 | — | 右时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否同月 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 同月判断

```c
		xrtTimeSameMonth(Base, Mar1) ? 1 : 0,
```

### `xrtTimeSameYear`

判断两个 UTC 时间是否同一年。

```c
bool xrtTimeSameYear(xtime iLeft, xtime iRight)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLeft` | 输入 | — | 左时间 |
| `iRight` | 输入 | — | 右时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否同年 | — |

#### 错误

- 无 — 纯计算，不设置错误

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 同年判断

```c
		xrtTimeSameYear(Base, Jan1 + 100 * XRT_TIME_DAY) ? 1 : 0,
```

### `xrtYearRange`

返回所在年的闭区间。

```c
bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `pStart` | 输出 | 非空 | 接收起点 |
| `pEnd` | 输出 | 非空 | 接收终点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 年区间

```c
	if ( !xrtYearRange(Base, &Start, &End) ) {
```

### `xrtMonthRange`

返回所在月的闭区间。

```c
bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `pStart` | 输出 | 非空 | 接收起点 |
| `pEnd` | 输出 | 非空 | 接收终点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 月区间

```c
	if ( !xrtMonthRange(Base, &Start, &End) ) {
```

### `xrtWeekRange`

返回所在周的闭区间。

```c
bool xrtWeekRange(xtime iTime, int iFirstWeekday, xtime* pStart, xtime* pEnd)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `iFirstWeekday` | 输入 | — | 每周第一天（0 = 周日） |
| `pStart` | 输出 | 非空 | 接收起点 |
| `pEnd` | 输出 | 非空 | 接收终点 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[range_tour](../../examples/time/range_tour/main.c) · 周区间

```c
	if ( !xrtWeekRange(Base, 0, &Start, &End) ) {
```

## 本地时区

```c
bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime);
bool xrtTimeFromLocal(const xdatetime* pDateTime,
	xtimefold Fold, xtime* pTime);
```

本地时区函数使用操作系统对目标时刻适用的历史规则，而不是把“当前 UTC 偏移”套用到任意日期。`xrtTimeFromLocal` 会枚举并回验候选绝对时间：

- DST 跳进产生的不存在时间报告 `XTIME_ERROR_LOCAL_GAP`。
- DST 回拨产生两个候选时，`REJECT` 报告 `XTIME_ERROR_LOCAL_FOLD`。
- `EARLIER` 和 `LATER` 明确选择两个候选中的较早或较晚绝对时间。

固定偏移场景应使用 `xrtTimeMake` 和 `xrtTimeSplitAt`，避免不必要地依赖操作系统时区数据库。

`xrtTimeFromLocal` 只读取 `Year`、`Month`、`Day`、`Hour`、`Minute`、`Second` 和
`Microsecond` 墙钟字段；`Offset`、`Weekday`、`YearDay` 和 `IsDST` 是分解结果中的派生字段，
不会约束反解。函数的可表示年份由操作系统时区 API 决定：Windows 通常从 FILETIME 的
1601 年开始；采用 64 位 `time_t` 的 POSIX 平台可能覆盖完整 `xtime` 范围，但仍受具体 C 库
和时区数据库限制。范围外失败不会修改输出。
Windows 优先动态解析 `SystemTimeToTzSpecificLocalTimeEx`，让历史转换采用系统动态 DST
规则；旧系统缺少该入口时才回退到传统转换 API。

### `xrtTimeLocal`

把 `xtime` 拆分为本地时区 `xdatetime` 字段。

```c
bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `pDateTime` | 输出 | 非空 | 接收字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[local](../../examples/time/local/main.c) · 转本地字段

```c
	if ( !xrtTimeLocal(iNow, &tLocal) ||
		 !xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iRoundtrip) ) {
```

### `xrtTimeFromLocal`

把本地时区字段转换为 `xtime`；歧义或跳变时按折叠策略选择。

```c
bool xrtTimeFromLocal(const xdatetime* pDateTime, xtimefold Fold, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDateTime` | 输入 | 非空 | 本地字段 |
| `Fold` | 输入 | — | 夏令时折叠策略 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_RANGE` — 字段越界或结果超出可表示域
- `XERR_ARGUMENT` — 日期/时间字段组合非法（如 2 月 30 日）
- `XERR_RANGE` — 本地字段在跳变间隙内不可表示

#### 范例

[local](../../examples/time/local/main.c) · 本地字段转时间

```c
		 !xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iRoundtrip) ) {
```

## 自定义时间文本

```c
size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity,
	const xdatetime* pDateTime, xstrview Format);
str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format);
bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime);

size_t xrtTimeWrite(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset, xstrview Format);
str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format);
bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime);
```

`Write` 函数不分配内存，返回完整结果所需字节数，不包含结尾零。传入 `NULL, 0` 可以只查询长度；缓冲区不足时写入可容纳的前缀、保证零结尾并仍返回完整长度。格式或参数失败返回 `XRT_NPOS`，非空缓冲区会被清空。输出缓冲区与 `Format` 的字节范围不得重叠；该参数错误在任何写入前报告，原格式缓冲区保持不变。

`Format` 函数精确分配结果，返回值由 `xrtFree` 释放。实现直接扫描格式，不存在旧版 64 个占位符、256 字节结果等固定上限。

解析函数必须完整消费输入和格式，失败不修改输出。格式串自身包含未知或不完整占位符、非法修饰符或内嵌零时报告 `XTIME_ERROR_FORMAT`；合法格式与输入不匹配时报告 `XTIME_ERROR_PARSE`。未给出的日期默认为 `1970-01-01`，未给出的时间和偏移默认为零。名称使用固定英文 ASCII，不读取进程 locale；匹配英文名称时忽略 ASCII 大小写。

在月份、日期、小时、分钟和秒数字占位符前加入 `-` 会取消输出填充，例如 `%-m`、`%-d` 和 `%-H`。解析这些占位符时接受一到两位数字；不支持 `%-Y`、`%-f` 或对名称、组合占位符使用 `-`，避免不明确的宽度契约。

| 占位符 | 含义 |
| --- | --- |
| `%%` | 百分号 |
| `%Y` | 至少四位的完整有符号年份 |
| `%y` | 两位年份；解析映射到 2000 到 2099 |
| `%m` | 两位月份 |
| `%b` / `%B` | 英文短/长月份名 |
| `%d` / `%e` | 零填充/空格填充日期 |
| `%H` / `%I` | 24/12 小时制小时 |
| `%M` / `%S` | 分钟/秒 |
| `%f` | 六位微秒 |
| `%p` / `%P` | 大写/小写 `AM` 或 `PM`；解析时忽略大小写，必须和 `%I` 配合 |
| `%a` / `%A` | 英文短/长星期名 |
| `%w` | 星期日为零的星期数字 |
| `%j` | 三位年内日期 |
| `%q` | 季度 1 到 4 |
| `%z` / `%:z` | `+HHMM` / `+HH:MM` 固定偏移，解析也接受 `Z` |
| `%F` | `%Y-%m-%d`，支持完整有符号扩展年份 |
| `%T` | `%H:%M:%S` |
| `%R` | `%H:%M` |

格式化偏移时不会丢弃秒级信息；如果 `Offset` 不能被 `%z` 或 `%:z` 无损表达，函数报告范围错误。

### `xrtTimeFormat`

按自定义格式生成零结尾文本，`xrtFree` 释放。

```c
str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `iOffset` | 输入 | — | UTC 偏移分钟 |
| `Format` | 输入 | 借用 | 格式串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_ARGUMENT` — 格式串非法
- `XERR_MEMORY` — 分配失败

#### 范例

[format](../../examples/time/format/main.c) · 格式化

```c
	sAllocated = xrtTimeFormat(iTime, 0,
		XRT_STR_LITERAL("%A, %B %d, %Y"));
```

### `xrtTimeWrite`

按自定义格式写入调用方缓冲。

```c
size_t xrtTimeWrite(char* sBuffer, size_t iCapacity, xtime iTime, int iOffset, xstrview Format)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sBuffer` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `iTime` | 输入 | — | 时间值 |
| `iOffset` | 输入 | — | UTC 偏移分钟 |
| `Format` | 输入 | 借用 | 格式串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 写入字节数（不含零） | — |
| `0` | 容量不足或格式非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[file_report](../../examples/file/report/main.c) · 格式化到缓冲

```c
	iNameSize = xrtTimeWrite(
		arrFileName,
		sizeof(arrFileName),
		iNow,
		0,
		XRT_STR_LITERAL("report_%Y%m%d_%H%M%S_%f.txt")
	);
```

### `xrtTimeParse`

按自定义格式严格解析为 `xtime`。

```c
bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Format` | 输入 | 借用 | 格式串 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 文本与格式不匹配、字段越界或存在未消费字符

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 解析

```c
	if ( !xrtTimeParse(SV("2024-03-10T08:34:56-0400"),
		SV("%Y-%m-%dT%H:%M:%S%z"), &Parsed) || (Parsed != Moment) ) {
```

### `xrtDateTimeWrite`

按自定义格式把 `xdatetime` 字段写入缓冲。

```c
size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity, const xdatetime* pDateTime, xstrview Format)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sBuffer` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pDateTime` | 输入 | 非空 | 字段结构 |
| `Format` | 输入 | 借用 | 格式串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 写入字节数（不含零） | — |
| `0` | 容量不足或格式非法 | `XERR_RANGE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 字段格式化到缓冲

```c
	iSize = xrtDateTimeWrite(Buffer, sizeof(Buffer), &(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Microsecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
```

### `xrtDateTimeFormat`

按自定义格式把 `xdatetime` 字段生成零结尾文本。

```c
str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDateTime` | 输入 | 非空 | 字段结构 |
| `Format` | 输入 | 借用 | 格式串 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_ARGUMENT` — 格式串非法
- `XERR_MEMORY` — 分配失败

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 字段格式化

```c
	sFormatted = xrtDateTimeFormat(&(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Microsecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
```

### `xrtDateTimeParse`

按自定义格式解析为 `xdatetime` 字段。

```c
bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `Format` | 输入 | 借用 | 格式串 |
| `pDateTime` | 输出 | 非空 | 接收字段 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 文本与格式不匹配或字段越界

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 字段解析

```c
	if ( !xrtDateTimeParse(SV("2024-03-10 12:34:56"),
		SV("%Y-%m-%d %H:%M:%S"), &Parts) ||
		 !xrtTimeMake(&Parts, &Parsed) || (Parsed != Moment) ) {
```

## 协议时间

```c
size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity,
	xtime iTime, int iOffset);
str xrtTimeRFC3339(xtime iTime, int iOffset);
bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime);
```

RFC 3339 输出使用四位非负年份；零偏移输出 `Z`，小数秒删除末尾零。解析接受任意正长度小数秒，超过微秒的部分向零截断。闰秒和秒级 UTC 偏移不在当前 `xtime` 协议契约中，严格拒绝。

```c
size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime);
str xrtTimeHTTPDate(xtime iTime);
bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime);
bool xrtTimeTryParseHTTPDate(xstrview Text, xtime* pTime);
```

HTTP 输出始终生成 29 字节 IMF-fixdate GMT 文本并丢弃微秒。解析支持 IMF-fixdate、RFC 850 和 ANSI C asctime 三种 HTTP 日期，且校验文本星期与实际日期一致。RFC 850 两位年份按相对当前时间的 50 年规则解释，不写死具体世纪。`xrtTimeTryParseHTTPDate` 供协议分类器试探输入，失败不修改输出和线程错误；严格入口会发布结构化解析错误。

```c
bool xrtTimeParseAny(xstrview Text, xtime* pTime);
```

便捷解析按文本形状选择唯一解析器，支持 RFC 3339、三种 HTTP-date、`YYYY-MM-DD HH:MM:SS`、斜线/点号形式、日期形式、`YYYYMMDDHHMMSS`、`YYYYMMDD HHMMSS`、`YYYYMMDD` 和 `HH:MM:SS`。它不通过连续试探解析器实现，因此一次失败只报告一次错误。

### `xrtTimeRFC3339`

生成 RFC 3339 文本，`xrtFree` 释放。

```c
str xrtTimeRFC3339(xtime iTime, int iOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |
| `iOffset` | 输入 | — | UTC 偏移分钟 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 偏移超出可表示范围
- `XERR_MEMORY` — 分配失败

#### 范例

[protocol](../../examples/time/protocol/main.c) · RFC 3339

```c
	sRFC3339 = xrtTimeRFC3339(iTime, 8 * 3600);
```

### `xrtTimeWriteRFC3339`

把 RFC 3339 文本写入调用方缓冲。

```c
size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity, xtime iTime, int iOffset)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sBuffer` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `iTime` | 输入 | — | 时间值 |
| `iOffset` | 输入 | — | UTC 偏移分钟 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 写入字节数（不含零） | — |
| `0` | 容量不足 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[file_report](../../examples/file/report/main.c) · RFC 3339 到缓冲

```c
	iCreatedSize = xrtTimeWriteRFC3339(
		arrCreated, sizeof(arrCreated), iNow, 0
	);
```

### `xrtTimeParseRFC3339`

严格解析 RFC 3339 文本为 `xtime`。

```c
bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 文本不符合 RFC 3339 或字段越界

#### 范例

[protocol](../../examples/time/protocol/main.c) · 解析 RFC 3339

```c
	if ( !xrtTimeParseRFC3339(
		XRT_STR_LITERAL("1994-11-06T08:49:37Z"), &iTime) ) {
```

### `xrtTimeHTTPDate`

生成 IMF-fixdate HTTP 日期文本，`xrtFree` 释放。

```c
str xrtTimeHTTPDate(xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾结果，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_MEMORY` 等 |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[protocol](../../examples/time/protocol/main.c) · HTTP 日期

```c
	sHTTPDate = xrtTimeHTTPDate(iTime);
```

### `xrtTimeWriteHTTPDate`

把 IMF-fixdate HTTP 日期写入调用方缓冲。

```c
size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sBuffer` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `iTime` | 输入 | — | 时间值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `> 0` | 写入字节数（不含零） | — |
| `0` | 容量不足 | `XERR_RANGE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · HTTP 日期到缓冲

```c
	iSize = xrtTimeWriteHTTPDate(Buffer, sizeof(Buffer), Moment);
```

### `xrtTimeParseHTTPDate`

严格解析 HTTP 日期为 `xtime`。

```c
bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 文本不符合 HTTP 日期三种合法形态

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 解析 HTTP 日期

```c
	if ( !xrtTimeParseHTTPDate(SV("Sun, 10 Mar 2024 12:34:56 GMT"),
		&Parsed) || (Parsed != Moment) ||
		 !xrtTimeParseHTTPDate(SV("Sunday, 10-Mar-24 12:34:56 GMT"),
			&Parsed) || (Parsed != Moment) ||
		 !xrtTimeParseHTTPDate(SV("Sun Mar 10 12:34:56 2024"),
			&Parsed) || (Parsed != Moment) ) {
```

### `xrtTimeTryParseHTTPDate`

按宽松规则解析 HTTP 日期为 `xtime`。

```c
bool xrtTimeTryParseHTTPDate(xstrview Text, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 文本无法解析

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 宽松解析 HTTP 日期

```c
	if ( xrtTimeTryParseHTTPDate(SV("not a date at all"), &Parsed) ||
		 (Parsed != Moment) ) {
```

### `xrtTimeParseAny`

自动识别 RFC 3339 或 HTTP 日期形态并解析。

```c
bool xrtTimeParseAny(xstrview Text, xtime* pTime)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |
| `pTime` | 输出 | 非空 | 接收结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 不匹配任何受支持形态

#### 范例

[text_parse](../../examples/time/text_parse/main.c) · 自动识别解析

```c
	if ( !xrtTimeParseAny(SV("2024-03-10T12:34:56Z"), &Parsed) ||
		 (Parsed != Moment) ) {
```

## 错误与线程

时间模块使用错误域 `xrt.time`：

| 错误码 | 含义 |
| --- | --- |
| `XTIME_ERROR_RANGE` | 字段、偏移或协议表达范围无效 |
| `XTIME_ERROR_OVERFLOW` | 结果超出 `xtime` 或长度范围 |
| `XTIME_ERROR_FORMAT` | 自定义格式无效 |
| `XTIME_ERROR_PARSE` | 输入不能被完整解析 |
| `XTIME_ERROR_LOCAL_GAP` | 本地时间不存在 |
| `XTIME_ERROR_LOCAL_FOLD` | 本地时间存在两个候选且未选择 |
| `XTIME_ERROR_LOCAL_UNSUPPORTED` | 平台时钟或时区转换不可用 |

全部函数不使用可变的格式化全局状态。固定日历与文本函数可并发调用；本地时区结果受操作系统时区配置变化影响，但实现不暴露共享 CRT `tm` 缓冲区。

## 示例

- `examples/time/basic/main.c`：构造、分解、偏移和日历加法。
- `examples/time/clock/main.c`：单调时钟、截止点和轻量计时。
- `examples/time/local/main.c`：系统本地时区往返。
- `examples/time/format/main.c`：调用方缓冲区与自动分配格式化。
- `examples/time/protocol/main.c`：RFC 3339 与 HTTP-date。

## 旧版资产决策

新版保留了旧版的微秒精度、`xrtNow`/`xrtTimer`/`xrtSleep` 使用手感、Gregorian 400 年周期思想、日期字段提取、格式化能力范围和跨平台单调时钟路径。旧测试和七组示例作为功能迁移清单继续承接。

以下实现因明确缺陷被替换：负时间 `abs` 镜像、`INT64_MIN` 未定义行为、非 ISO 周算法、以当前偏移代替历史 DST、共享/固定格式缓冲、重复智能解析器、解析未完整消费、零值兼作失败以及可变全局近似容差。替换后仍由一套日历原语支撑基础、文本和协议路径，没有保留第二套兼容实现。

| 旧版能力 | 新版去向 | 决策 |
| --- | --- | --- |
| `xrtTimeSerial` / `xrtTimeDecode` | `xrtDate`、`xrtDateTime`、`xrtTimeMake`、`xrtTimeSplit` | 合并为构造与分解两组明确原语 |
| `xrtDateAdd` / `xrtDateDiff` | `xrtTimeAdd` / `xrtTimeDiff` | 保留手感，统一固定时长与日历单位语义 |
| `xrtIsSameDay/Month/Year` | `xrtTimeSameDay/Month/Year` | 保留便利能力，修复负时间和极值边界 |
| `xrtFirst/LastDayOfMonth/Year` | `xrtMonthRange` / `xrtYearRange` | 用半开区间同时表达首尾边界 |
| `xrtWeekOfYear` | `xrtISOWeek` | 替换为真正的 ISO 8601 周年与周数 |
| `xrtWeekOfMonth` | `xrtWeekRange` | 退役含义不唯一的序号，改为显式周首日和范围 |
| `xrtTimeToStr` / `xrtStrToTime` / 自定义解析 | `Write`、`Format`、`Parse`、`ParseAny` 和 RFC 专用 API | 合并重复解析器并去除固定缓冲上限 |
| `xrtNowUTC` / `xrtUTCToLocal` / `xrtLocalToUTC` | `xrtNow`、`xrtTimeLocal`、`xrtTimeFromLocal`、`xrtTimeSplitAt` | 绝对时间统一为 Epoch，系统时区与固定偏移分层 |
| `xrtTimezoneOffset` | `xdatetime.Offset` | 返回目标时刻的真实历史偏移，不再套用当前偏移 |
| `xrtTimeApprox` | `xrtTimeNear` | 容差由每次调用显式传入，不保留可变全局状态 |
| `xrtRelativeTime` | 后续独立的可裁剪本地化层 | 不在基础/协议文本层硬编码中文和近似月年长度 |
