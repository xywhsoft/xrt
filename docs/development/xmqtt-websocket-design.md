# MQTT WebSocket integration design

Scope remains the full MQTT5/3.1.1 TCP/TLS/WS client with authentication, Session,
Inbox, automatic reconnect, durability and final production/performance gates.
This document does not declare a transport module implemented.

## Required behavior

Follow [MQTT5 chapter6](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and the corresponding MQTT3.1.1 transport rules independently. Checked HTTP Upgrade
must offer/select exactly the MQTT subprotocol. MQTT octets use Binary data;
Text is terminal. Packet parsing spans arbitrary WebSocket frame/message splits
and coalesced packets. Control Ping/Pong/Close stay with Core. Limits bound native
frames, staging, packet input, retained objects and whole-instance resources.
No user business/storage work runs on the network worker.

## Reuse and physical completion

Extend the existing Client transport switches and central constructor, preserving
one owner, protocol parser, QoS identifier/credit map, aliases, auth and Session.
Do not create a second MQTT runtime. Initial open-transport attachment must keep
success-only native handoff and false-output/alias atomicity, then add original-
entry setup clocks across URI/DNS/TCP/TLS/checked Upgrade. Verified WSS policy is
mandatory with no fallback. Attach must prove the selected `mqtt` protocol and
verified native OPEN state using public Core capabilities.

Actual Windows/WSL probes at05f306bc show a masked BinaryRef input owner released
while native output remains pending. Input ref release and global Drain cannot
supply independent QoS0 write-prefix completion. Core now provides optional
native WS/WSS prefix observation. Save Produced only after the accepted message's
Staged flag clears; previously saved cuts remain observable while later local
WSS suffixes are staged. Native Completed reaching a cut certifies local kernel
write retirement, not broker receipt/effects. Close/abort/overflow invalidate
success. Native completion can retire a batch at once. Keep real request/quota
credits through the enclosing native duties, including early QoS acknowledgements.
No per-write Future/watch or separate runtime is required by this primitive.

## Original integration checklist (see dated evidence below)

- Native TCP/WSS attachment plus the existing Client APIs, Session/Inbox/Auth
  variants and physical close/Wait/Destroy. Use existing original entry clocks;
  accepted partial MQTT output retains its conservative uncertainty.
- Feed callback-borrowed WS bytes directly into the existing parser when budgets
  permit. Pause at chunk boundaries; only unavoidable remainders use bounded,
  charged reusable staging. Measure each copy/allocation rather than promising
  zero-copy. Never reset MQTT framing at WS message boundaries.
- Bound per-turn packet/byte work across all callbacks and sends. Preserve
  deterministic backpressure, typed overload/error and graceful MQTT DISCONNECT
  followed by bounded native WS/TLS close. Numeric/URI/DNS/prepared setup must not
  duplicate protocol state or refresh setup/request deadlines.
- Exercise full QoS0/1/2, aliases, subscriptions, proof-gated authentication,
  Session recovery and reconnect factories on the same Client transport. Persist
  state with separately explicit durability/acknowledgement ordering.
- Literal fragmented/coalesced independent peers and real broker WS/WSS,
  adversarial/OOM/partial-write/close/cancel/lifetime, packages/singles, native
  Windows/Linux throughput/CPU/queue-inclusive tails and>=72h acceptance.

The new Core prerequisite and its evidence are recorded separately in
[the write-progress stage](websocket-write-progress-20261116.json). Full MQTT
WebSocket delivery and all four-product acceptance remain incomplete.


Implementation update 2026-11-16: native post-Upgrade attachment, ordinary Client
integration and selected Auth/Session recovery now have source-bound evidence in
[xmqtt-websocket-20261116.json](xmqtt-websocket-20261116.json). Host-native HTTP
establishment and original whole-setup clocks are covered by the subsequent
native setup stage below. Reconnect factories and broader production/performance
acceptance remain outstanding; the full checklist is not yet accepted.


Native setup update 2026-11-16: numeric, counted DNS and prepared WS/WSS factories
now perform strict native HTTP Upgrade under the original Client clock. See
[xmqtt-websocket-connect-20261116.json](xmqtt-websocket-connect-20261116.json).
URI/reconnect, credential capture after WSS handoff, durability, clean broker-close
qualification and production/performance acceptance remain outstanding.


URI setup update 2026-11-16: strict ws/wss parsing and native IP/decoded-name URI
factories now have source-bound evidence in
[xmqtt-websocket-uri-20261116.json](xmqtt-websocket-uri-20261116.json). Original
clocks, explicit TLS verification/SNI policy and optional Auth/Session/Inbox
remain on the ordinary Client. Automatic reconnect, post-handoff credential
capture, durability, clean broker-close and final performance/soak gates remain.


Automatic recovery update 2026-11-16: [reconnect_ws.md](../../extlibs/xmqtt/docs/reconnect_ws.md)
and [the source-bound report](xmqtt-websocket-reconnect-20261116.json) qualify
ordinary WS/WSS factories with existing controller/private Session/Inbox, leases,
subscription/auth/replay/retirement and independent broker/gateway transport.
No production runtime or new API is introduced. Post-Upgrade credential capture,
durable state, broker-native clean close, controlled performance and native soak
remain required; gateway and WSL evidence do not replace those gates.


Native TLS-ticket prerequisite update 2026-11-16: the separately selected Core
`xrtWsStreamSetTlsTicket` now supplies the missing post-handoff notification
without replacing WS event ownership. See
[websocket-tls-ticket-20261116.json](websocket-tls-ticket-20261116.json). It reuses
native credential queues and existing callback Data/lifetimes; it does not yet
wire the MQTT resume cache or complete the original full WebSocket checklist.

WSS credential integration update 2026-11-16: the optional
`xmqtt_client_ws_tls_resume` leaf now wires the native ticket callback and one-time
pre-attachment queue drain into the existing MQTT one-entry cache. Direct native
attachment needs an actual capacity snapshot; Count0 cannot distinguish empty
from disabled. The minimal Core `xrtTlsClientResumeCapacity` query reads existing
immutable state without allocating or mutating it. It adds no Core field/runtime.
Ordinary Client/TLS/WS ownership, cache quotas, deferred Notify, MQTT auth proof,
fresh CONNECT and controller Session/Inbox rules remain authoritative. See
[xmqtt-wss-resume-20261116.json](xmqtt-wss-resume-20261116.json) and the dedicated
example for evidence and real selected/cold costs. Independent OpenSSL gateway
results do not qualify broker-native WS, controlled performance or native soak.
