"""Independent big-integer oracle for an XRT TIME shared library.

Usage: python tests/time/verify_calendar.py --library out/time_oracle.so
Build the library with the same TIME sources as the module (PIC on POSIX).
"""
import argparse
import ctypes as c
import random

DAY = 86400000
EPOCH = 62135596800000
MIN, MAX = -(1 << 63), (1 << 63) - 1

class Date(c.Structure):
    _fields_ = [('Year', c.c_int64)] + [(name, c.c_int) for name in
        ('Month', 'Day', 'Hour', 'Minute', 'Second', 'Millisecond',
         'Offset', 'Weekday', 'YearDay', 'IsDST')]

def before_year(year):
    previous = year - 1
    return 365 * previous + previous // 4 - previous // 100 + previous // 400

def leap(year):
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)

def month_days(year):
    return [31, 29 if leap(year) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]

def civil_from_day(days):
    low, high = -300000000, 300000000
    while low + 1 < high:
        middle = (low + high) // 2
        if before_year(middle) <= days:
            low = middle
        else:
            high = middle
    within = days - before_year(low)
    year_day = within + 1
    month = 1
    for size in month_days(low):
        if within < size:
            break
        within -= size
        month += 1
    return low if low > 0 else low - 1, month, within + 1, year_day

def trunc_div(number, scale):
    return (1 if number >= 0 else -1) * (abs(number) // scale)

def run(library, samples):
    lib = c.CDLL(library)
    split = lib.xrtTimeSplitAt
    split.argtypes = [c.c_int64, c.c_int, c.POINTER(Date)]
    split.restype = c.c_bool
    make = lib.xrtTimeMake
    make.argtypes = [c.POINTER(Date), c.POINTER(c.c_int64)]
    make.restype = c.c_bool
    diff = lib.xrtDateDiff
    diff.argtypes = [c.c_int64, c.c_int64, c.c_int, c.POINTER(c.c_int64)]
    diff.restype = c.c_bool
    add = lib.xrtTimeAdd
    add.argtypes = [c.c_int64, c.c_int64, c.c_int, c.POINTER(c.c_int64)]
    add.restype = c.c_bool
    unix = lib.xrtTimeUnix
    unix.argtypes = [c.c_int64]
    unix.restype = c.c_int64
    rng = random.Random(20261006)
    boundaries = [MIN, MIN + 1, -DAY, -1001, -1000, -999, -1, 0, 1, 999,
                  1000, DAY - 1, EPOCH - 1, EPOCH, MAX - 1, MAX]
    scales = [1, 1000, 60000, 3600000, DAY, 7 * DAY]
    for i in range(samples):
        value = boundaries[i] if i < len(boundaries) else rng.randint(MIN, MAX)
        offset = rng.randint(-86399, 86399)
        date = Date()
        assert split(value, offset, c.byref(date))
        days, daytime = divmod(value + offset * 1000, DAY)
        expected = civil_from_day(days)
        assert (date.Year, date.Month, date.Day, date.YearDay) == expected
        assert date.Weekday == (days + 1) % 7
        hour, remain = divmod(daytime, 3600000)
        minute, remain = divmod(remain, 60000)
        second, milli = divmod(remain, 1000)
        assert (date.Hour, date.Minute, date.Second, date.Millisecond) == (hour, minute, second, milli)
        result = c.c_int64(123)
        assert make(c.byref(date), c.byref(result)) and result.value == value
        assert unix(value) == (value - EPOCH) // 1000
        other = rng.randint(MIN, MAX)
        unit = i % len(scales)
        expected_diff = trunc_div(other - value, scales[unit])
        result.value = 123
        ok = diff(value, other, unit, c.byref(result))
        assert ok == (MIN <= expected_diff <= MAX)
        assert result.value == (expected_diff if ok else 123)
        amount = rng.randint(MIN, MAX)
        expected_add = value + amount * scales[unit]
        result.value = 123
        ok = add(value, amount, unit, c.byref(result))
        assert ok == (MIN <= expected_add <= MAX)
        assert result.value == (expected_add if ok else 123)
    print('[PASS] independent big-integer calendar/difference/addition oracle:', samples, 'samples')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', required=True)
    parser.add_argument('--samples', type=int, default=100000)
    args = parser.parse_args()
    run(args.library, args.samples)
