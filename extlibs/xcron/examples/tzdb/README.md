# Named immutable snapshots

From the repository root:

```console
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tzdb
```

The offline example loads two actual fixed-offset TZif revisions under
`Demo/Office`, holds the old snapshot/zone during replacement and verifies old
and new rules after physical manager destruction. Input bytes are mutable after
Load returns. No installed data, host timezone or service is needed. See
[ownership, quota and performance costs](../../docs/tzdb.md).
