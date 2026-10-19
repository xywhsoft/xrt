# Endpoint admission boundary checks

`tests/single/test_single_endpoint_admission.c` calls the actual generated
endpoint rate gate with deterministic timestamps. The vectors verify that
changing ports does not evade an IP limit, IPv6 interface scopes stay distinct,
the 64-slot table refuses unknown addresses without evicting current entries,
known addresses retain their remaining allowance, and expired slots are reused.
They cover 10,000 full-table rejections, 256 window rotations, aggregate limits
and the exact 999999/1000000 microsecond boundary. XRT heap failure is armed for
the entire vector suite; no XRT allocation is attempted.

Development builds passed Windows GCC 16.2 and Linux Clang 21 with
ASan/UBSan/leak detection, with both endpoint-only and all-feature single-header
profiles. Logs are `dev/xrudp-endpoint-admission-*-dev.log`; the private receipts
in `out/xrudp-endpoint-admission/` record the compiled input and binary hashes.

This is a deterministic internal component check. It does not send UDP or
establish whole-endpoint CPU fairness under a hostile network. The public
native endpoint suite separately checks actual untrusted HELLO/forged-CID
traffic, reservation stability and survival of an authenticated connection.
No runtime defect was reproduced by these component vectors; production
runtime code remains `3d55ec41`.

The committed `a148a934` vectors were rebuilt in all four configurations and
passed. The native endpoint suite was also rebuilt from committed sources for
Windows Select/IOCP and Linux Select/Epoll: four actual native processes passed
all 24 lifecycle cases, including 464 untrusted HELLO/forged-CID datagrams,
stable reservations and surviving authenticated peers. All native engines
joined; all four Linux vector/native processes enabled ASan/UBSan/leak checks.

`out/xrudp-endpoint-admission/acceptance.json` records the combined result:
40,000 deterministic full-table rejections and 1,024 virtual window rotations
are component counts; the 464 untrusted packets are separately actual UDP
traffic. `out/xrudp-wire-interop/verify_endpoint_admission.py` checked every
source, cached core object, binary and the native case/backend set. Fixed logs
use `dev/xrudp-endpoint-admission-*-fixed.log`. No result in this record claims
large-scale CPU flood acceptance or 72-hour stability.
