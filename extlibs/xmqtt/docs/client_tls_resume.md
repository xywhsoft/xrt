# TLS credential handoff (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_TLS_RESUME` before Core feature processing.
It adds a one-entry latest TLS1.3 credential cache to the existing TLS client.
No existing public configuration/result layout changes. `Session.ResumeLimit`
still bounds the native queue; zero disables capture. Native mutual-auth clients
currently disable ticket publication and reject explicit resumption.

`xmqttClientTlsResumeInfo` reports authenticated TLS establishment/resumption,
cache data bytes and capture/eviction/rejection counts. This is independent of
MQTT readiness and session state. Only successful native queue handoffs count;
native queue eviction/disabled/OOM drops are outside these counters. Counter
arithmetic saturates. Snapshots and `xmqttClientTlsTakeResume` are thread-safe
while borrowing a live Client, including after physical Client wait.

Take transfers one existing native reference, refunds the local cache data-byte
charge and makes no allocation/copy/post. Empty leaves output and ambient error
unchanged. The caller owns storage/budget until `xrtTlsResumeRelease`. A new TLS
factory captures `Session.Resume` before DNS, so the caller can release its alias
immediately after successful admission. Credentials can outlive Client/Engine;
keep Core code resident through final release. Never log ticket/PSK contents.

MaxPrivateBytes charges the extra selected TLS state plus ticket, secret,
identity/name/ALPN bytes and saved peer-chain DER while cached. Optional capture
refusal preserves an older credential and the healthy connection. Native opaque
credential metadata, shared parsed-chain storage and native bounded queue remain
under Core policy/verify limits outside this local data-byte accounting. External
credentials have the new caller's ownership/budget. There is no second DER or
credential copy, adapter queue node or new shared runtime. Native ticket callbacks
only move immutable references and coalesce existing Client work. User Notify
runs in that work, outside native callback frames.

Windows/WSL x64 measurements retain existing public TLS config200,
ClientConfig272/Info160 and request header104 bytes. The new snapshot is48 bytes;
the selected private TLS state is160 instead of88 (+72 per TLS client). The
capture-disabled leaf stays88; the private Client object itself does not grow.

Native identity/ALPN/expiry/verifier policy applies to resumption. `ResumeOnly`
requires PSK acceptance; otherwise native full-handshake fallback may apply. No
0-RTT MQTT data, reconnect, persistence or protocol-request replay is introduced.
Each connection still performs CONNECT/CONNACK/authentication. Native credential
handoff currently covers TLS1.3; TLS1.2 full handshakes remain supported.

See the [example](../examples/tls_resume/README.md). Production/broker and qualified
performance acceptance remain outstanding; validation evidence is recorded as
the stage is completed.

Protocol references: [RFC8446 PSK resumption](https://www.rfc-editor.org/rfc/rfc8446.html#section-2.2)
and [NewSessionTicket](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.6.1).
The independent oracle checks the server's
[`session_reused`](https://docs.python.org/3/library/ssl.html#ssl.SSLSocket.session_reused)
flag, fresh literal MQTT exchanges and authenticated shutdown.

Stage evidence: [validation and measured limits](../tests/results/client-tls-resume-20261109.json).
