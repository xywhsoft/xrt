# Time 时间处理库

> 日期时间计算和格式化功能

[返回索引](README.md)

---

## 📑 目录

- [时间获取](#时间获取)
- [时间构建](#时间构建)
- [时间计算](#时间计算)
- [时间格式化](#时间格式化)
- [时间解析](#时间解析)

---

## 时间获取

### xrtNow

获取当前时间

**函数原型：**
```c
XXAPI xtime xrtNow();
```

**返回值：**
- 当前时间戳（秒）

**示例：**
```c
xtime now = xrtNow();
printf("Timestamp: %lld\n", now);
```

---

### xrtTimer

高精度计时器

**函数原型：**
```c
XXAPI double xrtTimer();
```

**返回值：**
- 高精度时间（秒，浮点数）

**说明：**
- Windows: 使用 `QueryPerformanceCounter`
- Linux: 使用 `clock_gettime`

**示例：**
```c
double start = xrtTimer();
// ... 执行操作
double end = xrtTimer();
printf("Elapsed: %.6f seconds\n", end - start);
```

---

## 时间构建

### xrtDateSerial

构建日期

**函数原型：**
```c
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
```

**参数：**
- `iYear` - 年份
- `iMonth` - 月份（1-12）
- `iDay` - 日期（1-31）

**返回值：**
- 时间戳

**示例：**
```c
xtime date = xrtDateSerial(2025, 1, 10);
```

---

### xrtTimeSerial

构建时间

**函数原型：**
```c
XXAPI xtime xrtTimeSerial(int iHour, int iMin, int iSec);
```

**参数：**
- `iHour` - 小时（0-23）
- `iMin` - 分钟（0-59）
- `iSec` - 秒（0-59）

**返回值：**
- 时间戳（当日秒数）

---

## 时间计算

### xrtDateAdd

时间加减

**函数原型：**
```c
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
```

**参数：**
- `interval` - 间隔类型
  - `0` - 秒
  - `1` - 分钟
  - `2` - 小时
  - `3` - 天
  - `4` - 月
  - `5` - 年
- `iValue` - 增量（可为负数）
- `iTime` - 基准时间

**返回值：**
- 计算后的时间戳

**示例：**
```c
xtime now = xrtNow();
xtime tomorrow = xrtDateAdd(3, 1, now);     // +1天
xtime next_week = xrtDateAdd(3, 7, now);    // +7天
xtime last_month = xrtDateAdd(4, -1, now);  // -1月
```

---

### xrtDateDiff

计算时间差

**函数原型：**
```c
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

**参数：**
- `interval` - 间隔单位（同 `xrtDateAdd`）
- `iTime1` - 时间1
- `iTime2` - 时间2

**返回值：**
- 时间差（iTime2 - iTime1）

**示例：**
```c
xtime start = xrtDateSerial(2025, 1, 1);
xtime end = xrtDateSerial(2025, 1, 10);
int64 days = xrtDateDiff(3, start, end);
printf("Days: %lld\n", days);  // 9
```

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

## 时间解析

### xrtStrToTime

字符串转时间

**函数原型：**
```c
XXAPI xtime xrtStrToTime(str sTime, size_t iSize);
```

**参数：**
- `sTime` - 时间字符串
- `iSize` - 长度（0自动）

**支持格式：**
- `YYYY-MM-DD HH:MM:SS`
- `YYYY-MM-DD`
- `YYYYMMDDHHMMSS`

**示例：**
```c
xtime t1 = xrtStrToTime("2025-01-10 15:30:45", 0);
xtime t2 = xrtStrToTime("2025-01-10", 0);
```

---

## 使用场景

### 1. 计时器

```c
double start = xrtTimer();
ProcessData();
double elapsed = xrtTimer() - start;
printf("Processed in %.3f seconds\n", elapsed);
```

---

### 2. 日志时间戳

```c
void Log(str message) {
    str timestamp = xrtTimeToStr(xrtNow(), 0);
    printf("[%s] %s\n", timestamp, message);
    xrtFree(timestamp);
}
```

---

### 3. 过期检查

```c
bool IsExpired(xtime expire_time) {
    return xrtNow() > expire_time;
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
