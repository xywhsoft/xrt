# Immutable ROLE inspection

The developing `xredis_role` leaf validates existing owned replies; it opens no
connection and adds no client/request state. The implementation stays inside
the reply owner so outputs cannot overwrite any reply metadata or reserved
payload/vector allocation. No extra bridge, queue, allocation or reference
acquisition is involved in a successful read or cursor step.

`xredisRoleRead` recognizes master, replica and Sentinel responses. Only VALUE
publishes output. Unknown envelopes/tags, server errors, malformed recognized
shapes and resource limits have separate statuses and preserve output and the
ambient error. Invalid API arguments set the existing reply argument diagnostic.
All recognized fields and every list entry are checked before any output write.
Ports are 0..65535, offsets are nonnegative except replica -1, numeric tuple
strings are decimal with checked int64 overflow, and native integer tuples are
also accepted. Link state text is borrowed and unknown future values remain
available as LINK_OTHER. Replica identity alone says nothing about freshness.

MASTER exposes Offset and Replicas. REPLICA exposes Offset, MasterHost/Port
and LinkText/Link. SENTINEL exposes Monitored. Only pass the cursor applicable
to Kind to its iterator; inactive fields are not iterator handles.

Limits default to 4096 list entries and 1 MiB scalar payload, including attached
attributes/numeric text. Zero is an actual cap. Reader limits govern prior
parsing/allocation; this view cannot undo parsing work. Host/name bytes remain
binary and are never rescanned or copied. Validation is linear in list entries
and decimal bytes. Cursor steps are constant work for canonical Redis numbers,
otherwise linear in that entry's decimal bytes. Limits and outputs are distinct
caller-owned objects; outputs/cursors cannot overlap any source allocation.

Views/cursors borrow one live immutable Reply. Retain it with ReplyRetain before
releasing its original owner; the original reader/input may already be gone.
Each thread needs its own cursor/output and a live owner reference. Only use an
unmodified matching Replicas or Monitored cursor from a successful Read.

This is a wire building block. Managed initialization [ROLE barriers](role_client.md) are available. Sentinel
address discovery/re-resolution, pool retirement, Cluster redirection and live
failover remain required stages. No automatic retry or readiness guarantee is
implied. The old role_client prototype remains an unfinished archived draft.

[Official ROLE wire documentation](https://redis.io/docs/latest/commands/role/)
and [runnable example](../examples/role/README.md). Tests separately exercise
fragmentation, malformed shapes, limits, ownership/aliasing, zero-heap decoding
and concurrent immutable access. Benchmarks separate already-owned validation
plus traversal, cursor-only traversal, and parse/validate/traverse/release.
These are component throughput/mean costs, not network or tail-latency claims.

The actual-server corpus runner is `tests/interop/run_redis_role.py`. Build
`tests/interop/redis_role.c` against the `xredis_role` SDK, then run
`python3 extlibs/xredis/tests/interop/run_redis_role.py --client /absolute/path/to/probe --report /absolute/path/to/report.json`.
It owns and verifies disposable master/online-replica/offline-replica/Sentinel
PIDs and run IDs, captures RESP2/3, executes the C probe, and reaps processes.
Server replication state uses the system temporary directory; `--runtime-root`
can select another native server filesystem. Wire corpus/logs remain beside
the report. A Windows probe can also run through WSL with the same corpus.

The historical full ROLE draft patch targets revision `039c589a`; its view
portion is now superseded by this implementation. Applying that entire archive
over current source is not an upgrade path. Managed client-role verification is supplied by the separate role_client
module.
