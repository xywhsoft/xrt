# MQTT client interoperability

## Pinned real broker

`broker.lock.json` pins the optional Eclipse Mosquitto 2.1.2 test dependency,
official source archive SHA256, licenses, fixture packages/tools and runtime
library hashes. Download from the recorded source URL and verify the checksum
before extracting. Broker sources and libraries are not product dependencies
or protocol implementation references. The fixture builds upstream unchanged;
only its public build files, configuration and CLI/public APIs are used.

`broker_probe.c` uses public xmqtt/Core APIs. `run_broker.py` starts two private
loopback broker processes, with anonymous access then runtime credentials, and
terminates/reaps each process on success or failure. It creates temporary CA,
certificates, private keys, password file and client credentials. Only public
certificate hashes, binary hashes and test outcomes enter the report. No service
is installed and no TLS key log is enabled. Prerequisites: Python 3, OpenSSL CLI,
GCC, a built pinned broker and its `mosquitto_passwd` tool.

From the repository root (Linux; add `.exe` for a native Windows probe):

```
python3 extlibs/xmqtt/tests/interop/build_broker_probe.py --kind modular --output out/broker/probe
python3 extlibs/xmqtt/tests/interop/run_broker.py --broker /absolute/path/to/mosquitto --passwd /absolute/path/to/mosquitto_passwd --client out/broker/probe --report out/broker/result.json
```

`--kind single` compiles the same assertions through generated single headers.
For static/shared consumers, first package
`--suite xmqtt_client_tls_resume,tls_identity_rsa` using `tools/package.py`
with `--manifest extlibs/xmqtt/config/modules.json --verify`, then pass its
printed output directory as `--package-dir` to the probe builder. The builder
uses the manifest closure and links platform dependencies; Windows shared DLLs
are copied beside the probe and Linux shared consumers get the package rpath.
Use `--sanitize address,undefined` or `--sanitize thread` with modular/single
builds to instrument the implementation as well as the probe.

Each full run executes 68 cases. MQTT 5/3.1.1, TCP/TLS and numeric/hostname/
prepared-endpoint factories cover binary QoS0/1/2, both QoS2 acknowledgements,
subscriptions/unsubscriptions, MQTT5 outgoing aliases and No Local ordering,
retained delivery/deletion, abnormal-close Will delivery, authentication success/
refusal and actual Client/Resolver/Engine retirement. A retained message remains
readable after those owners are destroyed. Additional small read/write/chunk/
packet budgets force fragmentation. These are protocol checks, not throughput
or tail-latency measurements.

The session cases independently seed a real retained broker session using literal
Python CONNECT frames, observe Session Present=1, then require the current xmqtt
client to return `XMQTT_STATUS_SESSION`. This proves an unsupported session fails
closed; it does **not** implement or qualify MQTT session recovery. When recovery
is implemented, replace these refusal assertions with the required replay/expiry/
identifier/duplicate-delivery assertions. No test skips that missing feature.

The recorded fixture uses a WSL Linux broker. Linux probes run in WSL; native
Windows probes can also connect through WSL localhost forwarding by supplying
the `.exe` to the Linux runner (runtime file paths use `wslpath -w`). This does
not qualify a native Windows broker or a native Linux production deployment.
The official Windows installer download failed in this environment; incomplete
downloads are retained as failed evidence and were never executed.

Reproducing the isolated Linux broker build, with the exact packages in the lock
file downloaded using `apt-get download` and extracted using `dpkg-deb -x`
into `$p/deps` (no global package installation):

```sh
p="$PWD/out/broker-fixture"
cmake -S "$p/source/mosquitto-2.1.2" -B "$p/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DWITH_TLS=ON -DWITH_DOCS=OFF \
  -DWITH_DYNAMIC_SECURITY=OFF -DWITH_CTRL_SHELL=OFF -DWITH_WEBSOCKETS=OFF \
  -DWITH_STATIC_LIBRARIES=ON -DWITH_TESTS=OFF \
  -DOPENSSL_INCLUDE_DIR="$p/deps/usr/include" \
  -DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.so.3 \
  -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.so.3 \
  -DCJSON_INCLUDE_DIR="$p/deps/usr/include/cjson" \
  -DCJSON_LIBRARY="$p/deps/usr/lib/x86_64-linux-gnu/libcjson.so.1.7.19" \
  -DCMAKE_C_FLAGS="-I$p/deps/usr/include/x86_64-linux-gnu -I$p/source/mosquitto-2.1.2/libcommon -I$p/source/mosquitto-2.1.2/deps"
cmake --build "$p/build" --parallel 4 --target mosquitto mosquitto_pub mosquitto_sub libmosquitto mosquitto_passwd
```

The extra include paths satisfy upstream static-target header lookup without
editing upstream. Adjust absolute system paths only for a different host and
record that host's versions/hashes separately. Configuration, bootstrap failures
and raw case logs for this run are under the evidence directory referenced by
`../results/broker-20261109.json`. Native broker variants, durable/reconnect/
expiry recovery, independent client performance comparisons, qualified deployment
P99 and native >=72h qualification remain release gates.

## Independent wire peers

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
development fixtures. Real-broker volatile-client checks are documented above;
reconnect/durable scenarios, performance comparisons and native >=72h
qualification remain.

`qos1_probe.c` plus `run_qos1_wire.py` adds 37 independent sender scenarios
across both versions. Use the same compilation/link procedure with this probe
source and invoke `run_qos1_wire.py --probe ... --report ...`. Cases cover
automatic/explicit/reused/colliding identifiers, out-of-order PUBACKs, negative
and no-subscriber decisions, repeated User Properties and retained ACKs,
Receive Maximum exhaustion with QoS0/PING progress, cancellation/deadline,
metadata/private bounds, and unknown/zero/duplicate/unsent/wrong-kind/bad-reason
or truncated ACKs, plus Request Problem Information=0 rejection/clean-ACK
acceptance. It uses standard-library framing helpers, never xmqtt codecs.
Incoming QoS-1 delivery and QoS-2/session replay still require subsequent work.


The independent incoming QoS-1 matrix uses `receive_probe.c` with
`run_receive_wire.py`. It checks MQTT 5/3.1.1 ordered PUBACKs, fragmented input,
one-byte output, post-ACK DUP/id reuse, duplex identifier independence, ACK
backpressure with callback message taking, partial outgoing-frame integrity,
peer close during ACK, reentrant close, object/queue/private limits, zero ids,
unsupported QoS2, MQTT5 advertised receive-window violations and peer packet caps.
The success peer sends a QoS0 barrier only after observing every expected ACK;
the probe retains messages/properties through Client/Engine destruction. Failure
cases assert absence of success ACKs for unaccepted messages. This does not
qualify graceful/error DISCONNECT, durable acknowledgement or broker parity.

```
python extlibs/xmqtt/tests/interop/run_receive_wire.py --probe PATH_TO_RECEIVE_PROBE --report out/receive-wire.json
```


`disconnect_probe.c` and `run_disconnect_wire.py` add byte-exact diagnostic-close
checks: MQTT5 reasons 0x81/0x82/0x93/0x94/0x95, caps of two/three bytes, one-byte
output, pre-readiness and 311 abort, partial PUBLISH/PUBACK suppression, peer reset,
reentrant CLOSING abort, absolute deadline expiry after an intentionally stalled
adversarial callback, and native queued-output retirement. The large native queue
case permits either completed local retirement or deadline fallback: kernel buffer
ownership and unread peer bytes are not the same state. It still asserts original
protocol classification, bounded physical completion and valid outgoing prefixes.
The earlier Linux blocked-queue result and initial overstrict Windows result are
preserved with exact source inputs under the stage evidence directory.

```
python extlibs/xmqtt/tests/interop/run_disconnect_wire.py --probe PATH_TO_DISCONNECT_PROBE --report out/disconnect-wire.json
```
