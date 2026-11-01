# TCP client wire interoperability

`run_client_wire.py` is an independent Python standard-library TCP peer. It
does not use xmqtt codecs or third-party MQTT code. Prerequisites: Python 3,
GCC, a local loopback socket and the built probe. It covers 31 scenarios across
MQTT 3.1.1/5: split/coalesced frames, original copied CONNECT credentials and
identity, assigned identity, copied publish input, retained binary/property
views, refusals, setup timeout, ordering, malformed headers, advertised/local/
private/object/receive bounds, unsupported QoS, aliases/auth, server DISCONNECT,
truncated EOF, and a transport close after a PUBLISH prefix. An abort may appear
as EOF or TCP reset. No broker comparison or performance claim is made.

From the repository root, Windows with GCC:

```
python tools/package.py --compiler gcc --arch x64 --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client --kind static --verify --cflag=-O2
gcc -std=c11 -O2 -Wall -Wextra -Werror -DXMQTT_MODULE_XMQTT_CLIENT -Iinclude -Iextlibs/xmqtt/include extlibs/xmqtt/tests/interop/client_probe.c release/gnu/x64/xmqtt_client/static/libxmqtt.a release/gnu/x64/xmqtt_client/static/libxrt.a -lws2_32 -o release/gnu/x64/xmqtt_client/static/wire-probe.exe
python extlibs/xmqtt/tests/interop/run_client_wire.py --probe release/gnu/x64/xmqtt_client/static/wire-probe.exe --report release/gnu/x64/xmqtt_client/static/wire-report.json
```

On Linux, use package `--arch native`, change both package paths from `x64` to
`native`, replace `-lws2_32` with `-lm -pthread` and omit `.exe`. Shared builds
also define `XRT_USE_SHARED`/`XMQTT_USE_SHARED`; link the generated import
libraries on Windows or `.so` files on Linux, and include that package directory
in PATH or LD_LIBRARY_PATH when running. Tests close sockets and reap the child
process; outputs remain in the ignored release directory. They can be removed
after inspection, without changing source.

The in-process fault/OOM/lifetime fixture is `../test_client.c`; the separately
documented runnable example is `../../examples/client/main.c`. These peers are
development fixtures. Pinned real-broker interoperability, reconnect/QoS/durable
scenarios, performance comparisons and native >=72h qualification remain.
