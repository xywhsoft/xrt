# Live service example

Requires GCC, Python and the repository's public XRT implementation. No broker,
network service or host time-zone configuration is needed. From repository root:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_service --no-single --cflag=-O2
```

The `examples_service_main` program schedules the next whole UTC second on a
dedicated owner and a borrowed two-worker executor. Expected output is `[PASS]`
with one actual callback and one success result. It usually takes under a second.
It joins before consuming output, reclaims original business data through a
retirement receipt, drains metadata, destroys the service, then closes/joins the
host pool. No files or background threads remain. Every owned optional error in a
general consumer must be released with `xrtErrorFree`; this success example asserts
that those error pointers are NULL.
