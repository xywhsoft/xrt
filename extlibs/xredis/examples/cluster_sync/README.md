# Synchronous Cluster call

Run from the repository root:

```
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_cluster_client_sync --exclude-test "*" --no-single
```

The example owns three local literal peers and a maintained node cache, then
issues a binary ECHO through an initially cold Cluster client. Check admission,
then the actual wait, then the typed route/query result. The explicit slot is
the caller's responsibility. The reply is read again after Client/Nodes/Engine
teardown, and the accepted Future is destroyed exactly once. It needs no external
Redis process or credentials. Production endpoints still need application policy
and native Connect factories as shown in the Cluster client example.

An accepted result owns Future even if Wait is XWAIT_ERROR. Keep that Future for
later wait/cancel/cleanup; business must not be replayed merely because waiting
failed. Blocking calls refuse every worker in either dependent Engine.
