# Negotiated connection constraints

`xmqttNegotiate` independently validates CONNECT/CONNACK descriptors using the
selected complete codec, then checks their cross-packet relationship. The
default config selects MQTT 5.0, no local session state and finite codec limits.
There is no implicit downgrade to 3.1.1. `xmqttNegotiatedPacketCheck` independently
preflights one subsequent client packet and returns its encoded size only when
both codec and negotiated per-packet constraints allow it. Neither function
sends packets, mutates application/session state or creates a shared runtime.

The implementation follows the independent
[MQTT 5.0 standard](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
sections 3.1.2.11, 3.2.2 and 4.12, and
[MQTT 3.1.1 Errata 01](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)
section 3.2.2. Reference client implementations are not implementation sources.

| Contract | Behavior |
| --- | --- |
| Peer refusal | REJECTED is a normal result; preserves version/reason/optional server-reference bytes, exposes no usable negotiated capabilities |
| Sessions | Clean Start/Session forbids Session Present; MQTT 5 missing local state with Session Present is STATE_ERROR, requiring close rather than pretending to resume |
| Version difference | 3.1.1 leaves unexpected session-state policy to the caller; a continuing MQTT 5 client must discard old local state when Session Present is zero |
| Client id | Accepted empty MQTT 5 id requires nonempty server assignment and a new session; an existing nonempty id cannot be replaced; redundant identical assignment is accepted |
| Authentication | Successful enhanced-auth CONNACK requires the original method, including present empty text; unsolicited method/data is rejected; refused connections need not echo the original method |
| Limits | Separate send/receive maxima, alias capacities, server keep-alive override and original/effective expiry; packet limit is clamped to the framing protocol maximum |
| Capabilities | Maximum QoS, retained messages, wildcard/shared subscriptions, subscription identifiers; a server accepting an incompatible Will is rejected |
| Response metadata | Unrequested Response Information is forbidden; requested information remains optional; problem-information=0 still permits CONNACK diagnostics |
| Packet checks | Peer packet cap, PUBLISH QoS/retain/alias, SUBSCRIBE availability and AUTH method; original CONNECT expiry prevents a later illegal positive DISCONNECT expiry even after server override |

Receive Maximum defaults to 65535 and applies to QoS 1/2 flow control. This
leaf returns the protocol limit; it does not count/reserve QoS credit, identifier
ownership or retransmission obligations. Topic Alias Maximum defaults to zero;
the check bounds alias numbers but does not maintain their topic mappings.
Maximum QoS does not restrict the requested QoS in a SUBSCRIBE. Availability
restrictions do not prevent an UNSUBSCRIBE from removing old subscriptions.
Session Expiry has no MQTT 3.1.1 meaning here. Preserve the version explicitly.
For 3.1.1, capability fields represent non-negotiated baseline values, not
discovery of MQTT 5-only features or broker extensions. The 3.1.1 codec still
rejects MQTT 5-only fields independently of these capability flags.

Results are caller-owned values. ClientId/AuthMethod borrow the relevant original
CONNECT/CONNACK text bytes, while ServerReference/ResponseInfo borrow CONNACK
bytes. Property scratch can be discarded after the call. Keep borrowed byte
ranges alive and unchanged while using a successful result; copy them into
bounded connection-owned storage if needed. Use that successful value unchanged.
No authentication data, username/password or Will payload is retained in it.
Method equality is only protocol consistency, not credential verification.

DONE/REJECTED publish the result and clear optional Issue without allocating or
changing the ambient error. Every negative status preserves result/Required.
Valid semantic failure publishes typed Issue independently of diagnostic OOM.
Invalid API/descriptor/config/output-alias failures return LOCAL_ERROR and leave
Issue unchanged. Callers needing wire syntax details use the packet codec Issue.
All output spans must be disjoint from each other, active descriptor vectors,
active text/binary/payload spans and config. Wire metadata is ignored, matching
the descriptor encoder contract. Inputs must be real readable C objects; range
checks cannot authenticate arbitrary memory addresses.

Costs are explicit: two codec descriptor preflights per negotiation, one per
outbound check, linear property extraction/alias walks, constant scalar defaults
and a fixed output value copy. Text validation occurs in codec preflight; auth/id
equality and disabled-wildcard checks can add scans. Borrowed payload/text is not
copied; there is no DOM, owned receive storage or success allocation. Diagnostics
may allocate. This is not end-to-end zero-copy or a performance acceptance claim.

```sh
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_tests --no-single --no-examples --cflag=-O2
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt --no-single --cflag=-O2
out/gcc/native/xmqtt/examples_negotiation_main
```

The offline example needs no broker/files or cleanup. It decodes literal peer
packets, prints `keep-alive=30 max-qos=1 receive-max=4 alias-max=8 packet-max=1024
publish-bytes=13`, and proves a QoS-2 publish is restricted. Select `--arch x64`
and its output path on Linux. A network client must still implement connection
ordering, enhanced-auth exchange state, keep-alive timers, QoS credit/ACK/id and
alias state, expiry/reconnect/session persistence, inbound negotiated checks and
transport ownership. These functions are prerequisites, not that client runtime.
