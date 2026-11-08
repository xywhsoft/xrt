# Selected live service

Run from the repository root:

```sh
python tools/build.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --suite xcron_service_selected --no-single
```

On Linux use `--arch x64`. This deterministic example supplies a fixed clock at
2024-11-03 06:30 UTC and an explicit New York POSIX rule. `FOLD_ALL` dispatches
both actual instances of nominal 01:30. Each worker reads the immutable nominal
label and complete candidate count from its execution token. The owner groups
already-ready work up to two leases without waiting for a full group.

The host owns the worker pool and business Data. Shutdown joins the owner,
consumes every accepted command and owned completion error, and reaps the full
retirement before releasing Data or the pool. A successful Add reply means job
admission; later calculation faults are reported by full job Info/retirement.
This example is repeatable integration coverage; it does not measure real clock
lateness, native timer accuracy or production performance.
