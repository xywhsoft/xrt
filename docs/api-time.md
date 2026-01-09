# Time 时间处理库

> 日期时间计算、格式化、解析和时区处理功能

[English](api-time.en.md) | [中文](api-time.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [时间获取](#时间获取)
- [时间构建](#时间构建)
- [时间提取](#时间提取)
- [时间解码](#时间解码)
- [时间计算](#时间计算)
- [时间格式化](#时间格式化)
- [自定义格式化](#自定义格式化)
- [时间比较](#时间比较)
  - [xrtTimeApprox](#xrttimeapprox)
- [边界日期](#边界日期)
- [周相关函数](#周相关函数)
- [时区处理](#时区处理)
- [Unix时间戳](#unix时间戳)
- [相对时间](#相对时间)
- [日期工具](#日期工具)
- [使用场景](#使用场景)

---

## 常量定义

### 时间单位常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_TIME_MINUTE` | `60` | 1分钟的秒数 |
| `XRT_TIME_HOUR` | `3600` | 1小时的秒数 |
| `XRT_TIME_DAY` | `86400` | 1天的秒数 |
| `XRT_TIME_YEAR` | `31536000` | 1平年的秒数 |
| `XRT_TIME_LEAPYEAR` | `31622400` | 1闰年的秒数 |
| `XRT_TIME_400YEAR` | `12622780800` | 400年的秒数（97个闰年+303个平年） |
| `XRT_TIME_19700101` | `62167219200` | 1970年1月1日的时间戳 |

### 时间间隔常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_TIME_INTERVAL_YEAR` | `1` | 年 |
| `XRT_TIME_INTERVAL_MONTH` | `2` | 月 |
| `XRT_TIME_INTERVAL_DAY` | `3` | 日 |
| `XRT_TIME_INTERVAL_HOUR` | `4` | 小时 |
| `XRT_TIME_INTERVAL_MINUTE` | `5` | 分钟 |
| `XRT_TIME_INTERVAL_SECOND` | `6` | 秒 |
| `XRT_TIME_INTERVAL_WEEKDAY` | `7` | 星期 |
| `XRT_TIME_INTERVAL_QUARTER` | `8` | 季度 |

### 时间格式常量

| 常量 | 值 | 输出格式 |
|------|-----|--------|
| `XRT_TIME_FORMAT_DATETIME` | `0` | `YYYY-MM-DD HH:MM:SS` |
| `XRT_TIME_FORMAT_DATE` | `1` | `YYYY-MM-DD` |
| `XRT_TIME_FORMAT_TIME` | `2` | `HH:MM:SS` |

---

## 时间获取

### xrtNow

获取当前日期和时间。

**函数原型：**
```c
XXAPI xtime xrtNow();
```

**返回值：**
- 当前日期时间的时间戳（秒）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    printf("时间戳: %" PRId64 "\n", now);
    
    // 转换为可读字符串
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDate

获取当前日期（不含时间部分）。

**函数原型：**
```c
XXAPI xtime xrtDate();
```

**返回值：**
- 当前日期的时间戳（时分秒为 00:00:00）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime today = xrtDate();
    str dateStr = xrtTimeToStr(today, XRT_TIME_FORMAT_DATE);
    printf("今天: %s\n", dateStr);
    xrtFree(dateStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTime

获取当前时间（不含日期部分）。

**函数原型：**
```c
XXAPI xtime xrtTime();
```

**返回值：**
- 当日已过的秒数（0-86399）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime currentTime = xrtTime();
    printf("今日已过秒数: %" PRId64 "\n", currentTime);
    
    str timeStr = xrtTimeToStr(currentTime, XRT_TIME_FORMAT_TIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimer

获取高精度计时器（用于性能测量）。

**函数原型：**
```c
XXAPI double xrtTimer();
```

**返回值：**
- 高精度时间（秒，浮点数）

**说明：**
- Windows: 使用 `QueryPerformanceCounter`
- Linux/macOS: 使用 `clock_gettime(CLOCK_MONOTONIC)`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

void HeavyWork() {
    // 模拟耗时操作
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    HeavyWork();
    double end = xrtTimer();
    
    printf("耗时: %.6f 秒\n", end - start);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 返回的是单调递增的时间，适合计算时间间隔
- 不受系统时间调整影响

---

### xrtSleep

毫秒级延时。

**函数原型：**
```c
XXAPI void xrtSleep(uint32 ms);
```

**参数：**
- `ms` - 延时毫秒数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("开始等待...\n");
    xrtSleep(1000);  // 等待 1 秒
    printf("1 秒后\n");
    
    xrtSleep(500);   // 等待 500 毫秒
    printf("又过了 0.5 秒\n");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `Sleep()`
- Linux/macOS 使用 `usleep()`

---

### xrtNowStr / xrtDateStr / xrtTimeStr

获取字符串格式的当前时间。

**函数原型：**
```c
XXAPI str xrtNowStr();   // 返回 "YYYY-MM-DD HH:MM:SS"
XXAPI str xrtDateStr();  // 返回 "YYYY-MM-DD"
XXAPI str xrtTimeStr();  // 返回 "HH:MM:SS"
```

**返回值：**
- 格式化的时间字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str now = xrtNowStr();
    str date = xrtDateStr();
    str time = xrtTimeStr();
    
    printf("当前日期时间: %s\n", now);   // "2025-12-03 15:30:45"
    printf("当前日期: %s\n", date);      // "2025-12-03"
    printf("当前时间: %s\n", time);      // "15:30:45"
    
    xrtFree(now);
    xrtFree(date);
    xrtFree(time);
    
    xrtUnit();
    return 0;
}
```

---

## 时间构建

### xrtDateSerial

构建日期时间戳。

**函数原型：**
```c
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
```

**参数：**
- `iYear` - 年份（支持负数表示公元前）
- `iMonth` - 月份（1-12）
- `iDay` - 日期（1-31）

**返回值：**
- 时间戳（秒）
- 月份无效时返回 0 并设置错误信息

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建日期
    xtime date = xrtDateSerial(2025, 1, 10);
    str dateStr = xrtTimeToStr(date, XRT_TIME_FORMAT_DATE);
    printf("日期: %s\n", dateStr);  // "2025-01-10"
    xrtFree(dateStr);
    
    // 公元前日期（负年份）
    xtime bc = xrtDateSerial(-100, 3, 15);
    printf("公元前时间戳: %" PRId64 "\n", bc);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeSerial

构建时间戳（当日秒数）。

**函数原型：**
```c
XXAPI xtime xrtTimeSerial(int iHour, int iMin, int iSec);
```

**参数：**
- `iHour` - 小时（0-23）
- `iMin` - 分钟（0-59）
- `iSec` - 秒（0-59）

**返回值：**
- 时间戳（当日秒数，0-86399）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建时间
    xtime time = xrtTimeSerial(15, 30, 45);
    printf("当日秒数: %" PRId64 "\n", time);  // 55845 = 15*3600 + 30*60 + 45
    
    str timeStr = xrtTimeToStr(time, XRT_TIME_FORMAT_TIME);
    printf("时间: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateTimeSerial

构建完整的日期+时间戳。

**函数原型：**
```c
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
```

**参数：**
- `iYear` - 年份
- `iMonth` - 月份（1-12）
- `iDay` - 日期（1-31）
- `iHour` - 小时（0-23）
- `iMinute` - 分钟（0-59）
- `iSecond` - 秒（0-59）

**返回值：**
- 完整时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 构建完整日期时间
    xtime dt = xrtDateTimeSerial(2025, 12, 25, 10, 30, 0);
    str dtStr = xrtTimeToStr(dt, XRT_TIME_FORMAT_DATETIME);
    printf("圣诞节: %s\n", dtStr);  // "2025-12-25 10:30:00"
    xrtFree(dtStr);
    
    // 计算结果等于 xrtDateSerial + xrtTimeSerial
    xtime d = xrtDateSerial(2025, 12, 25);
    xtime t = xrtTimeSerial(10, 30, 0);
    printf("相等: %s\n", (d + t == dt) ? "是" : "否");  // "是"
    
    xrtUnit();
    return 0;
}
```

---

## 时间计算

### xrtDateAdd

时间单位累加。

**函数原型：**
```c
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
```

**参数：**
- `interval` - 间隔类型（使用 `XRT_TIME_INTERVAL_*` 常量）
  - `XRT_TIME_INTERVAL_YEAR` (1) - 年
  - `XRT_TIME_INTERVAL_MONTH` (2) - 月
  - `XRT_TIME_INTERVAL_DAY` (3) - 天
  - `XRT_TIME_INTERVAL_HOUR` (4) - 小时
  - `XRT_TIME_INTERVAL_MINUTE` (5) - 分钟
  - `XRT_TIME_INTERVAL_SECOND` (6) - 秒
  - `XRT_TIME_INTERVAL_WEEKDAY` (7) - 星期（7天）
  - `XRT_TIME_INTERVAL_QUARTER` (8) - 季度（3个月）
- `iValue` - 增量（可为负数）
- `iTime` - 基准时间

**返回值：**
- 计算后的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    str nowStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前: %s\n", nowStr);
    xrtFree(nowStr);
    
    // 加 1 天
    xtime tomorrow = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, now);
    str s1 = xrtTimeToStr(tomorrow, XRT_TIME_FORMAT_DATE);
    printf("明天: %s\n", s1);
    xrtFree(s1);
    
    // 加 1 周
    xtime next_week = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 1, now);
    str s2 = xrtTimeToStr(next_week, XRT_TIME_FORMAT_DATE);
    printf("下周: %s\n", s2);
    xrtFree(s2);
    
    // 减 1 个月
    xtime last_month = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -1, now);
    str s3 = xrtTimeToStr(last_month, XRT_TIME_FORMAT_DATE);
    printf("上月: %s\n", s3);
    xrtFree(s3);
    
    // 加 1 季度
    xtime next_quarter = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 1, now);
    str s4 = xrtTimeToStr(next_quarter, XRT_TIME_FORMAT_DATE);
    printf("下季度: %s\n", s4);
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateDiff

计算时间差。

**函数原型：**
```c
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

**参数：**
- `interval` - 间隔单位（同 `xrtDateAdd`，但**不支持** `XRT_TIME_INTERVAL_WEEKDAY`）
- `iTime1` - 时间1
- `iTime2` - 时间2

**返回值：**
- 时间差（iTime2 - iTime1）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 12, 31);
    
    // 天数差
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, start, end);
    printf("相差天数: %" PRId64 "\n", days);  // 364
    
    // 月数差
    int64 months = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, start, end);
    printf("相差月数: %" PRId64 "\n", months);  // 11
    
    // 季度差
    int64 quarters = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, start, end);
    printf("相差季度: %" PRId64 "\n", quarters);  // 3
    
    // 小时差
    xtime t1 = xrtDateTimeSerial(2025, 1, 1, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 1, 1, 17, 30, 0);
    int64 hours = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, t1, t2);
    printf("相差小时: %" PRId64 "\n", hours);  // 9
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `XRT_TIME_INTERVAL_WEEKDAY` 不支持，返回 0
- 计算结果为 iTime2 - iTime1，可能为负数

---

## 时间格式化

### xrtTimeToStr

时间转字符串

**函数原型：**
```c
XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
```

**参数：**
- `iTime` - 时间戳
- `iFormat` - 格式类型
  - `0` - `YYYY-MM-DD HH:MM:SS`
  - `1` - `YYYY-MM-DD`
  - `2` - `HH:MM:SS`
  - `3` - `YYYYMMDDHHMMSS`

**返回值：**
- 格式化的时间字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xtime now = xrtNow();
str s0 = xrtTimeToStr(now, 0);  // "2025-01-10 15:30:45"
str s1 = xrtTimeToStr(now, 1);  // "2025-01-10"
str s2 = xrtTimeToStr(now, 2);  // "15:30:45"
str s3 = xrtTimeToStr(now, 3);  // "20250110153045"
xrtFree(s0); xrtFree(s1); xrtFree(s2); xrtFree(s3);
```

---

### xrtStrToTime

字符串解析为时间。

**函数原型：**
```c
XXAPI xtime xrtStrToTime(str sTime, size_t iSize);
```

**参数：**
- `sTime` - 时间字符串
- `iSize` - 字符串长度（0 表示自动计算）

**返回值：**
- 解析出的时间戳
- 解析失败返回 0

**支持的格式：**
- `YYYY-MM-DD HH:MM:SS`
- `YYYY-MM-DD`
- `YYYYMMDDHHMMSS`
- `YYYYMMDD`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 解析完整日期时间
    xtime t1 = xrtStrToTime("2025-01-10 15:30:45", 0);
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("解析1: %s\n", s1);  // "2025-01-10 15:30:45"
    xrtFree(s1);
    
    // 解析仅日期
    xtime t2 = xrtStrToTime("2025-01-10", 0);
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_DATE);
    printf("解析2: %s\n", s2);  // "2025-01-10"
    xrtFree(s2);
    
    // 解析紧凑格式
    xtime t3 = xrtStrToTime("20250110153045", 0);
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATETIME);
    printf("解析3: %s\n", s3);  // "2025-01-10 15:30:45"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

## 自定义格式化

### xrtTimeFormat

使用自定义格式将时间转换为字符串。

**函数原型：**
```c
XXAPI str xrtTimeFormat(xtime iTime, str sFormat);
```

**参数：**
- `iTime` - 时间戳
- `sFormat` - 格式字符串

**返回值：**
- 格式化后的字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**格式占位符：**

| 占位符 | 说明 | 示例 |
|--------|------|------|
| `yyyy` | 四位年份 | 2025 |
| `yy` | 两位年份 | 25 |
| `mm` | 两位月份（补零） | 01-12 |
| `m` | 月份（不补零） | 1-12 |
| `mmm` | 英文月份缩写 | Jan, Feb, ... |
| `mmmm` | 英文月份全称 | January, February, ... |
| `dd` | 两位日期（补零） | 01-31 |
| `d` | 日期（不补零） | 1-31 |
| `hh` | 24小时制小时（补零） | 00-23 |
| `h` | 24小时制小时（不补零） | 0-23 |
| `HH` | 12小时制小时（补零） | 01-12 |
| `H` | 12小时制小时（不补零） | 1-12 |
| `nn` | 分钟（补零） | 00-59 |
| `n` | 分钟（不补零） | 0-59 |
| `ss` | 秒（补零） | 00-59 |
| `s` | 秒（不补零） | 0-59 |
| `ap` | 小写上下午 | am, pm |
| `AP` | 大写上下午 | AM, PM |
| `w` | 星期数字 | 0-6 (0=周日) |
| `ww` | 英文星期缩写 | Sun, Mon, ... |
| `www` | 英文星期全称 | Sunday, Monday, ... |
| `q` | 季度 | 1-4 |

**特殊说明：**
- `mm` 在 `h` 或 `hh` 之后表示**分钟**，否则表示**月份**（兼容 VB 风格）
- 若需明确表示分钟，推荐使用 `nn` 或 `n`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateTimeSerial(2025, 3, 15, 14, 30, 45);
    
    // 基本格式
    str s1 = xrtTimeFormat(t, "yyyy-mm-dd hh:nn:ss");
    printf("%s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // 12小时制
    str s2 = xrtTimeFormat(t, "yyyy/mm/dd HH:nn:ss AP");
    printf("%s\n", s2);  // "2025/03/15 02:30:45 PM"
    xrtFree(s2);
    
    // 英文月份
    str s3 = xrtTimeFormat(t, "mmm d, yyyy");
    printf("%s\n", s3);  // "Mar 15, 2025"
    xrtFree(s3);
    
    // 完整英文格式
    str s4 = xrtTimeFormat(t, "mmmm d, yyyy");
    printf("%s\n", s4);  // "March 15, 2025"
    xrtFree(s4);
    
    // 带星期
    str s5 = xrtTimeFormat(t, "www, mmm d");
    printf("%s\n", s5);  // "Saturday, Mar 15"
    xrtFree(s5);
    
    // 季度信息
    str s6 = xrtTimeFormat(t, "yyyy年Q第q季度");
    printf("%s\n", s6);  // "2025年Q第1季度"
    xrtFree(s6);
    
    // 紧凑格式
    str s7 = xrtTimeFormat(t, "yyyymmddhhnnss");
    printf("%s\n", s7);  // "20250315143045"
    xrtFree(s7);
    
    xrtUnit();
    return 0;
}
```

**mm 上下文判断示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateTimeSerial(2025, 3, 15, 14, 30, 45);
    
    // mm 在 hh 后表示分钟
    str s1 = xrtTimeFormat(t, "hh:mm:ss");
    printf("%s\n", s1);  // "14:30:45"
    xrtFree(s1);
    
    // mm 在开头表示月份
    str s2 = xrtTimeFormat(t, "mm/dd/yyyy");
    printf("%s\n", s2);  // "03/15/2025"
    xrtFree(s2);
    
    // 明确使用 nn 表示分钟
    str s3 = xrtTimeFormat(t, "yyyy-mm-dd hh:nn:ss");
    printf("%s\n", s3);  // "2025-03-15 14:30:45"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeParse

使用自定义格式将字符串解析为时间。

**函数原型：**
```c
XXAPI xtime xrtTimeParse(str sTime, str sFormat);
```

**参数：**
- `sTime` - 要解析的字符串
- `sFormat` - 格式字符串

**返回值：**
- 解析出的时间戳
- 解析失败返回 0

**格式占位符：**

除了 `xrtTimeFormat` 的所有占位符外，还支持以下特殊匹配符：

| 匹配符 | 说明 |
|--------|------|
| `*` | 跳过任意非数字字符 |
| `.` | 匹配至少1个非数字字符 |
| `?` | 跳过1个字符 |
| 空格 | 跳过任意空白字符 |

**高容错特性：**
- 自动跳过前缀冗余文本
- 支持锚点定位（通过固定文本定位时间部分）
- 智能识别常见时间模式

**基本示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 解析标准格式
    xtime t1 = xrtTimeParse("2025-03-15 14:30:45", "yyyy-mm-dd hh:nn:ss");
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("%s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // 解析12小时制
    xtime t2 = xrtTimeParse("03/15/2025 02:30:45 PM", "mm/dd/yyyy HH:nn:ss AP");
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_DATETIME);
    printf("%s\n", s2);  // "2025-03-15 14:30:45"
    xrtFree(s2);
    
    // 解析英文月份
    xtime t3 = xrtTimeParse("Mar 15, 2025", "mmm d, yyyy");
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATE);
    printf("%s\n", s3);  // "2025-03-15"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

**冗余文本处理示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 自动跳过前缀冗余文本
    xtime t1 = xrtTimeParse("当前时间为：2025年3月15日 14:30:45", 
                            "yyyy年m月d日 hh:nn:ss");
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("解析1: %s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // 使用锚点定位
    xtime t2 = xrtTimeParse("中国北京时间 UTC+8  14:30:45", "UTC+8  hh:nn:ss");
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_TIME);
    printf("解析2: %s\n", s2);  // "14:30:45"
    xrtFree(s2);
    
    // 解析日志格式
    xtime t3 = xrtTimeParse("[2025-03-15 14:30:45] INFO: 系统启动", 
                            "[yyyy-mm-dd hh:nn:ss]");
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATETIME);
    printf("解析3: %s\n", s3);  // "2025-03-15 14:30:45"
    xrtFree(s3);
    
    // 使用通配符
    xtime t4 = xrtTimeParse("Date: 2025/03/15", "Date: yyyy/mm/dd");
    str s4 = xrtTimeToStr(t4, XRT_TIME_FORMAT_DATE);
    printf("解析4: %s\n", s4);  // "2025-03-15"
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 解析网站采集的时间
xtime ParseWebTime(str rawText) {
    // 尝试多种格式
    xtime t = xrtTimeParse(rawText, "yyyy-mm-dd hh:nn:ss");
    if (t > 0) return t;
    
    t = xrtTimeParse(rawText, "yyyy/mm/dd hh:nn:ss");
    if (t > 0) return t;
    
    t = xrtTimeParse(rawText, "mmm d, yyyy");
    if (t > 0) return t;
    
    return 0;  // 解析失败
}

int main() {
    xrtInit();
    
    // 各种采集来源
    const char* samples[] = {
        "发布时间：2025-03-15 14:30:45",
        "Posted on: 2025/03/15 14:30:45",
        "Last updated: Mar 15, 2025"
    };
    
    for (int i = 0; i < 3; i++) {
        xtime t = ParseWebTime((str)samples[i]);
        if (t > 0) {
            str s = xrtTimeToStr(t, XRT_TIME_FORMAT_DATETIME);
            printf("%s -> %s\n", samples[i], s);
            xrtFree(s);
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeApprox

判断两个时间是否约等于（在容差范围内）。

**函数原型：**
```c
XXAPI bool xrtTimeApprox(xtime a, xtime b);
```

**参数：**
- `a` - 第一个时间戳
- `b` - 第二个时间戳

**返回值：**
- `TRUE` - 两时间差值在容差范围内
- `FALSE` - 两时间差值超出容差

**配置：**
- 使用 `xCore.iApproxTimeTol` 配置容差（秒）
- 时间约等于仅支持差值模式

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 0);
    xtime t2 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 3);
    xtime t3 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 10);
    
    // 容差 5 秒
    xCore.iApproxTimeTol = 5;
    
    printf("12:00:00 ~ 12:00:03: %s\n", xrtTimeApprox(t1, t2) ? "TRUE" : "FALSE");  // TRUE (差 3 秒)
    printf("12:00:00 ~ 12:00:10: %s\n", xrtTimeApprox(t1, t3) ? "TRUE" : "FALSE");  // FALSE (差 10 秒)
    
    // 容差 1 分钟
    xCore.iApproxTimeTol = 60;
    printf("12:00:00 ~ 12:00:10 (1分钟容差): %s\n", xrtTimeApprox(t1, t3) ? "TRUE" : "FALSE");  // TRUE
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
- 日志时间比对（允许小误差）
- 定时任务检查（近似时间判断）
- 缓存过期检查（约等于超时时间）

---

## 时间提取

### xrtYear

获取时间中的年份。

**函数原型：**
```c
XXAPI int64 xrtYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 年份（负数表示公元前）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int64 year = xrtYear(now);
    printf("当前年份: %" PRId64 "\n", year);  // 2025
    
    xrtUnit();
    return 0;
}
```

---

### xrtMonth

获取时间中的月份。

**函数原型：**
```c
XXAPI int xrtMonth(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 月份（1-12）

---

### xrtDay

获取时间中的日期。

**函数原型：**
```c
XXAPI int xrtDay(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 日期（1-31）

---

### xrtHour

获取时间中的小时。

**函数原型：**
```c
XXAPI int xrtHour(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 小时（0-23）

---

### xrtMinute

获取时间中的分钟。

**函数原型：**
```c
XXAPI int xrtMinute(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 分钟（0-59）

---

### xrtSecond

获取时间中的秒。

**函数原型：**
```c
XXAPI int xrtSecond(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 秒（0-59）

---

### xrtWeekday

获取时间的星期。

**函数原型：**
```c
XXAPI int xrtWeekday(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 星期（0=星期日，1=星期一，...，6=星期六）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int weekday = xrtWeekday(now);
    const char* days[] = {"日", "一", "二", "三", "四", "五", "六"};
    printf("今天是星期%s\n", days[weekday]);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDayOfYear

获取时间是当年的第几天。

**函数原型：**
```c
XXAPI int xrtDayOfYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 当年第几天（1-366）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int day_of_year = xrtDayOfYear(now);
    printf("今天是今年第 %d 天\n", day_of_year);
    
    // 1月1日是第1天
    xtime jan1 = xrtDateSerial(2025, 1, 1);
    printf("1月1日: 第 %d 天\n", xrtDayOfYear(jan1));  // 1
    
    xrtUnit();
    return 0;
}
```

---

### xrtQuarter

获取时间的季度。

**函数原型：**
```c
XXAPI int xrtQuarter(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 季度（1-4）
  - 1-3月 → 第1季度
  - 4-6月 → 第2季度
  - 7-9月 → 第3季度
  - 10-12月 → 第4季度

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int quarter = xrtQuarter(now);
    printf("当前是第 %d 季度\n", quarter);
    
    // 各季度示例
    printf("1月: Q%d\n", xrtQuarter(xrtDateSerial(2025, 1, 15)));   // Q1
    printf("5月: Q%d\n", xrtQuarter(xrtDateSerial(2025, 5, 15)));   // Q2
    printf("8月: Q%d\n", xrtQuarter(xrtDateSerial(2025, 8, 15)));   // Q3
    printf("11月: Q%d\n", xrtQuarter(xrtDateSerial(2025, 11, 15))); // Q4
    
    xrtUnit();
    return 0;
}
```

---

### xrtDatePart

获取日期部分（去除时间）。

**函数原型：**
```c
XXAPI xtime xrtDatePart(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 仅包含日期的时间戳（时分秒为 00:00:00）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();  // 例如: 2025-01-10 15:30:45
    xtime datePart = xrtDatePart(now);  // 2025-01-10 00:00:00
    
    str s1 = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(datePart, XRT_TIME_FORMAT_DATETIME);
    
    printf("原始时间: %s\n", s1);
    printf("日期部分: %s\n", s2);
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 等效于 `iTime - (iTime % XRT_TIME_DAY)`
- 常用于日期比较（忽略时间部分）

---

### xrtTimePart

获取时间部分（去除日期）。

**函数原型：**
```c
XXAPI xtime xrtTimePart(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 当日已过的秒数（0-86399）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();  // 例如: 2025-01-10 15:30:45
    xtime timePart = xrtTimePart(now);  // 55845秒 (15*3600 + 30*60 + 45)
    
    printf("当日已过秒数: %" PRId64 "\n", timePart);
    
    str timeStr = xrtTimeToStr(timePart, XRT_TIME_FORMAT_TIME);
    printf("时间部分: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 等效于 `iTime % XRT_TIME_DAY`
- 常用于时间比较（忽略日期部分）

**完整时间提取示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    printf("当前时间: %" PRId64 "-", xrtYear(now));
    printf("%02d-%02d ", xrtMonth(now), xrtDay(now));
    printf("%02d:%02d:%02d\n", xrtHour(now), xrtMinute(now), xrtSecond(now));
    printf("星期: %d\n", xrtWeekday(now));
    printf("年内第: %d 天\n", xrtDayOfYear(now));
    printf("季度: Q%d\n", xrtQuarter(now));
    
    xrtUnit();
    return 0;
}
```

---

## 时间解码

### xrtDecodeSerial

一次性解码时间戳的所有组件。

**函数原型：**
```c
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, 
                           int* pHour, int* pMinute, int* pSecond, 
                           int* pWeekday, int* pDayOfYear);
```

**参数：**
- `iTime` - 时间戳
- `pYear` - 年份输出指针（可NULL）
- `pMonth` - 月份输出指针（可NULL）
- `pDay` - 日期输出指针（可NULL）
- `pHour` - 小时输出指针（可NULL）
- `pMinute` - 分钟输出指针（可NULL）
- `pSecond` - 秒输出指针（可NULL）
- `pWeekday` - 星期输出指针（可NULL）
- `pDayOfYear` - 年内天数输出指针（可NULL）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // 解码所有字段
    int64 year;
    int month, day, hour, minute, second, weekday, dayOfYear;
    xrtDecodeSerial(now, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);
    
    printf("完整解码:\n");
    printf("日期: %" PRId64 "-%02d-%02d\n", year, month, day);
    printf("时间: %02d:%02d:%02d\n", hour, minute, second);
    printf("星期: %d\n", weekday);
    printf("年内第: %d 天\n", dayOfYear);
    
    // 只解码部分字段
    int64 y;
    int m, d;
    xrtDecodeSerial(now, &y, &m, &d, NULL, NULL, NULL, NULL, NULL);
    printf("仅日期: %" PRId64 "-%02d-%02d\n", y, m, d);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 比多次调用单独的提取函数效率更高
- 不需要的字段可传 `NULL`

---

## 时间比较

### xrtIsSameDay

判断两个时间是否是同一天。

**函数原型：**
```c
XXAPI bool xrtIsSameDay(xtime iTime1, xtime iTime2);
```

**参数：**
- `iTime1` - 时间戳1
- `iTime2` - 时间戳2

**返回值：**
- `TRUE` - 同一天
- `FALSE` - 不同天

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateTimeSerial(2025, 3, 15, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 3, 15, 20, 0, 0);
    xtime t3 = xrtDateTimeSerial(2025, 3, 16, 8, 0, 0);
    
    printf("t1和t2同一天: %s\n", xrtIsSameDay(t1, t2) ? "是" : "否");  // 是
    printf("t1和t3同一天: %s\n", xrtIsSameDay(t1, t3) ? "是" : "否");  // 否
    
    xrtUnit();
    return 0;
}
```

---

### xrtIsSameMonth

判断两个时间是否是同一月。

**函数原型：**
```c
XXAPI bool xrtIsSameMonth(xtime iTime1, xtime iTime2);
```

**参数：**
- `iTime1` - 时间戳1
- `iTime2` - 时间戳2

**返回值：**
- `TRUE` - 同一月（同年同月）
- `FALSE` - 不同月

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateSerial(2025, 3, 1);
    xtime t2 = xrtDateSerial(2025, 3, 31);
    xtime t3 = xrtDateSerial(2025, 4, 1);
    xtime t4 = xrtDateSerial(2024, 3, 15);
    
    printf("3月1日和3月31日: %s\n", xrtIsSameMonth(t1, t2) ? "同月" : "不同");  // 同月
    printf("3月31日和4月1日: %s\n", xrtIsSameMonth(t2, t3) ? "同月" : "不同");  // 不同
    printf("2025年3月和2024年3月: %s\n", xrtIsSameMonth(t1, t4) ? "同月" : "不同");  // 不同
    
    xrtUnit();
    return 0;
}
```

---

### xrtIsSameYear

判断两个时间是否是同一年。

**函数原型：**
```c
XXAPI bool xrtIsSameYear(xtime iTime1, xtime iTime2);
```

**参数：**
- `iTime1` - 时间戳1
- `iTime2` - 时间戳2

**返回值：**
- `TRUE` - 同一年
- `FALSE` - 不同年

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateSerial(2025, 1, 1);
    xtime t2 = xrtDateSerial(2025, 12, 31);
    xtime t3 = xrtDateSerial(2026, 1, 1);
    
    printf("2025/1/1和2025/12/31: %s\n", xrtIsSameYear(t1, t2) ? "同年" : "不同");  // 同年
    printf("2025/12/31和2026/1/1: %s\n", xrtIsSameYear(t2, t3) ? "同年" : "不同");  // 不同
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeInRange

判断时间是否在指定区间内。

**函数原型：**
```c
XXAPI bool xrtTimeInRange(xtime iTime, xtime iStart, xtime iEnd);
```

**参数：**
- `iTime` - 要判断的时间
- `iStart` - 区间开始时间
- `iEnd` - 区间结束时间

**返回值：**
- `TRUE` - 在区间内（包含边界）
- `FALSE` - 不在区间内

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateTimeSerial(2025, 3, 1, 9, 0, 0);
    xtime end = xrtDateTimeSerial(2025, 3, 1, 18, 0, 0);
    
    xtime t1 = xrtDateTimeSerial(2025, 3, 1, 12, 0, 0);  // 中午
    xtime t2 = xrtDateTimeSerial(2025, 3, 1, 20, 0, 0);  // 晚上
    xtime t3 = xrtDateTimeSerial(2025, 3, 1, 9, 0, 0);   // 正好开始
    
    printf("12:00 在工作时间内: %s\n", xrtTimeInRange(t1, start, end) ? "是" : "否");  // 是
    printf("20:00 在工作时间内: %s\n", xrtTimeInRange(t2, start, end) ? "是" : "否");  // 否
    printf("09:00 在工作时间内: %s\n", xrtTimeInRange(t3, start, end) ? "是" : "否");  // 是
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeRangeOverlap

判断两个时间区间是否重叠。

**函数原型：**
```c
XXAPI bool xrtTimeRangeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2);
```

**参数：**
- `iStart1`, `iEnd1` - 第一个时间区间
- `iStart2`, `iEnd2` - 第二个时间区间

**返回值：**
- `TRUE` - 两个区间有重叠
- `FALSE` - 两个区间不重叠

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 会议1: 9:00 - 11:00
    xtime m1_start = xrtDateTimeSerial(2025, 3, 1, 9, 0, 0);
    xtime m1_end = xrtDateTimeSerial(2025, 3, 1, 11, 0, 0);
    
    // 会议2: 10:00 - 12:00 （与会议1重叠）
    xtime m2_start = xrtDateTimeSerial(2025, 3, 1, 10, 0, 0);
    xtime m2_end = xrtDateTimeSerial(2025, 3, 1, 12, 0, 0);
    
    // 会议3: 14:00 - 16:00 （与会议1不重叠）
    xtime m3_start = xrtDateTimeSerial(2025, 3, 1, 14, 0, 0);
    xtime m3_end = xrtDateTimeSerial(2025, 3, 1, 16, 0, 0);
    
    bool overlap12 = xrtTimeRangeOverlap(m1_start, m1_end, m2_start, m2_end);
    bool overlap13 = xrtTimeRangeOverlap(m1_start, m1_end, m3_start, m3_end);
    
    printf("会议1和会议2冲突: %s\n", overlap12 ? "是" : "否");  // 是
    printf("会议1和会议3冲突: %s\n", overlap13 ? "是" : "否");  // 否
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
- 会议室预约冲突检测
- 资源占用时间冲突检查
- 日程安排合理性验证

---

## 边界日期

### xrtFirstDayOfMonth

获取月份的第一天。

**函数原型：**
```c
XXAPI xtime xrtFirstDayOfMonth(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 该月第一天的时间戳（00:00:00）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateSerial(2025, 3, 15);
    xtime first = xrtFirstDayOfMonth(t);
    
    str s = xrtTimeToStr(first, XRT_TIME_FORMAT_DATE);
    printf("月初: %s\n", s);  // "2025-03-01"
    xrtFree(s);
    
    xrtUnit();
    return 0;
}
```

---

### xrtLastDayOfMonth

获取月份的最后一天。

**函数原型：**
```c
XXAPI xtime xrtLastDayOfMonth(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 该月最后一天的时间戳（00:00:00）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 平年2月
    xtime t1 = xrtDateSerial(2025, 2, 15);
    xtime last1 = xrtLastDayOfMonth(t1);
    str s1 = xrtTimeToStr(last1, XRT_TIME_FORMAT_DATE);
    printf("2025年2月月底: %s\n", s1);  // "2025-02-28"
    xrtFree(s1);
    
    // 闰年2月
    xtime t2 = xrtDateSerial(2024, 2, 15);
    xtime last2 = xrtLastDayOfMonth(t2);
    str s2 = xrtTimeToStr(last2, XRT_TIME_FORMAT_DATE);
    printf("2024年2月月底: %s\n", s2);  // "2024-02-29"
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFirstDayOfYear

获取年份的第一天。

**函数原型：**
```c
XXAPI xtime xrtFirstDayOfYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 该年1月1日的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateSerial(2025, 7, 15);
    xtime first = xrtFirstDayOfYear(t);
    
    str s = xrtTimeToStr(first, XRT_TIME_FORMAT_DATE);
    printf("年初: %s\n", s);  // "2025-01-01"
    xrtFree(s);
    
    xrtUnit();
    return 0;
}
```

---

### xrtLastDayOfYear

获取年份的最后一天。

**函数原型：**
```c
XXAPI xtime xrtLastDayOfYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 该年12月31日的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateSerial(2025, 7, 15);
    xtime last = xrtLastDayOfYear(t);
    
    str s = xrtTimeToStr(last, XRT_TIME_FORMAT_DATE);
    printf("年底: %s\n", s);  // "2025-12-31"
    xrtFree(s);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFirstDayOfWeek

获取周的第一天。

**函数原型：**
```c
XXAPI xtime xrtFirstDayOfWeek(xtime iTime, int iStartDay);
```

**参数：**
- `iTime` - 时间戳
- `iStartDay` - 一周的开始日（0=周日, 1=周一, ...）

**返回值：**
- 本周第一天的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 假设今天是 2025-03-12 (周三)
    xtime t = xrtDateSerial(2025, 3, 12);
    
    // 一周从周日开始
    xtime first_sun = xrtFirstDayOfWeek(t, 0);
    str s1 = xrtTimeToStr(first_sun, XRT_TIME_FORMAT_DATE);
    printf("本周开始(周日): %s\n", s1);  // "2025-03-09"
    xrtFree(s1);
    
    // 一周从周一开始 (ISO标准)
    xtime first_mon = xrtFirstDayOfWeek(t, 1);
    str s2 = xrtTimeToStr(first_mon, XRT_TIME_FORMAT_DATE);
    printf("本周开始(周一): %s\n", s2);  // "2025-03-10"
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtLastDayOfWeek

获取周的最后一天。

**函数原型：**
```c
XXAPI xtime xrtLastDayOfWeek(xtime iTime, int iStartDay);
```

**参数：**
- `iTime` - 时间戳
- `iStartDay` - 一周的开始日（0=周日, 1=周一, ...）

**返回值：**
- 本周最后一天的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 假设今天是 2025-03-12 (周三)
    xtime t = xrtDateSerial(2025, 3, 12);
    
    // 一周从周日开始，结束于周六
    xtime last_sat = xrtLastDayOfWeek(t, 0);
    str s1 = xrtTimeToStr(last_sat, XRT_TIME_FORMAT_DATE);
    printf("本周结束(周六): %s\n", s1);  // "2025-03-15"
    xrtFree(s1);
    
    // 一周从周一开始，结束于周日 (ISO标准)
    xtime last_sun = xrtLastDayOfWeek(t, 1);
    str s2 = xrtTimeToStr(last_sun, XRT_TIME_FORMAT_DATE);
    printf("本周结束(周日): %s\n", s2);  // "2025-03-16"
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

## 周相关函数

### xrtWeekOfYear

获取当年第几周（ISO周数，周一为一周开始）。

**函数原型：**
```c
XXAPI int xrtWeekOfYear(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 周数（1-53）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateSerial(2025, 1, 1);
    xtime t2 = xrtDateSerial(2025, 6, 15);
    xtime t3 = xrtDateSerial(2025, 12, 31);
    
    printf("2025-01-01 是第 %d 周\n", xrtWeekOfYear(t1));
    printf("2025-06-15 是第 %d 周\n", xrtWeekOfYear(t2));
    printf("2025-12-31 是第 %d 周\n", xrtWeekOfYear(t3));
    
    xrtUnit();
    return 0;
}
```

---

### xrtWeekOfMonth

获取当月第几周（周一为一周开始）。

**函数原型：**
```c
XXAPI int xrtWeekOfMonth(xtime iTime);
```

**参数：**
- `iTime` - 时间戳

**返回值：**
- 周数（1-6）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateSerial(2025, 3, 1);
    xtime t2 = xrtDateSerial(2025, 3, 15);
    xtime t3 = xrtDateSerial(2025, 3, 31);
    
    printf("3月1日 是本月第 %d 周\n", xrtWeekOfMonth(t1));
    printf("3月15日 是本月第 %d 周\n", xrtWeekOfMonth(t2));
    printf("3月31日 是本月第 %d 周\n", xrtWeekOfMonth(t3));
    
    xrtUnit();
    return 0;
}
```

---

## 时区处理

### xrtNowUTC

获取当前UTC时间。

**函数原型：**
```c
XXAPI xtime xrtNowUTC();
```

**返回值：**
- 当前UTC时间的时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime local = xrtNow();
    xtime utc = xrtNowUTC();
    
    str s1 = xrtTimeToStr(local, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(utc, XRT_TIME_FORMAT_DATETIME);
    
    printf("本地时间: %s\n", s1);
    printf("UTC时间:  %s\n", s2);
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

---

### xrtTimezoneOffset

获取本地时区偏移（秒）。

**函数原型：**
```c
XXAPI int xrtTimezoneOffset();
```

**返回值：**
- 时区偏移秒数（东时区为正，西时区为负）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int offset = xrtTimezoneOffset();
    int hours = offset / 3600;
    int minutes = (offset % 3600) / 60;
    
    printf("时区偏移: UTC%+d:%02d\n", hours, minutes);
    // 中国时区: "UTC+8:00"
    
    xrtUnit();
    return 0;
}
```

---

### xrtUTCToLocal

UTC时间转换为本地时间。

**函数原型：**
```c
XXAPI xtime xrtUTCToLocal(xtime utc);
```

**参数：**
- `utc` - UTC时间戳

**返回值：**
- 本地时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTC 2025-03-15 06:00:00
    xtime utc = xrtDateTimeSerial(2025, 3, 15, 6, 0, 0);
    xtime local = xrtUTCToLocal(utc);
    
    str s1 = xrtTimeToStr(utc, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(local, XRT_TIME_FORMAT_DATETIME);
    
    printf("UTC:  %s\n", s1);    // "2025-03-15 06:00:00"
    printf("本地: %s\n", s2);   // "2025-03-15 14:00:00" (中国UTC+8)
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

---

### xrtLocalToUTC

本地时间转换为UTC时间。

**函数原型：**
```c
XXAPI xtime xrtLocalToUTC(xtime local);
```

**参数：**
- `local` - 本地时间戳

**返回值：**
- UTC时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 本地 2025-03-15 14:00:00
    xtime local = xrtDateTimeSerial(2025, 3, 15, 14, 0, 0);
    xtime utc = xrtLocalToUTC(local);
    
    str s1 = xrtTimeToStr(local, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(utc, XRT_TIME_FORMAT_DATETIME);
    
    printf("本地: %s\n", s1);   // "2025-03-15 14:00:00"
    printf("UTC:  %s\n", s2);    // "2025-03-15 06:00:00" (中国UTC+8)
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

---

## Unix时间戳

### xrtToUnixTime

xtime转换为Unix时间戳。

**函数原型：**
```c
XXAPI int64 xrtToUnixTime(xtime iTime);
```

**参数：**
- `iTime` - xtime时间戳

**返回值：**
- Unix时间戳（1970年1月1日以来的秒数）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateTimeSerial(2025, 3, 15, 12, 0, 0);
    int64 unix_ts = xrtToUnixTime(t);
    
    printf("Unix时间戳: %" PRId64 "\n", unix_ts);
    
    // 验证: 1970-01-01 00:00:00 应该是 0
    xtime epoch = xrtDateTimeSerial(1970, 1, 1, 0, 0, 0);
    printf("Epoch: %" PRId64 "\n", xrtToUnixTime(epoch));  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtFromUnixTime

Unix时间戳转换为xtime。

**函数原型：**
```c
XXAPI xtime xrtFromUnixTime(int64 unixTime);
```

**参数：**
- `unixTime` - Unix时间戳

**返回值：**
- xtime时间戳

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 将Unix时间戳转换为xtime
    int64 unix_ts = 1742025600;  // 某个时间点
    xtime t = xrtFromUnixTime(unix_ts);
    
    str s = xrtTimeToStr(t, XRT_TIME_FORMAT_DATETIME);
    printf("时间: %s\n", s);
    xrtFree(s);
    
    // 验证双向转换
    xtime now = xrtNow();
    int64 unix_now = xrtToUnixTime(now);
    xtime back = xrtFromUnixTime(unix_now);
    printf("双向转换验证: %s\n", (now == back) ? "通过" : "失败");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- xtime基于公元元年，Unix时间戳基于1970年
- 内部使用 `XRT_TIME_19700101` 常量进行转换

---

## 相对时间

### xrtRelativeTime

获取相对时间描述（如“3天前”、“2小时后”）。

**函数原型：**
```c
XXAPI str xrtRelativeTime(xtime iTime, xtime iBaseTime);
```

**参数：**
- `iTime` - 目标时间
- `iBaseTime` - 基准时间（通常传 `xrtNow()`）

**返回值：**
- 相对时间描述字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**输出格式：**
- `刚刚` - 60秒内
- `N分钟前/后` - 60分钟内
- `N小时前/后` - 24小时内
- `N天前/后` - 30天内
- `N个月前/后` - 12个月内
- `N年前/后` - 超过12个月

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // 30分钟前
    xtime t1 = xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, -30, now);
    str s1 = xrtRelativeTime(t1, now);
    printf("%s\n", s1);  // "30分钟前"
    xrtFree(s1);
    
    // 3天后
    xtime t2 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 3, now);
    str s2 = xrtRelativeTime(t2, now);
    printf("%s\n", s2);  // "3天后"
    xrtFree(s2);
    
    // 2年前
    xtime t3 = xrtDateAdd(XRT_TIME_INTERVAL_YEAR, -2, now);
    str s3 = xrtRelativeTime(t3, now);
    printf("%s\n", s3);  // "2年前"
    xrtFree(s3);
    
    // 刚刚
    str s4 = xrtRelativeTime(now, now);
    printf("%s\n", s4);  // "刚刚"
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 文章发布时间显示
void PrintArticleTime(xtime publishTime) {
    str relative = xrtRelativeTime(publishTime, xrtNow());
    printf("发布于: %s\n", relative);
    xrtFree(relative);
}

int main() {
    xrtInit();
    
    // 模拟不同时间发布的文章
    xtime now = xrtNow();
    
    PrintArticleTime(xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, -5, now));   // "发布于: 5分钟前"
    PrintArticleTime(xrtDateAdd(XRT_TIME_INTERVAL_HOUR, -3, now));     // "发布于: 3小时前"
    PrintArticleTime(xrtDateAdd(XRT_TIME_INTERVAL_DAY, -7, now));      // "发布于: 7天前"
    
    xrtUnit();
    return 0;
}
```

---

## 日期工具

### xrtIsLeapYear

判断是否为闰年。

**函数原型：**
```c
XXAPI bool xrtIsLeapYear(int iYear);
```

**参数：**
- `iYear` - 年份

**返回值：**
- `TRUE` - 闰年
- `FALSE` - 平年

**判断规则：**
- 能被 400 整除 → 闰年
- 能被 100 整除 → 平年
- 能被 4 整除 → 闰年
- 其他 → 平年

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %s\n", xrtIsLeapYear(2024) ? "闰年" : "平年");  // 闰年
    printf("2025: %s\n", xrtIsLeapYear(2025) ? "闰年" : "平年");  // 平年
    printf("2000: %s\n", xrtIsLeapYear(2000) ? "闰年" : "平年");  // 闰年
    printf("1900: %s\n", xrtIsLeapYear(1900) ? "闰年" : "平年");  // 平年
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInMonth

获取某年某月有多少天。

**函数原型：**
```c
XXAPI int xrtDaysInMonth(int iYear, int iMonth);
```

**参数：**
- `iYear` - 年份
- `iMonth` - 月份（1-12）

**返回值：**
- 天数（28-31）
- 月份无效返回 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 2月天数取决于是否闰年
    printf("2024年2月: %d 天\n", xrtDaysInMonth(2024, 2));  // 29
    printf("2025年2月: %d 天\n", xrtDaysInMonth(2025, 2));  // 28
    
    // 其他月份
    printf("1月: %d 天\n", xrtDaysInMonth(2025, 1));   // 31
    printf("4月: %d 天\n", xrtDaysInMonth(2025, 4));   // 30
    printf("12月: %d 天\n", xrtDaysInMonth(2025, 12)); // 31
    
    // 无效月份
    printf("无效月份: %d\n", xrtDaysInMonth(2025, 13));  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInYear

获取某年有多少天。

**函数原型：**
```c
XXAPI int xrtDaysInYear(int iYear);
```

**参数：**
- `iYear` - 年份

**返回值：**
- 天数（365 或 366）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024年: %d 天\n", xrtDaysInYear(2024));  // 366
    printf("2025年: %d 天\n", xrtDaysInYear(2025));  // 365
    
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 性能计时器

```c
#include "xrt.h"
#include <stdio.h>

void ProcessData() {
    // 模拟耗时操作
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    ProcessData();
    double elapsed = xrtTimer() - start;
    
    printf("处理耗时: %.6f 秒\n", elapsed);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 日志时间戳

```c
#include "xrt.h"
#include <stdio.h>

void Log(str message) {
    str timestamp = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
    printf("[%s] %s\n", timestamp, message);
    xrtFree(timestamp);
}

int main() {
    xrtInit();
    
    Log((str)"应用程序启动");
    xrtSleep(1000);
    Log((str)"执行中...");
    xrtSleep(500);
    Log((str)"应用程序结束");
    
    xrtUnit();
    return 0;
}
```

---

### 3. 过期检查

```c
#include "xrt.h"
#include <stdio.h>

bool IsExpired(xtime expire_time) {
    return xrtNow() > expire_time;
}

int main() {
    xrtInit();
    
    // 设置过期时间为 2025年12月31日
    xtime expire = xrtDateSerial(2025, 12, 31);
    
    if (IsExpired(expire)) {
        printf("许可证已过期\n");
    } else {
        int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, xrtNow(), expire);
        printf("许可证剩余 %" PRId64 " 天\n", days);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. 生日计算

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 假设生日是 1990年5月15日
    xtime birthday = xrtDateSerial(1990, 5, 15);
    xtime today = xrtDate();
    
    // 计算年龄
    int64 years = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, birthday, today);
    printf("年龄: %" PRId64 " 岁\n", years);
    
    // 计算总天数
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, birthday, today);
    printf("已经活了: %" PRId64 " 天\n", days);
    
    // 下一个生日
    int64 this_year = xrtYear(today);
    xtime next_bday = xrtDateSerial(this_year, 5, 15);
    if (next_bday <= today) {
        next_bday = xrtDateSerial(this_year + 1, 5, 15);
    }
    int64 days_until = xrtDateDiff(XRT_TIME_INTERVAL_DAY, today, next_bday);
    printf("距离下次生日: %" PRId64 " 天\n", days_until);
    
    xrtUnit();
    return 0;
}
```

---

### 5. 工作日计算

```c
#include "xrt.h"
#include <stdio.h>

// 计算工作日数（不含周末）
int CountWorkdays(xtime start, xtime end) {
    int workdays = 0;
    xtime current = start;
    
    while (current <= end) {
        int weekday = xrtWeekday(current);
        if (weekday != 0 && weekday != 6) {  // 非周末
            workdays++;
        }
        current = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, current);
    }
    
    return workdays;
}

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 1, 31);
    
    int workdays = CountWorkdays(start, end);
    printf("2025年1月工作日: %d 天\n", workdays);
    
    xrtUnit();
    return 0;
}
```

---

### 6. 自定义日志格式

```c
#include "xrt.h"
#include <stdio.h>

void LogMessage(str level, str message) {
    xtime now = xrtNow();
    str timestamp = xrtTimeFormat(now, "yyyy-mm-dd hh:nn:ss");
    printf("[%s] [%s] %s\n", timestamp, level, message);
    xrtFree(timestamp);
}

int main() {
    xrtInit();
    
    LogMessage("INFO", "应用程序启动");
    xrtSleep(500);
    LogMessage("DEBUG", "加载配置文件");
    xrtSleep(300);
    LogMessage("WARN", "配置项缺失，使用默认值");
    LogMessage("INFO", "应用程序就绪");
    
    xrtUnit();
    return 0;
}
```

---

### 7. 时区转换

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 获取当前时间
    xtime local_now = xrtNow();
    xtime utc_now = xrtNowUTC();
    
    str s1 = xrtTimeToStr(local_now, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(utc_now, XRT_TIME_FORMAT_DATETIME);
    
    printf("本地时间: %s\n", s1);
    printf("UTC时间:  %s\n", s2);
    printf("时区偏移: UTC%+d\n", xrtTimezoneOffset() / 3600);
    
    xrtFree(s1);
    xrtFree(s2);
    
    // 时间转换应用: 假设有一个约定的UTC时间
    xtime meeting_utc = xrtDateTimeSerial(2025, 3, 15, 14, 0, 0);  // UTC 14:00
    xtime meeting_local = xrtUTCToLocal(meeting_utc);
    
    str s3 = xrtTimeFormat(meeting_local, "yyyy年m月d日 HH:nn AP");
    printf("会议本地时间: %s\n", s3);
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

### 8. 解析网页采集的时间

```c
#include "xrt.h"
#include <stdio.h>

// 尝试多种格式解析时间
xtime ParseFlexibleTime(str timeText) {
    // 尝试常见格式
    static const char* formats[] = {
        "yyyy-mm-dd hh:nn:ss",
        "yyyy/mm/dd hh:nn:ss",
        "yyyy年m月d日 hh:nn:ss",
        "yyyy年m月d日",
        "mmm d, yyyy",
        "mmmm d, yyyy",
        "mm/dd/yyyy",
        "dd/mm/yyyy"
    };
    
    int numFormats = sizeof(formats) / sizeof(formats[0]);
    for (int i = 0; i < numFormats; i++) {
        xtime t = xrtTimeParse(timeText, (str)formats[i]);
        if (t > 0) return t;
    }
    
    return 0;  // 解析失败
}

int main() {
    xrtInit();
    
    const char* samples[] = {
        "发布时间: 2025年3月15日 14:30:00",
        "Published: March 15, 2025",
        "Date: 2025-03-15 14:30:00",
        "更新于: Mar 15, 2025"
    };
    
    int numSamples = sizeof(samples) / sizeof(samples[0]);
    for (int i = 0; i < numSamples; i++) {
        xtime t = ParseFlexibleTime((str)samples[i]);
        if (t > 0) {
            str s = xrtTimeToStr(t, XRT_TIME_FORMAT_DATETIME);
            printf("\"%s\" -> %s\n", samples[i], s);
            xrtFree(s);
        } else {
            printf("\"%s\" -> 解析失败\n", samples[i]);
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [String 字符串处理](api-string.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#time-时间处理库)

</div>
