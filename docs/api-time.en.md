# Time Processing Library

> Date/time calculation, formatting, parsing and timezone handling

[English](api-time.en.md) | [中文](api-time.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [Time Retrieval](#time-retrieval)
- [Time Construction](#time-construction)
- [Time Extraction](#time-extraction)
- [Time Decoding](#time-decoding)
- [Time Calculation](#time-calculation)
- [Time Formatting](#time-formatting)
- [Custom Formatting](#custom-formatting)
- [Time Comparison](#time-comparison)
- [Boundary Dates](#boundary-dates)
- [Week Functions](#week-functions)
- [Timezone Handling](#timezone-handling)
- [Unix Timestamp](#unix-timestamp)
- [Relative Time](#relative-time)
- [Date Utilities](#date-utilities)
- [Use Cases](#use-cases)

---

## Constant Definitions

### Time Unit Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XRT_TIME_MINUTE` | `60` | Seconds in 1 minute |
| `XRT_TIME_HOUR` | `3600` | Seconds in 1 hour |
| `XRT_TIME_DAY` | `86400` | Seconds in 1 day |
| `XRT_TIME_YEAR` | `31536000` | Seconds in 1 common year |
| `XRT_TIME_LEAPYEAR` | `31622400` | Seconds in 1 leap year |
| `XRT_TIME_400YEAR` | `12622780800` | Seconds in 400 years (97 leap years + 303 common years) |
| `XRT_TIME_19700101` | `62167219200` | Timestamp of January 1, 1970 |

### Time Interval Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `XRT_TIME_INTERVAL_YEAR` | `1` | Year |
| `XRT_TIME_INTERVAL_MONTH` | `2` | Month |
| `XRT_TIME_INTERVAL_DAY` | `3` | Day |
| `XRT_TIME_INTERVAL_HOUR` | `4` | Hour |
| `XRT_TIME_INTERVAL_MINUTE` | `5` | Minute |
| `XRT_TIME_INTERVAL_SECOND` | `6` | Second |
| `XRT_TIME_INTERVAL_WEEKDAY` | `7` | Week |
| `XRT_TIME_INTERVAL_QUARTER` | `8` | Quarter |

### Time Format Constants

| Constant | Value | Output Format |
|----------|-------|---------------|
| `XRT_TIME_FORMAT_DATETIME` | `0` | `YYYY-MM-DD HH:MM:SS` |
| `XRT_TIME_FORMAT_DATE` | `1` | `YYYY-MM-DD` |
| `XRT_TIME_FORMAT_TIME` | `2` | `HH:MM:SS` |

---

## Time Retrieval

### xrtNow

Get current date and time.

**Prototype:**
```c
XXAPI xtime xrtNow();
```

**Return Value:**
- Timestamp of current date and time (seconds)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    printf("Timestamp: %" PRId64 "\n", now);
    
    // Convert to readable string
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDate

Get current date (without time part).

**Prototype:**
```c
XXAPI xtime xrtDate();
```

**Return Value:**
- Timestamp of current date (hour/minute/second are 00:00:00)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime today = xrtDate();
    str dateStr = xrtTimeToStr(today, XRT_TIME_FORMAT_DATE);
    printf("Today: %s\n", dateStr);
    xrtFree(dateStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTime

Get current time (without date part).

**Prototype:**
```c
XXAPI xtime xrtTime();
```

**Return Value:**
- Seconds elapsed today (0-86399)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime currentTime = xrtTime();
    printf("Seconds elapsed today: %" PRId64 "\n", currentTime);
    
    str timeStr = xrtTimeToStr(currentTime, XRT_TIME_FORMAT_TIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimer

Get high-precision timer (for performance measurement).

**Prototype:**
```c
XXAPI double xrtTimer();
```

**Return Value:**
- High-precision time (seconds, floating point)

**Description:**
- Windows: Uses `QueryPerformanceCounter`
- Linux/macOS: Uses `clock_gettime(CLOCK_MONOTONIC)`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

void HeavyWork() {
    // Simulate time-consuming operation
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    HeavyWork();
    double end = xrtTimer();
    
    printf("Elapsed: %.6f seconds\n", end - start);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Returns monotonically increasing time, suitable for calculating time intervals
- Not affected by system time adjustments

---

### xrtSleep

Millisecond-level delay.

**Prototype:**
```c
XXAPI void xrtSleep(uint32 ms);
```

**Parameters:**
- `ms` - Delay in milliseconds

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("Starting wait...\n");
    xrtSleep(1000);  // Wait 1 second
    printf("After 1 second\n");
    
    xrtSleep(500);   // Wait 500 milliseconds
    printf("Another 0.5 seconds passed\n");
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `Sleep()`
- Linux/macOS uses `usleep()`

---

### xrtNowStr / xrtDateStr / xrtTimeStr

Get string formatted current time.

**Prototypes:**
```c
XXAPI str xrtNowStr();   // Returns "YYYY-MM-DD HH:MM:SS"
XXAPI str xrtDateStr();  // Returns "YYYY-MM-DD"
XXAPI str xrtTimeStr();  // Returns "HH:MM:SS"
```

**Return Value:**
- Formatted time string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str now = xrtNowStr();
    str date = xrtDateStr();
    str time = xrtTimeStr();
    
    printf("Current datetime: %s\n", now);   // "2025-12-03 15:30:45"
    printf("Current date: %s\n", date);      // "2025-12-03"
    printf("Current time: %s\n", time);      // "15:30:45"
    
    xrtFree(now);
    xrtFree(date);
    xrtFree(time);
    
    xrtUnit();
    return 0;
}
```

---

## Time Construction

### xrtDateSerial

Construct date timestamp.

**Prototype:**
```c
XXAPI xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);
```

**Parameters:**
- `iYear` - Year (negative values for BC)
- `iMonth` - Month (1-12)
- `iDay` - Day (1-31)

**Return Value:**
- Timestamp (seconds)
- Returns 0 and sets error message for invalid month

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct date
    xtime date = xrtDateSerial(2025, 1, 10);
    str dateStr = xrtTimeToStr(date, XRT_TIME_FORMAT_DATE);
    printf("Date: %s\n", dateStr);  // "2025-01-10"
    xrtFree(dateStr);
    
    // BC date (negative year)
    xtime bc = xrtDateSerial(-100, 3, 15);
    printf("BC timestamp: %" PRId64 "\n", bc);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeSerial

Construct time timestamp (seconds of day).

**Prototype:**
```c
XXAPI xtime xrtTimeSerial(int iHour, int iMin, int iSec);
```

**Parameters:**
- `iHour` - Hour (0-23)
- `iMin` - Minute (0-59)
- `iSec` - Second (0-59)

**Return Value:**
- Timestamp (seconds of day, 0-86399)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct time
    xtime time = xrtTimeSerial(15, 30, 45);
    printf("Seconds of day: %" PRId64 "\n", time);  // 55845 = 15*3600 + 30*60 + 45
    
    str timeStr = xrtTimeToStr(time, XRT_TIME_FORMAT_TIME);
    printf("Time: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateTimeSerial

Construct complete date+time timestamp.

**Prototype:**
```c
XXAPI xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
```

**Parameters:**
- `iYear` - Year
- `iMonth` - Month (1-12)
- `iDay` - Day (1-31)
- `iHour` - Hour (0-23)
- `iMinute` - Minute (0-59)
- `iSecond` - Second (0-59)

**Return Value:**
- Complete timestamp

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Construct complete datetime
    xtime dt = xrtDateTimeSerial(2025, 12, 25, 10, 30, 0);
    str dtStr = xrtTimeToStr(dt, XRT_TIME_FORMAT_DATETIME);
    printf("Christmas: %s\n", dtStr);  // "2025-12-25 10:30:00"
    xrtFree(dtStr);
    
    // Result equals xrtDateSerial + xrtTimeSerial
    xtime d = xrtDateSerial(2025, 12, 25);
    xtime t = xrtTimeSerial(10, 30, 0);
    printf("Equal: %s\n", (d + t == dt) ? "Yes" : "No");  // "Yes"
    
    xrtUnit();
    return 0;
}
```

---

## Time Calculation

### xrtDateAdd

Add time units.

**Prototype:**
```c
XXAPI xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);
```

**Parameters:**
- `interval` - Interval type (use `XRT_TIME_INTERVAL_*` constants)
  - `XRT_TIME_INTERVAL_YEAR` (1) - Year
  - `XRT_TIME_INTERVAL_MONTH` (2) - Month
  - `XRT_TIME_INTERVAL_DAY` (3) - Day
  - `XRT_TIME_INTERVAL_HOUR` (4) - Hour
  - `XRT_TIME_INTERVAL_MINUTE` (5) - Minute
  - `XRT_TIME_INTERVAL_SECOND` (6) - Second
  - `XRT_TIME_INTERVAL_WEEKDAY` (7) - Week (7 days)
  - `XRT_TIME_INTERVAL_QUARTER` (8) - Quarter (3 months)
- `iValue` - Increment (can be negative)
- `iTime` - Base time

**Return Value:**
- Calculated timestamp

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    str nowStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current: %s\n", nowStr);
    xrtFree(nowStr);
    
    // Add 1 day
    xtime tomorrow = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 1, now);
    str s1 = xrtTimeToStr(tomorrow, XRT_TIME_FORMAT_DATE);
    printf("Tomorrow: %s\n", s1);
    xrtFree(s1);
    
    // Add 1 week
    xtime next_week = xrtDateAdd(XRT_TIME_INTERVAL_WEEKDAY, 1, now);
    str s2 = xrtTimeToStr(next_week, XRT_TIME_FORMAT_DATE);
    printf("Next week: %s\n", s2);
    xrtFree(s2);
    
    // Subtract 1 month
    xtime last_month = xrtDateAdd(XRT_TIME_INTERVAL_MONTH, -1, now);
    str s3 = xrtTimeToStr(last_month, XRT_TIME_FORMAT_DATE);
    printf("Last month: %s\n", s3);
    xrtFree(s3);
    
    // Add 1 quarter
    xtime next_quarter = xrtDateAdd(XRT_TIME_INTERVAL_QUARTER, 1, now);
    str s4 = xrtTimeToStr(next_quarter, XRT_TIME_FORMAT_DATE);
    printf("Next quarter: %s\n", s4);
    xrtFree(s4);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDateDiff

Calculate time difference.

**Prototype:**
```c
XXAPI int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);
```

**Parameters:**
- `interval` - Interval unit (same as `xrtDateAdd`, but **does NOT support** `XRT_TIME_INTERVAL_WEEKDAY`)
- `iTime1` - Time 1
- `iTime2` - Time 2

**Return Value:**
- Time difference (iTime2 - iTime1)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateSerial(2025, 1, 1);
    xtime end = xrtDateSerial(2025, 12, 31);
    
    // Days difference
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, start, end);
    printf("Days difference: %" PRId64 "\n", days);  // 364
    
    // Months difference
    int64 months = xrtDateDiff(XRT_TIME_INTERVAL_MONTH, start, end);
    printf("Months difference: %" PRId64 "\n", months);  // 11
    
    // Quarters difference
    int64 quarters = xrtDateDiff(XRT_TIME_INTERVAL_QUARTER, start, end);
    printf("Quarters difference: %" PRId64 "\n", quarters);  // 3
    
    // Hours difference
    xtime t1 = xrtDateTimeSerial(2025, 1, 1, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 1, 1, 17, 30, 0);
    int64 hours = xrtDateDiff(XRT_TIME_INTERVAL_HOUR, t1, t2);
    printf("Hours difference: %" PRId64 "\n", hours);  // 9
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `XRT_TIME_INTERVAL_WEEKDAY` is not supported, returns 0
- Result is iTime2 - iTime1, can be negative

---

## Time Formatting

### xrtTimeToStr

Convert time to string

**Prototype:**
```c
XXAPI str xrtTimeToStr(xtime iTime, int iFormat);
```

**Parameters:**
- `iTime` - Timestamp
- `iFormat` - Format type
  - `0` - `YYYY-MM-DD HH:MM:SS`
  - `1` - `YYYY-MM-DD`
  - `2` - `HH:MM:SS`
  - `3` - `YYYYMMDDHHMMSS`

**Return Value:**
- Formatted time string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
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

Parse string to time.

**Prototype:**
```c
XXAPI xtime xrtStrToTime(str sTime, size_t iSize);
```

**Parameters:**
- `sTime` - Time string
- `iSize` - String length (0 for auto-calculate)

**Return Value:**
- Parsed timestamp
- Returns 0 on parse failure

**Supported Formats:**
- `YYYY-MM-DD HH:MM:SS`
- `YYYY-MM-DD`
- `YYYYMMDDHHMMSS`
- `YYYYMMDD`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Parse full datetime
    xtime t1 = xrtStrToTime("2025-01-10 15:30:45", 0);
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("Parse1: %s\n", s1);  // "2025-01-10 15:30:45"
    xrtFree(s1);
    
    // Parse date only
    xtime t2 = xrtStrToTime("2025-01-10", 0);
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_DATE);
    printf("Parse2: %s\n", s2);  // "2025-01-10"
    xrtFree(s2);
    
    // Parse compact format
    xtime t3 = xrtStrToTime("20250110153045", 0);
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATETIME);
    printf("Parse3: %s\n", s3);  // "2025-01-10 15:30:45"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

## Custom Formatting

### xrtTimeFormat

Format time to string with custom format.

**Prototype:**
```c
XXAPI str xrtTimeFormat(xtime iTime, str sFormat);
```

**Parameters:**
- `iTime` - Timestamp
- `sFormat` - Format string

**Return Value:**
- Formatted string

**Memory Release:** ✅ Requires `xrtFree` to release

**Format Placeholders:**

| Placeholder | Description | Example |
|-------------|-------------|----------|
| `yyyy` | 4-digit year | 2025 |
| `yy` | 2-digit year | 25 |
| `mm` | 2-digit month (zero-padded) | 01-12 |
| `m` | Month (no padding) | 1-12 |
| `mmm` | English month abbreviation | Jan, Feb, ... |
| `mmmm` | English month full name | January, February, ... |
| `dd` | 2-digit day (zero-padded) | 01-31 |
| `d` | Day (no padding) | 1-31 |
| `hh` | 24-hour format hour (zero-padded) | 00-23 |
| `h` | 24-hour format hour (no padding) | 0-23 |
| `HH` | 12-hour format hour (zero-padded) | 01-12 |
| `H` | 12-hour format hour (no padding) | 1-12 |
| `nn` | Minute (zero-padded) | 00-59 |
| `n` | Minute (no padding) | 0-59 |
| `ss` | Second (zero-padded) | 00-59 |
| `s` | Second (no padding) | 0-59 |
| `ap` | Lowercase AM/PM | am, pm |
| `AP` | Uppercase AM/PM | AM, PM |
| `w` | Weekday number | 0-6 (0=Sunday) |
| `ww` | English weekday abbreviation | Sun, Mon, ... |
| `www` | English weekday full name | Sunday, Monday, ... |
| `q` | Quarter | 1-4 |

**Special Note:**
- `mm` after `h` or `hh` means **minute**, otherwise means **month** (VB compatible)
- Use `nn` or `n` to explicitly specify minute

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateTimeSerial(2025, 3, 15, 14, 30, 45);
    
    // Basic format
    str s1 = xrtTimeFormat(t, "yyyy-mm-dd hh:nn:ss");
    printf("%s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // 12-hour format
    str s2 = xrtTimeFormat(t, "yyyy/mm/dd HH:nn:ss AP");
    printf("%s\n", s2);  // "2025/03/15 02:30:45 PM"
    xrtFree(s2);
    
    // English month
    str s3 = xrtTimeFormat(t, "mmm d, yyyy");
    printf("%s\n", s3);  // "Mar 15, 2025"
    xrtFree(s3);
    
    // Full English format
    str s4 = xrtTimeFormat(t, "mmmm d, yyyy");
    printf("%s\n", s4);  // "March 15, 2025"
    xrtFree(s4);
    
    // With weekday
    str s5 = xrtTimeFormat(t, "www, mmm d");
    printf("%s\n", s5);  // "Saturday, Mar 15"
    xrtFree(s5);
    
    // Compact format
    str s6 = xrtTimeFormat(t, "yyyymmddhhnnss");
    printf("%s\n", s6);  // "20250315143045"
    xrtFree(s6);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeParse

Parse string to time with custom format.

**Prototype:**
```c
XXAPI xtime xrtTimeParse(str sTime, str sFormat);
```

**Parameters:**
- `sTime` - String to parse
- `sFormat` - Format string

**Return Value:**
- Parsed timestamp
- Returns 0 on parse failure

**Format Placeholders:**

In addition to all `xrtTimeFormat` placeholders, the following special matchers are supported:

| Matcher | Description |
|---------|-------------|
| `*` | Skip any non-digit characters |
| `.` | Match at least 1 non-digit character |
| `?` | Skip 1 character |
| Space | Skip any whitespace characters |

**High Tolerance Features:**
- Automatically skips prefix redundant text
- Supports anchor positioning (locate time via fixed text)
- Intelligently recognizes common time patterns

**Basic Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Parse standard format
    xtime t1 = xrtTimeParse("2025-03-15 14:30:45", "yyyy-mm-dd hh:nn:ss");
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("%s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // Parse 12-hour format
    xtime t2 = xrtTimeParse("03/15/2025 02:30:45 PM", "mm/dd/yyyy HH:nn:ss AP");
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_DATETIME);
    printf("%s\n", s2);  // "2025-03-15 14:30:45"
    xrtFree(s2);
    
    // Parse English month
    xtime t3 = xrtTimeParse("Mar 15, 2025", "mmm d, yyyy");
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATE);
    printf("%s\n", s3);  // "2025-03-15"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

**Redundant Text Handling Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Auto-skip prefix redundant text
    xtime t1 = xrtTimeParse("Posted: 2025-03-15 14:30:45", "yyyy-mm-dd hh:nn:ss");
    str s1 = xrtTimeToStr(t1, XRT_TIME_FORMAT_DATETIME);
    printf("Parse1: %s\n", s1);  // "2025-03-15 14:30:45"
    xrtFree(s1);
    
    // Use anchor positioning
    xtime t2 = xrtTimeParse("Server Time UTC+8  14:30:45", "UTC+8  hh:nn:ss");
    str s2 = xrtTimeToStr(t2, XRT_TIME_FORMAT_TIME);
    printf("Parse2: %s\n", s2);  // "14:30:45"
    xrtFree(s2);
    
    // Parse log format
    xtime t3 = xrtTimeParse("[2025-03-15 14:30:45] INFO: System started", 
                            "[yyyy-mm-dd hh:nn:ss]");
    str s3 = xrtTimeToStr(t3, XRT_TIME_FORMAT_DATETIME);
    printf("Parse3: %s\n", s3);  // "2025-03-15 14:30:45"
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

## Time Extraction

### xrtYear

Get year from time.

**Prototype:**
```c
XXAPI int64 xrtYear(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Year (negative for BC)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int64 year = xrtYear(now);
    printf("Current year: %" PRId64 "\n", year);  // 2025
    
    xrtUnit();
    return 0;
}
```

---

### xrtMonth

Get month from time.

**Prototype:**
```c
XXAPI int xrtMonth(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Month (1-12)

---

### xrtDay

Get day from time.

**Prototype:**
```c
XXAPI int xrtDay(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Day (1-31)

---

### xrtHour

Get hour from time.

**Prototype:**
```c
XXAPI int xrtHour(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Hour (0-23)

---

### xrtMinute

Get minute from time.

**Prototype:**
```c
XXAPI int xrtMinute(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Minute (0-59)

---

### xrtSecond

Get second from time.

**Prototype:**
```c
XXAPI int xrtSecond(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Second (0-59)

---

### xrtWeekday

Get weekday from time.

**Prototype:**
```c
XXAPI int xrtWeekday(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Weekday (0=Sunday, 1=Monday, ..., 6=Saturday)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int weekday = xrtWeekday(now);
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    printf("Today is %s\n", days[weekday]);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDayOfYear

Get day of year from time.

**Prototype:**
```c
XXAPI int xrtDayOfYear(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Day of year (1-366)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int day_of_year = xrtDayOfYear(now);
    printf("Today is day %d of the year\n", day_of_year);
    
    // January 1st is day 1
    xtime jan1 = xrtDateSerial(2025, 1, 1);
    printf("January 1st: Day %d\n", xrtDayOfYear(jan1));  // 1
    
    xrtUnit();
    return 0;
}
```

---

### xrtQuarter

Get quarter from time.

**Prototype:**
```c
XXAPI int xrtQuarter(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Quarter (1-4)
  - January-March → Q1
  - April-June → Q2
  - July-September → Q3
  - October-December → Q4

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    int quarter = xrtQuarter(now);
    printf("Current quarter: Q%d\n", quarter);
    
    // Quarter examples
    printf("January: Q%d\n", xrtQuarter(xrtDateSerial(2025, 1, 15)));   // Q1
    printf("May: Q%d\n", xrtQuarter(xrtDateSerial(2025, 5, 15)));       // Q2
    printf("August: Q%d\n", xrtQuarter(xrtDateSerial(2025, 8, 15)));    // Q3
    printf("November: Q%d\n", xrtQuarter(xrtDateSerial(2025, 11, 15))); // Q4
    
    xrtUnit();
    return 0;
}
```

---

### xrtDatePart

Get date part (remove time).

**Prototype:**
```c
XXAPI xtime xrtDatePart(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Timestamp containing only date (time is 00:00:00)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();  // e.g., 2025-01-10 15:30:45
    xtime datePart = xrtDatePart(now);  // 2025-01-10 00:00:00
    
    str s1 = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(datePart, XRT_TIME_FORMAT_DATETIME);
    
    printf("Original: %s\n", s1);
    printf("Date part: %s\n", s2);
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Equivalent to `iTime - (iTime % XRT_TIME_DAY)`
- Useful for date comparison (ignoring time)

---

### xrtTimePart

Get time part (remove date).

**Prototype:**
```c
XXAPI xtime xrtTimePart(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Seconds elapsed today (0-86399)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();  // e.g., 2025-01-10 15:30:45
    xtime timePart = xrtTimePart(now);  // 55845 seconds (15*3600 + 30*60 + 45)
    
    printf("Seconds elapsed today: %" PRId64 "\n", timePart);
    
    str timeStr = xrtTimeToStr(timePart, XRT_TIME_FORMAT_TIME);
    printf("Time part: %s\n", timeStr);  // "15:30:45"
    xrtFree(timeStr);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Equivalent to `iTime % XRT_TIME_DAY`
- Useful for time comparison (ignoring date)

**Complete Time Extraction Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    printf("Current time: %" PRId64 "-", xrtYear(now));
    printf("%02d-%02d ", xrtMonth(now), xrtDay(now));
    printf("%02d:%02d:%02d\n", xrtHour(now), xrtMinute(now), xrtSecond(now));
    printf("Weekday: %d\n", xrtWeekday(now));
    printf("Day of year: %d\n", xrtDayOfYear(now));
    printf("Quarter: Q%d\n", xrtQuarter(now));
    
    xrtUnit();
    return 0;
}
```

---

## Time Decoding

### xrtDecodeSerial

Decode all time components at once.

**Prototype:**
```c
XXAPI void xrtDecodeSerial(xtime iTime, int64* pYear, int* pMonth, int* pDay, 
                           int* pHour, int* pMinute, int* pSecond, 
                           int* pWeekday, int* pDayOfYear);
```

**Parameters:**
- `iTime` - Timestamp
- `pYear` - Year output pointer (can be NULL)
- `pMonth` - Month output pointer (can be NULL)
- `pDay` - Day output pointer (can be NULL)
- `pHour` - Hour output pointer (can be NULL)
- `pMinute` - Minute output pointer (can be NULL)
- `pSecond` - Second output pointer (can be NULL)
- `pWeekday` - Weekday output pointer (can be NULL)
- `pDayOfYear` - Day of year output pointer (can be NULL)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // Decode all fields
    int64 year;
    int month, day, hour, minute, second, weekday, dayOfYear;
    xrtDecodeSerial(now, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);
    
    printf("Full decode:\n");
    printf("Date: %" PRId64 "-%02d-%02d\n", year, month, day);
    printf("Time: %02d:%02d:%02d\n", hour, minute, second);
    printf("Weekday: %d\n", weekday);
    printf("Day of year: %d\n", dayOfYear);
    
    // Decode only some fields
    int64 y;
    int m, d;
    xrtDecodeSerial(now, &y, &m, &d, NULL, NULL, NULL, NULL, NULL);
    printf("Date only: %" PRId64 "-%02d-%02d\n", y, m, d);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- More efficient than multiple calls to individual extraction functions
- Fields not needed can pass `NULL`

---

## Time Comparison

### xrtIsSameDay

Check if two times are on the same day.

**Prototype:**
```c
XXAPI bool xrtIsSameDay(xtime iTime1, xtime iTime2);
```

**Parameters:**
- `iTime1` - Timestamp 1
- `iTime2` - Timestamp 2

**Return Value:**
- `TRUE` - Same day
- `FALSE` - Different days

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t1 = xrtDateTimeSerial(2025, 3, 15, 8, 0, 0);
    xtime t2 = xrtDateTimeSerial(2025, 3, 15, 20, 0, 0);
    xtime t3 = xrtDateTimeSerial(2025, 3, 16, 8, 0, 0);
    
    printf("t1 and t2 same day: %s\n", xrtIsSameDay(t1, t2) ? "Yes" : "No");  // Yes
    printf("t1 and t3 same day: %s\n", xrtIsSameDay(t1, t3) ? "Yes" : "No");  // No
    
    xrtUnit();
    return 0;
}
```

---

### xrtIsSameMonth

Check if two times are in the same month.

**Prototype:**
```c
XXAPI bool xrtIsSameMonth(xtime iTime1, xtime iTime2);
```

**Parameters:**
- `iTime1` - Timestamp 1
- `iTime2` - Timestamp 2

**Return Value:**
- `TRUE` - Same month (same year and month)
- `FALSE` - Different months

---

### xrtIsSameYear

Check if two times are in the same year.

**Prototype:**
```c
XXAPI bool xrtIsSameYear(xtime iTime1, xtime iTime2);
```

**Parameters:**
- `iTime1` - Timestamp 1
- `iTime2` - Timestamp 2

**Return Value:**
- `TRUE` - Same year
- `FALSE` - Different years

---

### xrtTimeInRange

Check if time is within a specified range.

**Prototype:**
```c
XXAPI bool xrtTimeInRange(xtime iTime, xtime iStart, xtime iEnd);
```

**Parameters:**
- `iTime` - Time to check
- `iStart` - Range start time
- `iEnd` - Range end time

**Return Value:**
- `TRUE` - Within range (inclusive)
- `FALSE` - Outside range

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime start = xrtDateTimeSerial(2025, 3, 1, 9, 0, 0);
    xtime end = xrtDateTimeSerial(2025, 3, 1, 18, 0, 0);
    
    xtime t1 = xrtDateTimeSerial(2025, 3, 1, 12, 0, 0);  // Noon
    xtime t2 = xrtDateTimeSerial(2025, 3, 1, 20, 0, 0);  // Evening
    
    printf("12:00 in work hours: %s\n", xrtTimeInRange(t1, start, end) ? "Yes" : "No");  // Yes
    printf("20:00 in work hours: %s\n", xrtTimeInRange(t2, start, end) ? "Yes" : "No");  // No
    
    xrtUnit();
    return 0;
}
```

---

### xrtTimeRangeOverlap

Check if two time ranges overlap.

**Prototype:**
```c
XXAPI bool xrtTimeRangeOverlap(xtime iStart1, xtime iEnd1, xtime iStart2, xtime iEnd2);
```

**Parameters:**
- `iStart1`, `iEnd1` - First time range
- `iStart2`, `iEnd2` - Second time range

**Return Value:**
- `TRUE` - Ranges overlap
- `FALSE` - Ranges don't overlap

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Meeting 1: 9:00 - 11:00
    xtime m1_start = xrtDateTimeSerial(2025, 3, 1, 9, 0, 0);
    xtime m1_end = xrtDateTimeSerial(2025, 3, 1, 11, 0, 0);
    
    // Meeting 2: 10:00 - 12:00 (overlaps with meeting 1)
    xtime m2_start = xrtDateTimeSerial(2025, 3, 1, 10, 0, 0);
    xtime m2_end = xrtDateTimeSerial(2025, 3, 1, 12, 0, 0);
    
    // Meeting 3: 14:00 - 16:00 (no overlap)
    xtime m3_start = xrtDateTimeSerial(2025, 3, 1, 14, 0, 0);
    xtime m3_end = xrtDateTimeSerial(2025, 3, 1, 16, 0, 0);
    
    bool overlap12 = xrtTimeRangeOverlap(m1_start, m1_end, m2_start, m2_end);
    bool overlap13 = xrtTimeRangeOverlap(m1_start, m1_end, m3_start, m3_end);
    
    printf("Meeting 1 & 2 conflict: %s\n", overlap12 ? "Yes" : "No");  // Yes
    printf("Meeting 1 & 3 conflict: %s\n", overlap13 ? "Yes" : "No");  // No
    
    xrtUnit();
    return 0;
}
```

---

## Boundary Dates

### xrtFirstDayOfMonth

Get first day of month.

**Prototype:**
```c
XXAPI xtime xrtFirstDayOfMonth(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Timestamp of first day of month (00:00:00)

---

### xrtLastDayOfMonth

Get last day of month.

**Prototype:**
```c
XXAPI xtime xrtLastDayOfMonth(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Timestamp of last day of month (00:00:00)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Common year February
    xtime t1 = xrtDateSerial(2025, 2, 15);
    xtime last1 = xrtLastDayOfMonth(t1);
    str s1 = xrtTimeToStr(last1, XRT_TIME_FORMAT_DATE);
    printf("Feb 2025 end: %s\n", s1);  // "2025-02-28"
    xrtFree(s1);
    
    // Leap year February
    xtime t2 = xrtDateSerial(2024, 2, 15);
    xtime last2 = xrtLastDayOfMonth(t2);
    str s2 = xrtTimeToStr(last2, XRT_TIME_FORMAT_DATE);
    printf("Feb 2024 end: %s\n", s2);  // "2024-02-29"
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFirstDayOfYear / xrtLastDayOfYear

Get first/last day of year.

**Prototypes:**
```c
XXAPI xtime xrtFirstDayOfYear(xtime iTime);
XXAPI xtime xrtLastDayOfYear(xtime iTime);
```

---

### xrtFirstDayOfWeek / xrtLastDayOfWeek

Get first/last day of week.

**Prototypes:**
```c
XXAPI xtime xrtFirstDayOfWeek(xtime iTime, int iStartDay);
XXAPI xtime xrtLastDayOfWeek(xtime iTime, int iStartDay);
```

**Parameters:**
- `iTime` - Timestamp
- `iStartDay` - Week start day (0=Sunday, 1=Monday, ...)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime t = xrtDateSerial(2025, 3, 12);  // Wednesday
    
    // Week starts on Sunday
    xtime first_sun = xrtFirstDayOfWeek(t, 0);
    str s1 = xrtTimeToStr(first_sun, XRT_TIME_FORMAT_DATE);
    printf("Week start (Sunday): %s\n", s1);
    xrtFree(s1);
    
    // Week starts on Monday (ISO standard)
    xtime first_mon = xrtFirstDayOfWeek(t, 1);
    str s2 = xrtTimeToStr(first_mon, XRT_TIME_FORMAT_DATE);
    printf("Week start (Monday): %s\n", s2);
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

## Week Functions

### xrtWeekOfYear

Get week number of year (ISO week, Monday is first day of week).

**Prototype:**
```c
XXAPI int xrtWeekOfYear(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Week number (1-53)

---

### xrtWeekOfMonth

Get week number of month (Monday is first day of week).

**Prototype:**
```c
XXAPI int xrtWeekOfMonth(xtime iTime);
```

**Parameters:**
- `iTime` - Timestamp

**Return Value:**
- Week number (1-6)

---

## Timezone Handling

### xrtNowUTC

Get current UTC time.

**Prototype:**
```c
XXAPI xtime xrtNowUTC();
```

**Return Value:**
- Current UTC timestamp

---

### xrtTimezoneOffset

Get local timezone offset (seconds).

**Prototype:**
```c
XXAPI int xrtTimezoneOffset();
```

**Return Value:**
- Timezone offset in seconds (positive for east, negative for west)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int offset = xrtTimezoneOffset();
    int hours = offset / 3600;
    int minutes = (offset % 3600) / 60;
    
    printf("Timezone offset: UTC%+d:%02d\n", hours, minutes);
    // China: "UTC+8:00"
    
    xrtUnit();
    return 0;
}
```

---

### xrtUTCToLocal / xrtLocalToUTC

Convert between UTC and local time.

**Prototypes:**
```c
XXAPI xtime xrtUTCToLocal(xtime utc);
XXAPI xtime xrtLocalToUTC(xtime local);
```

**Example:**
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
    
    printf("UTC:   %s\n", s1);    // "2025-03-15 06:00:00"
    printf("Local: %s\n", s2);    // "2025-03-15 14:00:00" (China UTC+8)
    
    xrtFree(s1);
    xrtFree(s2);
    xrtUnit();
    return 0;
}
```

---

## Unix Timestamp

### xrtToUnixTime

Convert xtime to Unix timestamp.

**Prototype:**
```c
XXAPI int64 xrtToUnixTime(xtime iTime);
```

**Parameters:**
- `iTime` - xtime timestamp

**Return Value:**
- Unix timestamp (seconds since January 1, 1970)

---

### xrtFromUnixTime

Convert Unix timestamp to xtime.

**Prototype:**
```c
XXAPI xtime xrtFromUnixTime(int64 unixTime);
```

**Parameters:**
- `unixTime` - Unix timestamp

**Return Value:**
- xtime timestamp

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Convert xtime to Unix timestamp
    xtime t = xrtDateTimeSerial(2025, 3, 15, 12, 0, 0);
    int64 unix_ts = xrtToUnixTime(t);
    printf("Unix timestamp: %" PRId64 "\n", unix_ts);
    
    // Verify: 1970-01-01 00:00:00 should be 0
    xtime epoch = xrtDateTimeSerial(1970, 1, 1, 0, 0, 0);
    printf("Epoch: %" PRId64 "\n", xrtToUnixTime(epoch));  // 0
    
    // Bi-directional conversion
    xtime back = xrtFromUnixTime(unix_ts);
    printf("Round-trip: %s\n", (t == back) ? "Pass" : "Fail");
    
    xrtUnit();
    return 0;
}
```

---

## Relative Time

### xrtRelativeTime

Get relative time description (e.g., "3 days ago", "2 hours later").

**Prototype:**
```c
XXAPI str xrtRelativeTime(xtime iTime, xtime iBaseTime);
```

**Parameters:**
- `iTime` - Target time
- `iBaseTime` - Base time (usually pass `xrtNow()`)

**Return Value:**
- Relative time description string

**Memory Release:** ✅ Requires `xrtFree` to release

**Output Format (Chinese):**
- `刚刚` - Within 60 seconds
- `N分钟前/后` - Within 60 minutes
- `N小时前/后` - Within 24 hours
- `N天前/后` - Within 30 days
- `N个月前/后` - Within 12 months
- `N年前/后` - Over 12 months

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xtime now = xrtNow();
    
    // 30 minutes ago
    xtime t1 = xrtDateAdd(XRT_TIME_INTERVAL_MINUTE, -30, now);
    str s1 = xrtRelativeTime(t1, now);
    printf("%s\n", s1);  // "30分钟前"
    xrtFree(s1);
    
    // 3 days later
    xtime t2 = xrtDateAdd(XRT_TIME_INTERVAL_DAY, 3, now);
    str s2 = xrtRelativeTime(t2, now);
    printf("%s\n", s2);  // "3天后"
    xrtFree(s2);
    
    xrtUnit();
    return 0;
}
```

---

## Date Utilities

### xrtIsLeapYear

Check if year is a leap year.

**Prototype:**
```c
XXAPI bool xrtIsLeapYear(int iYear);
```

**Parameters:**
- `iYear` - Year

**Return Value:**
- `TRUE` - Leap year
- `FALSE` - Common year

**Rules:**
- Divisible by 400 → Leap year
- Divisible by 100 → Common year
- Divisible by 4 → Leap year
- Others → Common year

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %s\n", xrtIsLeapYear(2024) ? "Leap year" : "Common year");  // Leap year
    printf("2025: %s\n", xrtIsLeapYear(2025) ? "Leap year" : "Common year");  // Common year
    printf("2000: %s\n", xrtIsLeapYear(2000) ? "Leap year" : "Common year");  // Leap year
    printf("1900: %s\n", xrtIsLeapYear(1900) ? "Leap year" : "Common year");  // Common year
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInMonth

Get number of days in a month.

**Prototype:**
```c
XXAPI int xrtDaysInMonth(int iYear, int iMonth);
```

**Parameters:**
- `iYear` - Year
- `iMonth` - Month (1-12)

**Return Value:**
- Number of days (28-31)
- Returns 0 for invalid month

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // February days depend on leap year
    printf("Feb 2024: %d days\n", xrtDaysInMonth(2024, 2));  // 29
    printf("Feb 2025: %d days\n", xrtDaysInMonth(2025, 2));  // 28
    
    // Other months
    printf("January: %d days\n", xrtDaysInMonth(2025, 1));   // 31
    printf("April: %d days\n", xrtDaysInMonth(2025, 4));     // 30
    printf("December: %d days\n", xrtDaysInMonth(2025, 12)); // 31
    
    // Invalid month
    printf("Invalid month: %d\n", xrtDaysInMonth(2025, 13));  // 0
    
    xrtUnit();
    return 0;
}
```

---

### xrtDaysInYear

Get number of days in a year.

**Prototype:**
```c
XXAPI int xrtDaysInYear(int iYear);
```

**Parameters:**
- `iYear` - Year

**Return Value:**
- Number of days (365 or 366)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    printf("2024: %d days\n", xrtDaysInYear(2024));  // 366
    printf("2025: %d days\n", xrtDaysInYear(2025));  // 365
    
    xrtUnit();
    return 0;
}
```

---

## Use Cases

### 1. Performance Timer

```c
#include "xrt.h"
#include <stdio.h>

void ProcessData() {
    // Simulate time-consuming operation
    for (volatile int i = 0; i < 10000000; i++);
}

int main() {
    xrtInit();
    
    double start = xrtTimer();
    ProcessData();
    double elapsed = xrtTimer() - start;
    
    printf("Processing time: %.6f seconds\n", elapsed);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Log Timestamp

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
    
    Log((str)"Application started");
    xrtSleep(1000);
    Log((str)"Processing...");
    xrtSleep(500);
    Log((str)"Application ended");
    
    xrtUnit();
    return 0;
}
```

---

### 3. Expiration Check

```c
#include "xrt.h"
#include <stdio.h>

bool IsExpired(xtime expire_time) {
    return xrtNow() > expire_time;
}

int main() {
    xrtInit();
    
    // Set expiration date to December 31, 2025
    xtime expire = xrtDateSerial(2025, 12, 31);
    
    if (IsExpired(expire)) {
        printf("License has expired\n");
    } else {
        int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, xrtNow(), expire);
        printf("License remaining: %" PRId64 " days\n", days);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. Birthday Calculation

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Assume birthday is May 15, 1990
    xtime birthday = xrtDateSerial(1990, 5, 15);
    xtime today = xrtDate();
    
    // Calculate age
    int64 years = xrtDateDiff(XRT_TIME_INTERVAL_YEAR, birthday, today);
    printf("Age: %" PRId64 " years\n", years);
    
    // Calculate total days
    int64 days = xrtDateDiff(XRT_TIME_INTERVAL_DAY, birthday, today);
    printf("Days lived: %" PRId64 " days\n", days);
    
    // Next birthday
    int64 this_year = xrtYear(today);
    xtime next_bday = xrtDateSerial(this_year, 5, 15);
    if (next_bday <= today) {
        next_bday = xrtDateSerial(this_year + 1, 5, 15);
    }
    int64 days_until = xrtDateDiff(XRT_TIME_INTERVAL_DAY, today, next_bday);
    printf("Days until next birthday: %" PRId64 " days\n", days_until);
    
    xrtUnit();
    return 0;
}
```

---

### 5. Workday Calculation

```c
#include "xrt.h"
#include <stdio.h>

// Count workdays (excluding weekends)
int CountWorkdays(xtime start, xtime end) {
    int workdays = 0;
    xtime current = start;
    
    while (current <= end) {
        int weekday = xrtWeekday(current);
        if (weekday != 0 && weekday != 6) {  // Not weekend
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
    printf("January 2025 workdays: %d days\n", workdays);
    
    xrtUnit();
    return 0;
}
```

---

### 6. Custom Log Format

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
    
    LogMessage("INFO", "Application started");
    xrtSleep(500);
    LogMessage("DEBUG", "Loading configuration");
    xrtSleep(300);
    LogMessage("WARN", "Config missing, using defaults");
    LogMessage("INFO", "Application ready");
    
    xrtUnit();
    return 0;
}
```

---

### 7. Timezone Conversion

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Get current times
    xtime local_now = xrtNow();
    xtime utc_now = xrtNowUTC();
    
    str s1 = xrtTimeToStr(local_now, XRT_TIME_FORMAT_DATETIME);
    str s2 = xrtTimeToStr(utc_now, XRT_TIME_FORMAT_DATETIME);
    
    printf("Local time: %s\n", s1);
    printf("UTC time:   %s\n", s2);
    printf("Timezone: UTC%+d\n", xrtTimezoneOffset() / 3600);
    
    xrtFree(s1);
    xrtFree(s2);
    
    // Time conversion: meeting scheduled in UTC
    xtime meeting_utc = xrtDateTimeSerial(2025, 3, 15, 14, 0, 0);  // UTC 14:00
    xtime meeting_local = xrtUTCToLocal(meeting_utc);
    
    str s3 = xrtTimeFormat(meeting_local, "mmm d, yyyy HH:nn AP");
    printf("Meeting local time: %s\n", s3);
    xrtFree(s3);
    
    xrtUnit();
    return 0;
}
```

---

### 8. Parse Web-Scraped Time

```c
#include "xrt.h"
#include <stdio.h>

// Try multiple formats to parse time
xtime ParseFlexibleTime(str timeText) {
    static const char* formats[] = {
        "yyyy-mm-dd hh:nn:ss",
        "yyyy/mm/dd hh:nn:ss",
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
    
    return 0;  // Parse failed
}

int main() {
    xrtInit();
    
    const char* samples[] = {
        "Posted: 2025-03-15 14:30:00",
        "Published: March 15, 2025",
        "Date: 2025-03-15 14:30:00",
        "Updated: Mar 15, 2025"
    };
    
    int numSamples = sizeof(samples) / sizeof(samples[0]);
    for (int i = 0; i < numSamples; i++) {
        xtime t = ParseFlexibleTime((str)samples[i]);
        if (t > 0) {
            str s = xrtTimeToStr(t, XRT_TIME_FORMAT_DATETIME);
            printf("\"%s\" -> %s\n", samples[i], s);
            xrtFree(s);
        } else {
            printf("\"%s\" -> Parse failed\n", samples[i]);
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## Related Documents

- [String Processing](api-string.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#time-processing-library)

</div>
