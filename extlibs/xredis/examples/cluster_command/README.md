# Metadata-checked Cluster calls

From the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_command --exclude-test "*" --no-single
```

This runnable local-peer example creates a Cluster client and a retained command
catalogue, submits a binary GET using its calculated slot, releases the catalogue
immediately, and keeps the result through full client/node/engine teardown. The
literal peer echoes GET's key as its value; real server semantics are checked by
the owned Redis interoperability probe. There are no external service settings.

Applications fetch COMMAND/COMMAND INFO using their authorized server connection,
create an immutable catalogue and keep one owner throughout each CallCommand.
Only ACCEPTED publishes a Future. UNKNOWN/UNSUPPORTED need explicit metadata or
server-assistance policy; CROSS_SLOT is a local refusal. NO_SLOT rejects keyless
commands; a0..16383 fallback is an explicit application choice. This stage adds
no automatic command metadata fetch or server-assisted extraction.
