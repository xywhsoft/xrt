# Role-verified connection initialization

The developing xredis_role_client leaf adds optional initialization flags
REQUIRE_MASTER, REQUIRE_REPLICA and REQUIRE_SENTINEL. Choose at most one.
ROLE runs after HELLO/auth/name/SELECT, before Ready or queued business calls.
All existing numeric, hostname, prepared-address and TLS factories share this
barrier and the original setup deadline. Missing ROLE support fails closed;
AUTO fallback applies only to HELLO and never bypasses role verification.

Role payloads use the existing immutable decoder with Reader.MaxValues and
Reader.MaxFrameBytes bounds. The owned parser retains its existing depth,
container, bulk, feed and memory limits. The additional command is a fixed
14-byte tail of the original bounded/wiped initialization buffer. MaxWireBytes
counts it plus all AUTO fallback bytes. Stream.WriteLimit applies to the slice.
No request/Future/worker, native object field or wire-offset table slot is added.

Mismatch is READY_ROLE_MISMATCH at INIT_ROLE. Protocol/resource/server/auth/
timeout/closed outcomes retain their typed setup status. Error text remains
redacted. A successful Ready keeps only its original HELLO map, and retains no
ROLE reply. Cancellation of a readiness observer does not cancel setup; queued
requests retain their own deadlines/cancellation and remain unsent on failure.

Replica identity alone does not establish freshness. Verification is a point in
time and must run for each new transport. Sentinel discovery/re-resolution,
stale endpoint replacement and Cluster routing remain required next stages.
[Runnable example](../examples/role_client/README.md).
