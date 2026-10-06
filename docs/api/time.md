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


### `xrtDate`

```c
bool xrtDate(int64 iYear, int iMonth, int iDay, xtime* pTime);
```

构造 UTC 零点日期。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iYear` | `int64` | 公元纪年，负数为公元前，不允许零年。 |
| `iMonth` | `int` | 月份 1..12。 |
| `iDay` | `int` | 月内日期，必须在该年月的有效范围。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDate(2023, 1, 1, &NewYear2023)
```
### `xrtDateDiff`

```c
bool xrtDateDiff(xtime iStart, xtime iEnd, xtimeunit Unit, int64* pResult);
```

按指定维度计算整数日期差；固定单位向零截断，年月按日历序号计算。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iStart` | `xtime` | 区间或日期差的起点，公元 UTC 毫秒。 |
| `iEnd` | `xtime` | 区间或日期差的终点，公元 UTC 毫秒。 |
| `Unit` | `xtimeunit` | 固定单位或 Gregorian 日历单位的枚举值。 |
| `pResult` | `int64*` | 调用方结果槽，按当前签名的类型交付结果。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDateDiff(Base, Day10 + 10 * XRT_TIME_DAY,
		XTIME_UNIT_DAY, &iDiff);
```
### `xrtDatePart`

```c
xtime xrtDatePart(xtime iTime);
```

返回 UTC 当日零点。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDatePart(Moment);
```
### `xrtDateTime`

```c
bool xrtDateTime(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond, int iMillisecond, xtime* pTime);
```

构造 UTC 日期时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iYear` | `int64` | 公元纪年，负数为公元前，不允许零年。 |
| `iMonth` | `int` | 月份 1..12。 |
| `iDay` | `int` | 月内日期，必须在该年月的有效范围。 |
| `iHour` | `int` | 小时 0..23。 |
| `iMinute` | `int` | 分钟 0..59。 |
| `iSecond` | `int` | 秒 0..59。 |
| `iMillisecond` | `int` | 秒内毫秒 0..999。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/template/core/main.c](../../examples/template/core/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDateTime(
		2026,
		4,
		2,
		12,
		30,
		0,
		0,
		&Time
	)
```
### `xrtDateTimeFormat`

```c
str xrtDateTimeFormat(const xdatetime* pDateTime, xstrview Format);
```

按 % 占位符创建时间文本；%p 输出大写午别，%P 输出小写午别，返回值由 xrtFree 释放。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pDateTime` | `const xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDateTimeFormat(&(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Millisecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
```
### `xrtDateTimeParse`

```c
bool xrtDateTimeParse(xstrview Text, xstrview Format, xdatetime* pDateTime);
```

严格解析完整格式；%-m 等字段接受一到两位数字，格式非法与文本不匹配使用不同错误码。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |
| `pDateTime` | `xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDateTimeParse(SV("2024-03-10 12:34:56"),
		SV("%Y-%m-%d %H:%M:%S"), &Parts)
```
### `xrtDateTimeWrite`

```c
size_t xrtDateTimeWrite(char* sBuffer, size_t iCapacity, const xdatetime* pDateTime, xstrview Format);
```

按 % 占位符写入并返回所需字节数；%-m 等数字占位符取消填充，输出缓冲不得与 Format 重叠。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `sBuffer` | `char*` | 调用方输出缓冲，容量为零时可查询所需长度。 |
| `iCapacity` | `size_t` | 输出缓冲容量，包含终止 NUL 的空间。 |
| `pDateTime` | `const xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |

#### 返回值

所需文本字节数，不含 NUL；容量不足时仍报告所需长度，缓冲规则见时间文本契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDateTimeWrite(Buffer, sizeof(Buffer), &(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Millisecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
```
### `xrtDay`

```c
int xrtDay(xtime iTime);
```

提取 UTC 月内日期。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDay(Moment)
```
### `xrtDayOfYear`

```c
int xrtDayOfYear(xtime iTime);
```

提取年内日期，范围为 1 到 366。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDayOfYear(Moment)
```
### `xrtDaysInMonth`

```c
int xrtDaysInMonth(int64 iYear, int iMonth);
```

返回指定月份的天数；月份无效时返回零并设置参数错误。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iYear` | `int64` | 公元纪年，负数为公元前，不允许零年。 |
| `iMonth` | `int` | 月份 1..12。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDaysInMonth(2024, 2)
```
### `xrtDaysInYear`

```c
int xrtDaysInYear(int64 iYear);
```

返回指定年份的天数。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iYear` | `int64` | 公元纪年，负数为公元前，不允许零年。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtDaysInYear(2024)
```
### `xrtHour`

```c
int xrtHour(xtime iTime);
```

提取 UTC 小时。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtHour(Moment)
```
### `xrtISOWeek`

```c
bool xrtISOWeek(xtime iTime, int64* pWeekYear, int* pWeek, int* pWeekday);
```

返回 ISO 8601 周年、周数和星期值，其中星期一为 1，星期日为 7。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pWeekYear` | `int64*` | 输出 ISO 周年，可能与日历年不同。 |
| `pWeek` | `int*` | 输出 ISO 周数 1..53。 |
| `pWeekday` | `int*` | 输出 ISO 星期 1..7，星期一为 1。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtISOWeek(Moment, &iWeekYear, &iWeek, &iWeekday)
```
### `xrtIsLeapYear`

```c
bool xrtIsLeapYear(int64 iYear);
```

判断公元年份是否为闰年；负数表示公元前，不接受零年。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iYear` | `int64` | 公元纪年，负数为公元前，不允许零年。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtIsLeapYear(2000)
```
### `xrtMillisecond`

```c
int xrtMillisecond(xtime iTime);
```

提取秒内毫秒。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtMillisecond(Moment)
```
### `xrtMinute`

```c
int xrtMinute(xtime iTime);
```

提取 UTC 分钟。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtMinute(Moment)
```
### `xrtMonth`

```c
int xrtMonth(xtime iTime);
```

提取 UTC 月份。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtMonth(Moment)
```
### `xrtMonthRange`

```c
bool xrtMonthRange(xtime iTime, xtime* pStart, xtime* pEnd);
```

返回包含给定时间的半开月份区间 [start, end)。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pStart` | `xtime*` | 输出半开区间的包含端点。 |
| `pEnd` | `xtime*` | 输出半开区间的不包含端点。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtMonthRange(Base, &Start, &End)
```
### `xrtNow`

```c
xtime xrtNow(void);
```

返回当前公元 UTC 毫秒。

#### 参数

无参数。

#### 返回值

当前公元 UTC 毫秒；系统日历校时会改变读数，读取失败返回零并设错。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtNow();
```
### `xrtQuarter`

```c
int xrtQuarter(xtime iTime);
```

提取季度，范围为 1 到 4。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtQuarter(Moment)
```
### `xrtSecond`

```c
int xrtSecond(xtime iTime);
```

提取 UTC 秒。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtSecond(Moment)
```
### `xrtSleep`

```c
void xrtSleep(int64 iMilliseconds);
```

至少睡眠指定毫秒；零表示让出当前执行时间片。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iMilliseconds` | `int64` | 毫秒；Unix 转换采用 Unix 零点，Sleep 采用非负相对时长。 |

#### 返回值

无返回值。资源或引用的释放范围按上述契约执行。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/concurrency/deadline/main.c](../../examples/concurrency/deadline/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtSleep(50);
```
### `xrtTimeAdd`

```c
bool xrtTimeAdd(xtime iTime, int64 iValue, xtimeunit Unit, xtime* pResult);
```

增加固定时长或 Gregorian 日历单位，月末会钳制到目标月最后一天。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iValue` | `int64` | 要增减的有符号单位数。 |
| `Unit` | `xtimeunit` | 固定单位或 Gregorian 日历单位的枚举值。 |
| `pResult` | `xtime*` | 调用方结果槽，按当前签名的类型交付结果。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeAdd(iNow, 1, XTIME_UNIT_MONTH, &iNextMonth)
```
### `xrtTimeFormat`

```c
str xrtTimeFormat(xtime iTime, int iOffset, xstrview Format);
```

按固定 UTC 偏移和上述占位符创建时间文本，返回值由 xrtFree 释放。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iOffset` | `int` | 归档在 Source 中的起始字节偏移。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/format/main.c](../../examples/time/format/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeFormat(iTime, 0,
		XRT_STR_LITERAL("%A, %B %d, %Y"));
```
### `xrtTimeFromLocal`

```c
bool xrtTimeFromLocal(const xdatetime* pDateTime, xtimefold Fold, xtime* pTime);
```

使用操作系统时区规则构造本地时间，并显式处理 DST 重复区间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pDateTime` | `const xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |
| `Fold` | `xtimefold` | DST 回拨时选择拒绝、较早或较晚候选值。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/local/main.c](../../examples/time/local/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeFromLocal(&tLocal, XTIME_FOLD_EARLIER, &iRoundtrip)
```
### `xrtTimeFromUnix`

```c
bool xrtTimeFromUnix(int64 iSeconds, xtime* pTime);
```

从 Unix 秒安全构造 xtime。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iSeconds` | `int64` | 以 1970-01-01 UTC 为零点的有符号 Unix 秒。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeFromUnix(xrtTimeUnix(Moment), &FromS);
```
### `xrtTimeFromUnix32`

```c
bool xrtTimeFromUnix32(int32 iSeconds, xtime* pTime);
```

32 位有符号 Unix 秒的安全双向转换。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iSeconds` | `int32` | 以 1970-01-01 UTC 为零点的有符号 Unix 秒。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)，结合本节参数和生存期规则使用。



### `xrtTimeFromUnixMs`

```c
bool xrtTimeFromUnixMs(int64 iMilliseconds, xtime* pTime);
```

从 Unix 毫秒安全构造 xtime。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iMilliseconds` | `int64` | 毫秒；Unix 转换采用 Unix 零点，Sleep 采用非负相对时长。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeFromUnixMs(EXAMPLE_EPOCH_MS, &FromMs)
```
### `xrtTimeHTTPDate`

```c
str xrtTimeHTTPDate(xtime iTime);
```

创建 HTTP IMF-fixdate，返回值由 xrtFree 释放。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/protocol/main.c](../../examples/time/protocol/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeHTTPDate(iTime);
```
### `xrtTimeIn`

```c
bool xrtTimeIn(xtime iTime, xtime iStart, xtime iEnd);
```

判断时间是否位于闭区间；反向区间返回 false。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iStart` | `xtime` | 区间或日期差的起点，公元 UTC 毫秒。 |
| `iEnd` | `xtime` | 区间或日期差的终点，公元 UTC 毫秒。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeIn(Day11, Base, Day11)
```
### `xrtTimeLocal`

```c
bool xrtTimeLocal(xtime iTime, xdatetime* pDateTime);
```

使用操作系统当前时区规则分解绝对时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pDateTime` | `xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/local/main.c](../../examples/time/local/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeLocal(iNow, &tLocal)
```
### `xrtTimeMake`

```c
bool xrtTimeMake(const xdatetime* pDateTime, xtime* pTime);
```

按结构中的显式 UTC 偏移构造绝对时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `pDateTime` | `const xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeMake(&Parts, &Made)
```
### `xrtTimeNear`

```c
bool xrtTimeNear(xtime iLeft, xtime iRight, uint64 iTolerance);
```

使用显式毫秒容差比较两个时间，计算覆盖完整 int64 域。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iLeft` | `xtime` | 第一个公元 UTC 毫秒时间。 |
| `iRight` | `xtime` | 第二个公元 UTC 毫秒时间。 |
| `iTolerance` | `uint64` | 允许的非负毫秒差。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeNear(Base, Base + 900, 1000u)
```
### `xrtTimeOverlap`

```c
bool xrtTimeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2);
```

判断两个闭区间是否重叠；任一反向区间返回 false。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iStart1` | `xtime` | 闭区间端点，公元 UTC 毫秒；反向区间不匹配。 |
| `iEnd1` | `xtime` | 闭区间端点，公元 UTC 毫秒；反向区间不匹配。 |
| `iStart2` | `xtime` | 闭区间端点，公元 UTC 毫秒；反向区间不匹配。 |
| `iEnd2` | `xtime` | 闭区间端点，公元 UTC 毫秒；反向区间不匹配。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeOverlap(Mar1, Base, Mar1, Apr1)
```
### `xrtTimeParse`

```c
bool xrtTimeParse(xstrview Text, xstrview Format, xtime* pTime);
```

严格按上述完整格式解析绝对时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeParse(SV("2024-03-10T08:34:56-0400"),
		SV("%Y-%m-%dT%H:%M:%S%z"), &Parsed)
```
### `xrtTimeParseAny`

```c
bool xrtTimeParseAny(xstrview Text, xtime* pTime);
```

解析 RFC 3339、HTTP-date 和常见数字日期时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeParseAny(SV("2024-03-10T12:34:56Z"), &Parsed)
```
### `xrtTimeParseHTTPDate`

```c
bool xrtTimeParseHTTPDate(xstrview Text, xtime* pTime);
```

解析 IMF-fixdate、RFC 850 和 ANSI C asctime 三种 HTTP 日期格式。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeParseHTTPDate(SV("Sun, 10 Mar 2024 12:34:56 GMT"),
		&Parsed)
```
### `xrtTimeParseRFC3339`

```c
bool xrtTimeParseRFC3339(xstrview Text, xtime* pTime);
```

严格解析 RFC 3339；秒内小数的毫秒以下尾数会被丢弃。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/protocol/main.c](../../examples/time/protocol/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeParseRFC3339(
		XRT_STR_LITERAL("1994-11-06T08:49:37Z"), &iTime)
```
### `xrtTimePart`

```c
xtime xrtTimePart(xtime iTime);
```

返回 UTC 当日已经经过的毫秒，范围为 [0, XRT_TIME_DAY)。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimePart(Moment)
```
### `xrtTimeRFC3339`

```c
str xrtTimeRFC3339(xtime iTime, int iOffset);
```

创建 RFC 3339 文本，返回值由 xrtFree 释放。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iOffset` | `int` | 归档在 Source 中的起始字节偏移。 |

#### 返回值

成功为拥有型缓冲，使用 xrtFree 释放；失败为 NULL。精确长度及输出槽规则见上述契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/protocol/main.c](../../examples/time/protocol/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeRFC3339(iTime, 8 * 3600);
```
### `xrtTimeSameDay`

```c
bool xrtTimeSameDay(xtime iLeft, xtime iRight);
```

判断两个 UTC 时间是否位于同一个 Gregorian 日期。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iLeft` | `xtime` | 第一个公元 UTC 毫秒时间。 |
| `iRight` | `xtime` | 第二个公元 UTC 毫秒时间。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeSameDay(Base, Base + 3600 * XRT_TIME_SECOND)
```
### `xrtTimeSameMonth`

```c
bool xrtTimeSameMonth(xtime iLeft, xtime iRight);
```

判断两个 UTC 时间是否位于同一个 Gregorian 月份。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iLeft` | `xtime` | 第一个公元 UTC 毫秒时间。 |
| `iRight` | `xtime` | 第二个公元 UTC 毫秒时间。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeSameMonth(Base, Mar1)
```
### `xrtTimeSameYear`

```c
bool xrtTimeSameYear(xtime iLeft, xtime iRight);
```

判断两个 UTC 时间是否位于同一个 Gregorian 年份。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iLeft` | `xtime` | 第一个公元 UTC 毫秒时间。 |
| `iRight` | `xtime` | 第二个公元 UTC 毫秒时间。 |

#### 返回值

true 表示谓词成立，false 表示不成立；普通不成立不代表操作失败。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 谓词成立 | 按上述契约交付结果 |
| `false` | 谓词不成立 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeSameYear(Base, Jan1 + 100 * XRT_TIME_DAY)
```
### `xrtTimeSplit`

```c
bool xrtTimeSplit(xtime iTime, xdatetime* pDateTime);
```

把绝对时间按 UTC 分解为日期时间。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pDateTime` | `xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeSplit(iNow, &tUTC)
```
### `xrtTimeSplitAt`

```c
bool xrtTimeSplitAt(xtime iTime, int iOffset, xdatetime* pDateTime);
```

把绝对时间按固定 UTC 偏移分解，偏移范围为 -23:59:59 到 +23:59:59。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iOffset` | `int` | 归档在 Source 中的起始字节偏移。 |
| `pDateTime` | `xdatetime*` | 日历字段；Make/格式化读取输入，Split/解析写入输出。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeSplitAt(iNow, 8 * 3600, &tLocalOffset)
```
### `xrtTimeToUnix32`

```c
bool xrtTimeToUnix32(xtime iTime, int32* pSeconds);
```

转成有符号 32 位 Unix 秒，向负无穷取整；超出目标范围失败且保持输出原值。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pSeconds` | `int32*` | 输出有符号 Unix 秒。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/basic/main.c](../../examples/time/basic/main.c)，结合本节参数和生存期规则使用。



### `xrtTimeToUnixMs`

```c
bool xrtTimeToUnixMs(xtime iTime, int64* pMilliseconds);
```

安全转换为 Unix 毫秒；目标范围溢出时不修改输出。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pMilliseconds` | `int64*` | 输出有符号 Unix 毫秒。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeToUnixMs(FromMs, &RoundtripMs)
```
### `xrtTimeTryParseHTTPDate`

```c
bool xrtTimeTryParseHTTPDate(xstrview Text, xtime* pTime);
```

尝试解析三种 HTTP 日期格式；失败不修改输出和线程错误。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `Text` | `xstrview` | 带精确长度的输入文本视图，允许内嵌 NUL。 |
| `pTime` | `xtime*` | 输出公元 UTC 毫秒；可失败构造和解析保持输出原值。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

正常不匹配不设置新错误；TryParseHTTPDate 失败同时保持输出和已有线程错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeTryParseHTTPDate(SV("not a date at all"), &Parsed)
```
### `xrtTimeUnix`

```c
int64 xrtTimeUnix(xtime iTime);
```

返回向负无穷取整的 Unix 秒。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeUnix(Midnight)
```
### `xrtTimeWrite`

```c
size_t xrtTimeWrite(char* sBuffer, size_t iCapacity, xtime iTime, int iOffset, xstrview Format);
```

按固定 UTC 偏移和上述占位符写入；输出缓冲不得与 Format 重叠。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `sBuffer` | `char*` | 调用方输出缓冲，容量为零时可查询所需长度。 |
| `iCapacity` | `size_t` | 输出缓冲容量，包含终止 NUL 的空间。 |
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iOffset` | `int` | 归档在 Source 中的起始字节偏移。 |
| `Format` | `xstrview` | 格式视图，%f 表示三位毫秒。 |

#### 返回值

所需文本字节数，不含 NUL；容量不足时仍报告所需长度，缓冲规则见时间文本契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/file/report/main.c](../../examples/file/report/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeWrite(
		arrFileName,
		sizeof(arrFileName),
		iNow,
		0,
		XRT_STR_LITERAL("report_%Y%m%d_%H%M%S_%f.txt")
	);
```
### `xrtTimeWriteHTTPDate`

```c
size_t xrtTimeWriteHTTPDate(char* sBuffer, size_t iCapacity, xtime iTime);
```

写入 HTTP IMF-fixdate，时间始终转换为 GMT 并丢弃秒以下部分。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `sBuffer` | `char*` | 调用方输出缓冲，容量为零时可查询所需长度。 |
| `iCapacity` | `size_t` | 输出缓冲容量，包含终止 NUL 的空间。 |
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

所需文本字节数，不含 NUL；容量不足时仍报告所需长度，缓冲规则见时间文本契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/text_parse/main.c](../../examples/time/text_parse/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeWriteHTTPDate(Buffer, sizeof(Buffer), Moment);
```
### `xrtTimeWriteRFC3339`

```c
size_t xrtTimeWriteRFC3339(char* sBuffer, size_t iCapacity, xtime iTime, int iOffset);
```

写入 RFC 3339 文本；零偏移使用 Z，毫秒末尾的零会被删除。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `sBuffer` | `char*` | 调用方输出缓冲，容量为零时可查询所需长度。 |
| `iCapacity` | `size_t` | 输出缓冲容量，包含终止 NUL 的空间。 |
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iOffset` | `int` | 归档在 Source 中的起始字节偏移。 |

#### 返回值

所需文本字节数，不含 NUL；容量不足时仍报告所需长度，缓冲规则见时间文本契约。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/file/report/main.c](../../examples/file/report/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimeWriteRFC3339(
		arrCreated, sizeof(arrCreated), iNow, 0
	);
```
### `xrtTimer`

```c
double xrtTimer(void);
```

返回高精度单调计时器的 double 秒数；原点无意义，两个读数相减得到耗时。

#### 参数

无参数。

#### 返回值

单调时钟的 double 秒数；只使用读数之差。读取失败返回 NaN 并设错。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/concurrency/channel_select_cancel/main.c](../../examples/concurrency/channel_select_cancel/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtTimer()
```
### `xrtWeekRange`

```c
bool xrtWeekRange(xtime iTime, int iFirstWeekday, xtime* pStart, xtime* pEnd);
```

返回包含给定时间的半开星期区间 [start, end)。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `iFirstWeekday` | `int` | 一周起始星期，使用星期日为零的星期枚举。 |
| `pStart` | `xtime*` | 输出半开区间的包含端点。 |
| `pEnd` | `xtime*` | 输出半开区间的不包含端点。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtWeekRange(Base, 0, &Start, &End)
```
### `xrtWeekday`

```c
int xrtWeekday(xtime iTime);
```

提取星期，范围为 XTIME_SUNDAY 到 XTIME_SATURDAY。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtWeekday(Moment)
```
### `xrtYear`

```c
int64 xrtYear(xtime iTime);
```

提取 UTC 年份。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |

#### 返回值

返回上述契约定义的计数、日历字段、状态或能力值；单位与当前函数签名一致。

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/calendar_tour/main.c](../../examples/time/calendar_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtYear(Moment)
```
### `xrtYearRange`

```c
bool xrtYearRange(xtime iTime, xtime* pStart, xtime* pEnd);
```

返回包含给定时间的半开年份区间 [start, end)。

#### 参数

| 参数 | 类型 | 说明 |
|---|---|---|
| `iTime` | `xtime` | 公元 UTC 毫秒，零点为公元 1 年 1 月 1 日。 |
| `pStart` | `xtime*` | 输出半开区间的包含端点。 |
| `pEnd` | `xtime*` | 输出半开区间的不包含端点。 |

#### 返回值

true 表示完成，false 表示拒绝或失败；失败时的输出及数据所有权按上述契约处理。

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 操作完成 | 按上述契约交付结果 |
| `false` | 拒绝、忙碌或失败 | 正常不成立及忙碌按本节错误契约区分；其余失败状态见上述契约 |

#### 错误

日期/偏移/零年无效为 XTIME_ERROR_RANGE，目标或算术溢出为 XTIME_ERROR_OVERFLOW；文本格式与内容分别为 XTIME_ERROR_FORMAT / XTIME_ERROR_PARSE。当地 DST gap/fold/系统限制使用对应 LOCAL 错误。纯比较的不成立不设置错误。

#### 范例

参见已注册的 [examples/time/range_tour/main.c](../../examples/time/range_tour/main.c)；下面调用摘自该完整程序，初始化、返回值处理和清理见原文件。

```c
xrtYearRange(Base, &Start, &End)
```
### `xdatetime`

Gregorian 日历字段。Year 使用无零年的公元纪年，Millisecond 为 0..999，Offset 是 UTC 以东为正的秒，Weekday 星期日为零，YearDay 从 1 开始，IsDST 为 1/0/-1。

```c
typedef struct xdatetime {
	int64 Year;
	int Month;
	int Day;
	int Hour;
	int Minute;
	int Second;
	int Millisecond;
	int Offset;
	int Weekday;
	int YearDay;
	int IsDST;
} xdatetime;
```


| 字段 | 类型 | 语义 |
|---|---|---|
| `Year` | `int64` | 公元年份，负数为公元前，无零年。 |
| `Month` | `int` | 月份 1..12。 |
| `Day` | `int` | 月内有效日。 |
| `Hour` | `int` | 小时 0..23。 |
| `Minute` | `int` | 分钟 0..59。 |
| `Second` | `int` | 秒 0..59。 |
| `Millisecond` | `int` | 秒内毫秒 0..999。 |
| `Offset` | `int` | UTC 以东为正的秒数。 |
| `Weekday` | `int` | 星期日零起点的星期编号。 |
| `YearDay` | `int` | 年内日，从 1 开始。 |
| `IsDST` | `int` | 1 为夏令时，0 为非夏令时，-1 为未知。 |

### `xtimeerror`

xrt.time 的稳定错误码，区分日期范围、溢出、格式、解析及当地 DST/系统范围失败。

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
| `XTIME_ERROR_RANGE` | 参数或日期范围错误 |
| `XTIME_ERROR_OVERFLOW` | 目标或算术溢出 |
| `XTIME_ERROR_FORMAT` | 格式串无效 |
| `XTIME_ERROR_PARSE` | 文本不匹配或无效 |
| `XTIME_ERROR_LOCAL_GAP` | 当地时间处于 DST gap |
| `XTIME_ERROR_LOCAL_FOLD` | 当地时间有两个未消歧候选 |
| `XTIME_ERROR_LOCAL_UNSUPPORTED` | 系统无法表示当地时间 |

### `xtimefold`

当地 DST 回拨出现两个候选时，显式拒绝或选择较早、较晚时间；固定偏移转换不使用该策略。

```c
typedef enum xtimefold {
	XTIME_FOLD_REJECT = 0,
	XTIME_FOLD_EARLIER,
	XTIME_FOLD_LATER
} xtimefold;
```


| 值 | 语义 |
|---|---|
| `XTIME_FOLD_REJECT` | 拒绝重复当地时间。 |
| `XTIME_FOLD_EARLIER` | 选择较早的 UTC 候选。 |
| `XTIME_FOLD_LATER` | 选择较晚的 UTC 候选。 |

### `xtimeunit`

毫秒至周为固定时长，月、季度、年按连续 Gregorian 日历序号计算。

```c
typedef enum xtimeunit {

	XTIME_UNIT_MILLISECOND = 0,
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
| `XTIME_UNIT_MILLISECOND` | 毫秒单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_SECOND` | 秒单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_MINUTE` | 分钟单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_HOUR` | 小时单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_DAY` | 日单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_WEEK` | 周单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_MONTH` | 月单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_QUARTER` | 季度单位；月、季度、年按日历，其余固定长度。 |
| `XTIME_UNIT_YEAR` | 年单位；月、季度、年按日历，其余固定长度。 |

### `xtimeweekday`

星期日为零、星期六为六的稳定日历星期编号。ISOWeek 的输出另使用星期一为一。

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
| `XTIME_SUNDAY` | 星期日 |
| `XTIME_MONDAY` | 星期一 |
| `XTIME_TUESDAY` | 星期二 |
| `XTIME_WEDNESDAY` | 星期三 |
| `XTIME_THURSDAY` | 星期四 |
| `XTIME_FRIDAY` | 星期五 |
| `XTIME_SATURDAY` | 星期六 |
