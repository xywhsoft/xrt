# Native endpoint retained-handle quota validation

`tests/test_endpoint_quota.c` exercises the public native endpoint API. It
measures the full negotiating-peer reservation and the charge retained by a
closed user handle, then binds a new endpoint with a budget that is one byte
short of admitting another peer when two such handles remain alive.

Each process performs 64 cycles. It checks that closed peers release their
slots and runtime reservation, retained handles remain charged, a refused dial
returns `XRUDP_BACKPRESSURE` without an output or accounting change, and
destroying one handle immediately permits a new dial. Each cycle completes
three peer abort callbacks and returns to the original reservation. Both
engines are stopped and joined before the closed endpoint handles are freed.

The 2026-10-19 development run passed all 16 combinations of Windows GCC 16.2
and Linux Clang 21 with ASan/UBSan/leak detection, IPv4/IPv6, Select and explicit
IOCP/Epoll, and modular all-features versus single-header endpoint-only builds.
This is 1,024 quota rejection/recovery cycles and 3,072 pressure peer closes,
in addition to the measurement phase. No runtime defect was reproduced; the
production runtime remains `3d55ec41`.

Private reproducibility records are in `out/xrudp-endpoint-quota/`; development
logs are `dev/xrudp-endpoint-quota-*-dev-r3.log` and
`dev/xrudp-endpoint-quota-*-single-dev.log`. They include source and binary
hashes, compiler commands and per-process results. This bounded admission test
does not establish handshake flood resistance, real Internet NAT reachability,
or 72-hour stability.

The same 16 processes were rebuilt after committing test revision `ed000181`.
`out/xrudp-wire-interop/verify_endpoint_quota.py` rechecked every captured source
and executable hash, the complete family/backend matrix and normal completion.
The aggregate receipt is `out/xrudp-endpoint-quota/acceptance.json`; fixed logs
use the `*-fixed.log` suffix. These committed runs also passed, including all
eight Linux sanitizer processes.
