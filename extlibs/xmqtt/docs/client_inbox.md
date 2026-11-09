# Client Inbox (developing)

Select `XMQTT_MODULE_XMQTT_CLIENT_INBOX` before Core feature processing.
`xmqttClientInboxCreate` creates a passive queue with no Engine, worker, Future,
timer, transport or callback. `xmqttClientConnectInbox` attaches one numeric TCP
Client. Its MQTT version and four object/message caps must exactly match the
Inbox configuration. The default caps match `xmqttClientConfigInit`.

Attachment lasts until actual Client Destroy, including after Close/Wait and
failed readiness. A second attachment and Inbox Destroy fail while attached.
Constructor failure releases the attachment and preserves the caller's output.
Stop concurrent entries before Destroy; concurrent Create/Destroy of a borrowed
Inbox is outside the ownership contract.

Accepted PUBLISH messages enter the same FIFO across successive Clients. Both
`xmqttClientTakeMessage` and `xmqttClientInboxTake` transfer one queued reference;
Inbox Take also works without a Client. Closing or destroying a Client/Engine
does not discard queued messages. Inbox Destroy releases untaken messages.
Taken messages, retained CONNACK/ACK metadata and typed Futures may outlive Inbox
and all Clients/Engines. Their existing quota references own physical storage.

Object count/bytes include all these physical owners across generations. Queue
count/bytes include only queued PUBLISH messages. Taking a message refunds queue
capacity; releasing its last reference refunds object capacity. A new connection
may therefore fail readiness or admission when earlier objects consume its
budget. A full queue closes the connection without acknowledging the new packet
or dropping previously accepted messages. There is no hidden offline queue.

ClientInfo exposes shared queue/object gauges and per-connection counters
(`Received` counts only publications accepted on that Client). InboxInfo samples
queue and object gauges separately, so concurrent snapshots are not atomic across
both sets. Inbox uses its own mutex; no application callback runs under that
mutex or the quota mutex. Stable-owner Take/Info may run concurrently with IO.

One Inbox allocation, one existing quota allocation and two native mutexes are
fixed metadata outside configured payload budgets. A Client borrows the shared
quota instead of allocating a fresh one. There is no new per-message allocation,
copy or reference operation: the existing owned message is linked into Inbox.
The opt-in Client adds one pointer; builds selecting only ordinary Client omit
that field and Inbox code. The enabled receive path takes the existing Client
gate plus one Inbox gate. Public Client configuration/result layouts are unchanged.

This is application payload preservation, not protocol session recovery. Incoming
QoS1/2 protocol state still belongs to the connection; the independent Session
module has not yet been attached to Client. SessionPresent without local state
still fails closed. No durable storage, automatic replay/reconnect, cross-process
recovery or TLS/Dial/Auth Inbox factory is provided by this phase.

Runnable example: [two Engines](../examples/inbox/README.md). Tests cover both
MQTT versions, incoming QoS0/1/2, FIFO across three Engines, independent result
retention, count/byte queue overflow across reconnect, physical object limits,
exclusive attachment and allocation-budget constructor failures.

Quota accounting covers requested result/message box allocations and their owned
wire/properties/resolved topics. Core Future/error objects, allocator overhead,
native queues and socket/kernel buffers remain outside this box quota and use
their existing Core/caller limits. Shared gauges are not total process memory.
