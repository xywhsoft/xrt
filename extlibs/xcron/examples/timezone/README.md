# Explicit timezone rules

Prerequisites: Python 3 and GCC from the repository's supported Windows/Linux
toolchain. Run from the repository root; no broker, installed timezone database,
network, environment TZ setting or external input is required.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_timezone --no-single --cflag=-O2
```

`main.c` compiles explicit New York modern recurrence rules. It asserts that
2024-11-03 01:30 has exactly the two sorted instants 05:30/06:30 UTC, and that
2024-03-10 02:30 has no matching instant. The example releases the owned zone and
prints `[PASS] explicit zone: fold has two instants, gap has none, owner released`.
No files or services are created, so no cleanup beyond the demonstrated release
is needed. These explicit rules are not a substitute for historical IANA data.

The same public calls work from the generated single header after selecting
`XCRON_MODULE_XCRON_TIMEZONE`, or from an independently linked static/shared
package. See the root guide for build/package commands.
