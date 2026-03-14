# Linux Debug Merge Record

This directory records the Linux-debugging and Linux-validation changes made
after commit `0c1b065ce4ef95fbff91f5c5d9528a235bf8eb58`.

It exists so the same work can be replayed after the repository is reverted
and updated to a newer upstream state.

## Scope

Included in this record:

- Linux compile/runtime portability fixes for the current tree
- Linux-specific test hardening for `xnet2`
- `xnet2` benchmark portability and teardown fixes
- `xnet-v1` comparison benchmark sources
- spec updates that document Linux validation and v1/v2 comparison results

Not included in this record:

- generated binaries under `dev/bench/xnet_v1/*.exe`
- generated text artifacts under `release/x64/test/` and `release/x86/test/`

## Merge Targets

Tracked modified files:

- `xrt.h`
- `lib/thread.h`
- `lib/netpoll.h`
- `test/test_xnet2_stream.h`
- `dev/bench/xnet2/bench_common.h`
- `dev/bench/xnet2/bench_idle_conn.c`
- `dev/bench/xnet2/bench_stream_server.h`
- `dev/XNET_V2_SPEC.md`

New source files:

- `dev/bench/xnet_v1/bench_v1_common.h`
- `dev/bench/xnet_v1/bench_echo_tcp_v1.c`
- `dev/bench/xnet_v1/bench_echo_tls_v1.c`
- `dev/bench/xnet_v1/bench_udp_burst_v1.c`

## Replay Order

1. Apply `patches/tracked_linux_debug.patch`.
2. Restore the `snapshot/dev/bench/xnet_v1/` source files into the live tree.
3. Re-run Linux validation and comparison commands from `MANIFEST.md`.
4. Reconcile any upstream conflicts, then update the spec if behavior changes.

## Notes

- `xnet-v1` Linux comparison on the Debian 13 host required a privileged
  container for TCP/TLS poller creation to succeed.
- `xnet-v2` Linux stage validation succeeded in a regular Debian 13 container
  on the same host.
