# Owned TZif data

From the repository root, with Python 3 and GCC available on Windows or Linux:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_timezone --no-single --cflag=-O2
```

The default example uses a small self-contained UTC TZif v1 fixture. It asserts
the 1970 civil date, zero offset and one exact resolution candidate, then prints
`[PASS] TZif owned bytes: UTC offset=0 candidates=1`. No network, installed
database or service is needed. The file input buffer can be freed immediately
after creation; all returned views require the live retained zone.

The built program optionally accepts a local TZif filename. That application IO
is deliberately outside the extension; supply data whose origin/version/hash
you trust. The 1970 query may report unknown/partial history for truncated data.
Leap-aware files currently report the explicit unsupported-scale issue.

All file handles/input buffers and zone owners are released on the demonstrated
paths. The example creates no files or services, so no external cleanup is needed.
Static/shared packages expose the same public calls; single-header builds select
`XCRON_MODULE_XCRON_TIMEZONE` before including the generated headers.
