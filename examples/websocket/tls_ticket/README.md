# Native WSS TLS ticket notification

This local, runnable attachment demo performs genuine TLS1.3, verifies the exact
local fixture certificate/name, attaches both native WS peers, issues a real TLS
credential without WS application data, removes its observer from the callback,
and joins WS/TLS/TCP/Engine before releasing an independently owned credential.
It uses only public native APIs and the existing local certificate fixture.
No ticket/PSK contents are printed. No external service or credential is needed.

```text
python tools/build.py --compiler E:/software/w64devkit/bin/gcc.exe --arch native --suite websocket_stream_tls_ticket_tests --no-examples --cflag=-O2
python tools/build.py --compiler E:/software/w64devkit/bin/gcc.exe --arch native --suite websocket_stream_tls_ticket_tests --cflag=-O2
```

On Linux/WSL use `--compiler /usr/bin/gcc --arch x64`. The build tool compiles
and runs the registered program. Individual example output lives under
`out/gcc/<arch>/websocket_stream_tls_ticket_tests/websocket_tls_ticket_main`
(with `.exe` on Windows).
The protocol/fault/lifetime tests are separately registered in `tests/websocket/`.

A network-facing host must validate its HTTP Upgrade before attachment. This is
a Core transport/credential demo, not an HTTP or broker interoperability witness.
The optional callback is bounded and runs directly inside the native TLS owner
frame; schedule business work elsewhere. Its context/code lives through real
retirement. Registration does not replay queued credentials; drain the existing
native queue on owner when installing late. Native ResumeLimit and trust/expiry/
identity policy remain authoritative. See [the API guide](../../../docs/api/websocket.md).
