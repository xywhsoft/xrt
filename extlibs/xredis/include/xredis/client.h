#ifndef XREDIS_CLIENT_H
#define XREDIS_CLIENT_H
#include <xredis/features.h>
#include <xrt/features.h>
#include <xredis/command.h>
#include <xredis/reply.h>
#include <xrt/tcp.h>
#include <xrt/future.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT) && \
	(!defined(XREDIS_FEATURE_XREDIS_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_REPLY) || \
	 !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_FUTURE))
#error "XREDIS_FEATURE_XREDIS_CLIENT requires command, reply, net_tcp and future features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
typedef struct xredisclient xredisclient;

typedef enum xredisclientstate {
	XREDIS_CLIENT_CONNECTING = 0, XREDIS_CLIENT_OPEN, XREDIS_CLIENT_CLOSING, XREDIS_CLIENT_CLOSED
} xredisclientstate;

typedef enum xredisrequeststatus {
	XREDIS_REQUEST_OK = 0, XREDIS_REQUEST_SERVER_ERROR, XREDIS_REQUEST_TIMEOUT,
	XREDIS_REQUEST_CANCELLED, XREDIS_REQUEST_CLOSED, XREDIS_REQUEST_DISCONNECTED,
	XREDIS_REQUEST_PROTOCOL, XREDIS_REQUEST_LIMIT, XREDIS_REQUEST_MEMORY
} xredisrequeststatus;

typedef enum xredisdelivery {
	XREDIS_NOT_SENT = 0, XREDIS_MAY_HAVE_EXECUTED, XREDIS_REPLIED
} xredisdelivery;

typedef enum xredisadmission {
	XREDIS_ADMISSION_ERROR = -1, XREDIS_ADMISSION_ACCEPTED = 0,
	XREDIS_ADMISSION_OVERLOADED, XREDIS_ADMISSION_CLOSED
} xredisadmission;

typedef enum xredisclienterror {
	XREDIS_CLIENT_ARGUMENT = 3001, XREDIS_CLIENT_STATE, XREDIS_CLIENT_COMMAND_MODE
} xredisclienterror;

/* Every accepted call resolves its Future with this immutable typed outcome,
 * including cancellation/timeout/transport failure. Future cancellation asks
 * the producer to stop; XFUTURE_RESOLVED itself does not imply request success.
 * Reply (OK/SERVER_ERROR) and optional explicit Cause are borrowed from that
 * Future. Retain a reply before dropping its Future. Status/Delivery remain
 * exact even if error diagnostics cannot allocate. Delivery states knowledge
 * at logical completion: accepting bytes into TCP is MAY_HAVE_EXECUTED, not
 * proof that the server received/executed them. No operation is auto-replayed. */
typedef struct xredisrequestresult {
	xredisrequeststatus Status;
	xredisdelivery Delivery;
	xredisreply* Reply;
	const xerror* Cause;
} xredisrequestresult;

typedef struct xredisclientconfig {
	size_t Size;
	xnetstreamconfig Stream;
	xredisreplyreaderconfig Replies;
	size_t MaxRequests;
	size_t MaxCommandBytes;
	size_t MaxCommandBytesTotal;
	size_t MaxInFlight;
	size_t MaxPushes;
	size_t MaxPushBytes;
	size_t WorkRequests;
	size_t ReadBytes;
	size_t ReadFrames;
	/* Positive grace after a sent request is locally cancelled/timed out.
	 * Keep its FIFO slot until drained; retire the connection if grace expires. */
	int64 DrainTimeout;
} xredisclientconfig;

typedef struct xredisclientinfo {
	size_t Size;
	xredisclientstate State;
	/* Includes concurrent construction and wire/timer/cancel-release tails.
	 * Completed results retained by callers do not retain request/client/engine. */
	size_t Requests;
	size_t CommandBytes;
	size_t Pushes;
	size_t PushBytes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Completed;
	uint64 DiscardedReplies;
	xredisrequeststatus CloseStatus;
} xredisclientinfo;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config);

/* One TCP connection on an existing running XRT engine, fixed worker affinity.
 * Numeric address only. Config is copied; NULL supplies finite defaults.
 * Asynchronous start permits calls while connecting. Result unchanged on
 * failure. No AUTH/HELLO negotiation, DNS, TLS or reconnect is implicit.
 * Connect/Call may finish before their creation function returns. The caller
 * owns one reference and must Close then Release. Engine is pinned until all
 * physical requests/posts retire; the engine outlives these APIs/callbacks.
 * This module handles ordinary one-reply commands. Known PubSub/replication/
 * MONITOR/CLIENT REPLY mode switches are rejected before admission; dedicated
 * modes and unknown module-command cardinality need their own contract. */
XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result);
XREDIS_API bool xredisClientRetain(xredisclient* Client);
XREDIS_API void xredisClientRelease(xredisclient* Client);

/* Thread-safe bounded admission; copies/encodes arguments during this call.
 * Timeout is signed milliseconds (-1 forever, 0 already due), measured from
 * API entry through encoding, submission, setup and processing. Future output
 * changes only on ACCEPTED. OVERLOADED/CLOSED are normal counted admission
 * results, not request outcomes. All outputs are disjoint from input/client.
 * Successful submission does not retain any source argument/storage.
 * Limits include abandoned sent requests and actual retained command buffers.
 * xrtFutureCancel wakes the worker without running request completion inline.
 * Cancellation wins when observed before completion; deadline equality expires.
 * Local completion does not remove a sent request's FIFO/wire obligation.
 * Future watch/continuation notifications execute on XRT completion paths:
 * application callbacks must not block a network worker. */
XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Same operation/deadline, then waits for the typed outcome. Rejects calls
 * from ANY worker of the same engine (including a different affinity), since
 * blocking an engine worker is not a supported sync facade. */
XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Nonblocking ownership transfer of one RESP3 PUSH. Empty returns false and
 * preserves Reply/ambient error. A full count/byte queue retires the connection
 * rather than silently losing pushes or shifting response correlation.
 * Ordinary replies without a sent FIFO slot are protocol errors. */
XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply);
XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info);

/* Idempotent, allocation-free close request. Accepted requests are completed
 * with explicit statuses; physical duties remain alive until TCP closure and
 * data-bearing request/control ownership retires. Closed returns a retained
 * observer of that barrier, resolved with NULL. The current notification tail
 * may still be returning; stop/join the engine before unloading callback code.
 * Closed does not itself request close. */
XREDIS_API bool xredisClientClose(xredisclient* Client);
XREDIS_API xfuture* xredisClientClosed(xredisclient* Client);
XRT_EXTERN_C_END
#endif
#endif
