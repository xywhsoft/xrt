# Atomic ready-lease batch

Run from the repository root:

```text
python tools/build.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --suite xcron_executor_selected
```

Expected output: `Selected worker batch preserved both nominal labels.`

The offline example selects both occurrences of a repeated New York01:30 label,
submits the two already-ready leases atomically to a borrowed two-worker XRT
executor and immediately poisons the caller's input array. Both callbacks read
their own copied nominal snapshot. Each physical receipt completes its original
scheduler lease; Close, pool join, receipt collection and job retirement precede
resource release. Native batch rejection transfers neither duty, so cleanup
completes the two caller-owned leases. No timer waits to fill a batch, network
service or machine-local time zone is used.
