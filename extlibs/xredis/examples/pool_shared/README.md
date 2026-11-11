# Shared native pool example

This offline program starts two local Redis protocol peers on one XRT engine,
creates managed members, and pipelines eight binary ECHO requests without leases.
It closes the pool and engine before inspecting retained replies.

From the repository root:

```powershell
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_pool_shared --no-single
```

Replace the peers with ready managed numeric/DNS/TLS clients using the same
service, database and credential profile. Retain the pool across caller APIs;
Close and await Closed before Release/engine shutdown. Overload is immediate,
not a queued acquisition. Isolate blocking traffic; no key affinity, reconnect,
redirect or business replay is implemented by this leaf.
