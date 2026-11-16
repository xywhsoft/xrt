# Bounded Session image round trip

This offline example performs a real passive Session checkpoint, destroys its
original owner, restores a new clock epoch and emits the original id with DUP
before an explicit simulated PUBACK. The old immutable packet reference outlives
both Sessions. No socket, file write or durable-ack guarantee is implied.

```text
python tools/build.py --compiler E:/software/w64devkit/bin/gcc.exe --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_session_image_tests --cflag=-O2
out/gcc/native/xmqtt_session_image_tests/examples_session_image_main.exe
```

Linux/WSL uses `--compiler /usr/bin/gcc --arch x64` and the corresponding
`out/gcc/x64/xmqtt_session_image_tests/examples_session_image_main`.

Real hosts still authenticate/complete a fresh CONNACK, join their old transport,
and call Sent only after actual full emission. `xmqttSessionImageRestoreAssigned`
explicitly recovers a server-assigned MQTT5 ClientId under the exact storage
Binding; use `xmqttSessionClientId` before constructing the next CONNECT.
Normal Restore requires the caller's exact ClientId. Binding should identify a
broker, security principal and logical storage slot. Never confuse CRC with
storage authentication or rollback protection.

The future durable store must atomically commit payload/Inbox/protocol state and
order sync barriers before network acknowledgements. See [the image contract](../../docs/session_image.md).
