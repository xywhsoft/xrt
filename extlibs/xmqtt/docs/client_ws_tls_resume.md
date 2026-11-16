# MQTT WSS TLS credential handoff (developing)

Select `xmqtt_client_ws_tls_resume` to reuse `xmqttClientTlsResumeInfo` and
`xmqttClientTlsTakeResume` on ordinary WSS attachment, numeric/DNS/prepared
HTTP setup and URI factories. No new cache/result API, shared runtime or public
configuration layout is added. The existing cache has one latest immutable
native credential and charges its saved data to MaxPrivateBytes.

Direct native attachment snapshots actual native capture capacity; an empty
queue does not indicate disabled capture. Native ClientAuth forces capacity0.
HTTP factories preserve the same configured policy before DNS and after WS event
handoff. After attachment, capture drains already queued credentials once and
receives later ticket-only events without waiting for WS/MQTT application bytes.
Core owns immutable queues/limits/expiry/trust; the caller still configures the
native bounded queue and owns configuration/callback code through real joins.

Notify remains coalesced in ordinary Client work, outside native TLS/WS frames.
Info describes authenticated TLS independently of MQTT CONNACK/auth proof. Each
full or resumed TLS connection must still complete fresh MQTT readiness; no
0-RTT or implicit MQTT request replay is introduced. Captured credentials can
outlive Client/Engine and be supplied to an explicit next factory; the ordinary
factory snapshots/retains them before DNS. Stop external calls before Destroy.

Without this leaf, direct WSS attachment retains its prior behavior and native
HTTP WSS requires ResumeLimit0. Plain WS never acquires a TLS cache. Selecting
this leaf also selects the existing TLS-client/resume and native WSS-ticket
capabilities and their real code/storage costs. Direct WSS adds the existing
charged cold TLS state; it adds no new Client/WS public field or payload wrapper.
The existing work budget bookkeeping and cold capture/Notify/lease costs remain
real. This is not zero-overhead, controlled performance or production acceptance.

Run the registered suite and [trusted example](../examples/ws_tls_resume/README.md):

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_tls_resume_tests --cflag=-O2
```

Windows select/IOCP and WSL select/epoll cover both MQTT versions, seven routes,
real pre-attachment and ticket-only post-Upgrade capture, exact cache charging,
budget1 handoff after Wait, concurrent takers, reentrant Notify/Close, native
fallback/ResumeOnly rejection, inferred identity mismatch, constructor OOM,
challenge/final-proof/reauth isolation, and twelve automatic PSK plus MQTT Session
ACK-cut recoveries. WSL ASan/UBSan/leaks and separate TSan are distinct from
native Linux acceptance. Static/shared and standalone declarations are separate.

The independent OpenSSL HTTP/WS gateway fronts a real Mosquitto TCP broker and
checks session_reused, full fallback after ticket-key loss, retained broker
Session, fresh CONNECT/CONNACK/PUBACK, framing/control and strict closes. Gateway
tickets may precede HTTP; native tests separately prove post-Upgrade ticket-only
delivery. This is not broker-native WS qualification; prior native broker close
failures remain unresolved.

`bench_client_ws_tls_resume.c COUNT MODE INSTRUMENT` compares capture disabled0,
idle1, cached2 and an actual PSK-resumed channel3 using the same selected code,
32-byte QoS1/window32, checksum and physical joins. Cold native setup, cached
data bytes and actual XRT allocator diagnostics remain visible. P50/P95/P99/P99.9,
max and whole-process CPU include both peers; no fixed-arrival/comparator or
complete native/CRT/kernel cost claim is made. See the
[source-bound report](../../../docs/development/xmqtt-wss-resume-20261116.json)
for actual counts, quality failures and preserved attempts.

Durable/crash state, broker-native close/adversarial/scale/load qualification,
controlled performance and native Windows/Linux >=72h soak remain outstanding.
The strict four-product release gate remains closed.
