# Shared ASKING example

This offline example uses a local stateful protocol peer, creates a fixed
managed shared member, executes an adjacent ASKING/binary query with one native
Future, and inspects both outcomes. Retained replies survive engine shutdown.

```powershell
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_pool_shared_asking --no-single
```

Use the same API with an automatically maintained shared pool. Select the ASK
target explicitly; this leaf does not follow redirects. Both commands are
pipelined, so Query can execute even if Control is an error. Inspect both
outcomes. No accepted request is replayed. Outage/overload rejection is immediate.
Retain Pool during APIs and Close/await Closed/Release normally.
