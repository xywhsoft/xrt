# Typed store transaction example

`python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite
xmqtt_store_delta_tests` builds/runs the example. Without arguments it encodes
and decodes a3-record MQTT5 batch: bound identity, accepted-open metadata and
canonical incoming QoS2 PUBLISH. Payload/delivery and receive identity use the
same IN_ACCEPT record. This is an illustrative format example; it performs no
broker authentication, MQTT state mutation, ACK output or application delivery.

Optionally pass an existing exclusively owned file. Empty initializes and
synchronizes the batch once; nonempty validates/replays its committed records.
It does not append a duplicate BIND after recovery. Keep binding/generation and
schema unchanged, and supply a file whose directory entry has already been
durably published under the host's filesystem policy. The journal borrows the
file; this example closes it. Never run blocking sync in a network callback.

The [codec contract](../../docs/store_delta.md) describes lifecycle/epoch/key
validation still required in the full store. Transaction grouping cannot move a
required durable boundary past network output. See the
[store execution plan](../../../../docs/development/xmqtt-store-design.md).
