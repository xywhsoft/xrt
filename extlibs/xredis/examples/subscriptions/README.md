# Desired subscription restoration

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_subscriptions --exclude-test "*" --no-single
```

The standalone offline example owns a deterministic local protocol fixture. It
commits a binary desired name, retains an immutable snapshot, creates a fresh
transport and drives bounded recovery using native request Futures. A restored
revision is distinct from later desired edits. No external Redis is contacted.
Every Client/Engine is physically closed; the message and desired snapshot remain
valid afterwards. Production event-driven callers schedule another turn for MORE
and a native Future Watch for WAIT, rather than blocking an Engine worker.
Automatic backoff/endpoint selection and Cluster routing are separate work.
