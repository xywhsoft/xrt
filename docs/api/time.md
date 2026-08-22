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

## Unix 单位转换

```c
bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime);
bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime);
int64 xrtTimeUnix(xtime iTime);
int64 xrtTimeUnixMs(xtime iTime);
```

缩小精度时向负无穷取整，因此 `xrtTimeUnix(-1) == -1`，不会把 Epoch 前一微秒错误转换为零。放大精度发生溢出时返回 false。

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
