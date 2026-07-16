# HTTP / WebSocket Release Gates

These gates freeze the XRT HTTP/1.1, shared HTTP semantics, HTTPD/XWeb, and RFC 6455 WebSocket contracts. HTTP/2, HTTP/3, and QUIC are outside this phase.

## Windows

```bat
build_HTTP_WS_RELEASE_GATE_x64.bat
```

Override `FUZZ_ROUNDS` and `STRESS_ROUNDS` to change the deterministic fuzz and reconnect counts. Defaults are 1000 and 500.

## POSIX / MinGW shell

```sh
./build_http_ws_release_gate.sh
```

The gate performs:

1. URL, HTTP semantics, HTTP client, HTTPD, XWeb, and XWS unit/integration tests.
2. A standalone `xrt.c` compile with `-Wall -Wextra -Werror`.
3. HTTP allocation-failure and OOM tests.
4. Deterministic split-input drivers for the HTTP/1.1 and WebSocket parsers.
5. Bidirectional interoperability against an independent RFC 6455 peer implemented with only the Python standard library, including handshake, masking, compressed fragmentation, Ping/Pong, and Close.
6. Mixed normal reconnect and 1009 protocol-limit close stress, including successful connection establishment after every fault case.
7. A single-header build and runtime test.

## Native libFuzzer

The deterministic drivers keep parser regression available without Clang, but they do not replace coverage-guided fuzzing. With Clang installed, run:

```sh
RUNS=100000 ./build_fuzz_http.sh
RUNS=100000 ./build_fuzz_ws.sh
```

Both scripts default to `fuzzer,address,undefined`. A crashing input must be retained in a corpus or minimized into a regression test before release.

## xlang Integration Gate

From `D:\GIT\x-lang\demo6`, run:

```bat
build.bat
release\xl.exe run test\cases\stdlib_http_semantics.xl
release\xl.exe run test\cases\stdlib_websocket.xl
release\xl.exe run test\cases\stdlib_websocket_http_upgrade.xl
release\xl.exe run test\cases\stdlib_websocket_tls.xl
```

The shared HTTP Upgrade case is checked in interpreted and static-build modes. Its HTTP server and WebSocket manager must use the same `NetEngine`, and teardown must destroy WebSocket state, then the HTTP server, then the engine.

## Release Decision

- Every command exits with status zero and no output contains `FAIL`.
- ASan, UBSan, and libFuzzer report no bounds error, use-after-free, leak, or undefined behavior.
- Accept/connect continues to work after protocol-limit closes.
- Single-header and modular-header behavior match.
- Public size/version, ownership, threading, backpressure, cancellation, and close semantics are documented contracts rather than implementation accidents.
