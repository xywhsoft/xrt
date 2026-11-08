# Ordered zoned occurrences

Prerequisites: Python 3 and GCC on Windows/Linux. From the repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_zoned --no-single --cflag=-O2
```

The offline example uses explicit modern New York rules. Starting at 05:45 UTC
on 2024-11-03 (01:45 before rollback), it finds the later 01:30 at 06:30 UTC.
It also asserts that 2024-03-10 has no actual 02:30 occurrence. NONE preserves
the previous output. The expected `[PASS]` line states both assertions and owner
release. No host timezone/database, clock, network or service is needed.

The same later-fold query also runs through the real Init/Step cursor with all
three budgets set to1. It prints `bounded later-fold query` with host-turn and
cumulative segment/month/rule-year counts before the existing PASS line. Each
PENDING turn permits other host work. The cursor copies the expression and
borrows the same zone held by this example; storage remains at one address and
the owner is released after the final query. It needs no cursor Unit.

All actual repeated instants are eligible, and missing labels have no occurrence.
Nominal gap compensation, fold selection and delivery policy are separate
scheduler work. An owned TZif zone can replace the explicit rules without
changing these APIs; missing coverage is an error if it prevents proving an
earliest result. The example releases its zone on both success and failure.
It creates no files or services, so there is no external cleanup.
