---
num: 42
slug: time
title: Time and Time Zones
volume: 卷五 系统服务
type: practice
lead: Civil UTC milliseconds for events and double monotonic seconds for elapsed time.
api: time, math
---

## Time representations

`xtime` is signed int64 UTC milliseconds since 0001-01-01 CE. Civil years are ..., -2, -1, 1, 2, ...; there is no year zero. `xdatetime.Millisecond` is 0 through 999.

`xrtTimer()` reads the high-resolution monotonic timer in double seconds. Its origin has no calendar meaning; subtract two readings to measure elapsed time. Waits and sleeps accept relative milliseconds.

```c
double Start = xrtTimer();
xrtSleep(10);
double ElapsedSeconds = xrtTimer() - Start;
```

`xrtDateDiff` returns an integer difference in the requested dimension. Fixed units truncate elapsed milliseconds toward zero; years and months use continuous calendar indices. Calendar addition clamps month ends and skips the nonexistent year zero.

Signed Unix 32-bit seconds, 64-bit seconds and 64-bit milliseconds have checked bidirectional conversions. Negative second conversions floor; failed conversions leave outputs unchanged. Fixed-offset splitting covers the full int64 domain, while native local zones and text protocols have narrower ranges. `%f` is three millisecond digits.

See the [Time API](../../api/time.md) and [Wait API](../../api/wait.md) for complete contracts.
