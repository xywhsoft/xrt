#ifndef XRUDP_ENDPOINT_H
#define XRUDP_ENDPOINT_H

#include <xrudp/features.h>
#include <xrudp/session.h>
#include <xrt/udp.h>

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT)

#define XRUDP_ENDPOINT_PEERS_MAX 256u
#define XRUDP_ENDPOINT_ADDRESS_BUCKETS 64u
#define XRUDP_PEER_READ_READY 1u
#define XRUDP_PEER_WRITE_READY 2u
#define XRUDP_PEER_WRITE_DONE 4u
#define XRUDP_PEER_READ_EOF 8u

typedef struct xrudpendpoint xrudpendpoint;
typedef struct xrudppeer xrudppeer;
typedef enum xrudpendpointstate {
	XRUDP_ENDPOINT_OPENING = 0,
	XRUDP_ENDPOINT_OPEN,
	XRUDP_ENDPOINT_CLOSING,
	XRUDP_ENDPOINT_CLOSED
} xrudpendpointstate;
typedef enum xrudppeerstate {
	XRUDP_PEER_NEGOTIATING = 0,
	XRUDP_PEER_CONFIRMED,
	XRUDP_PEER_CLOSED
} xrudppeerstate;

/* All callbacks run on the bound Worker and borrow their arguments. Ref a peer
 * to keep its handle beyond the callback. Confirmed means identity/address/key
 * proof AND default reliable session admission have completed.
 * Callbacks may close/ref/destroy owned handles, but must not block the Worker.
 * Context remains borrowed until endpoint Close returns. */
typedef struct xrudpendpointevents {
	void (*Open)(xrudpendpoint* pEndpoint, void* pContext);
	void (*Confirmed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, void* pContext);
	void (*PeerClose)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, xrudpresult iReason, void* pContext);
	void (*Close)(xrudpendpoint* pEndpoint, xrudpresult iReason, const xerror* pError, void* pContext);
	/* Edge hints; drain Read until BACKPRESSURE/EOF/RESET. WRITE_READY means
	 * ACK returned buffer capacity; WRITE_DONE is all bytes+FIN acknowledged.
	 * Mutations inside a callback defer further Changed delivery to another
	 * worker turn; no recursive Changed callbacks. Borrowed peer and context. */
	void (*Changed)(xrudpendpoint* pEndpoint, xrudppeer* pPeer, uint32_t iEvents, void* pContext);
} xrudpendpointevents;

typedef struct xrudpendpointconfig {
	const void* StaticPrivate; /* 32 bytes, copied; securely erased on Close. */
	const void* Scope; /* Optional nonzero 16 bytes; NULL creates a random scope. */
	const void* PeerPublic; /* Optional listener pin; pin or Trust required when Listen. */
	const void* ApplicationContext; /* Optional exact listener context, copied. */
	xrudpnoisetrustproc Trust;
	void* TrustContext;
	xrudpsessionconfig Session; /* Local is advertised; Peer overwritten after authentication. */
	uint64_t HandshakeTimeoutUs;
	size_t MemoryLimit; /* Peak endpoint/peer/handshake/session reservation bytes.
	                    * XRT engine pools/kernel socket buffers have own limits. */
	uint32_t MaxPeers;
	uint32_t MaxHalfOpen;
	uint32_t HandshakesPerSecond;
	uint32_t AddressHandshakesPerSecond; /* IP (not port); fixed 64-entry table. */
	bool Listen;
} xrudpendpointconfig;

typedef struct xrudpconnectconfig {
	const void* PeerPublic; /* Trusted 32-byte public key, mandatory. */
	const void* Scope; /* 16-byte listener scope from trusted discovery. */
	const void* ApplicationContext; /* Optional 32-byte binding, copied into handshake. */
} xrudpconnectconfig;

typedef struct xrudpendpointinfo {
	xnetaddr Local;
	unsigned char Public[32], Scope[16];
	uint32_t Peers, HalfOpen;
	size_t ReservedBytes, MemoryLimit;
	uint64_t Received, Sent, Invalid, RateDropped, AddressDropped, CapacityDropped, IoErrors;
} xrudpendpointinfo;

XRT_EXTERN_C_BEGIN

XRUDP_API void xrtRudpEndpointConfigInit(xrudpendpointconfig* pConfig);
/* Must be called on a running Engine Worker. Binding and all mutable operations
 * are serialized on that Worker; no private engine/thread is created. Native
 * IPv4/IPv6 are supported as separate endpoints; IPv6 is v6-only.
 * Bind success returns one owned reference plus an internal I/O lifetime.
 * The endpoint owns UDP queues (64 packets/64KiB send; bounded push receive).
 * Context/Open callback cannot run before Bind returns on this Worker. */
XRUDP_API xrudpresult xrtRudpEndpointBind(xnetworker* pWorker, const xnetaddr* pLocal,
	const xrudpendpointconfig* pConfig, const xrudpendpointevents* pEvents,
	void* pContext, xrudpendpoint** ppEndpoint);
XRUDP_API xrudpendpoint* xrtRudpEndpointRef(xrudpendpoint* pEndpoint);
XRUDP_API void xrtRudpEndpointDestroy(xrudpendpoint* pEndpoint);
/* Thread-safe states. Info requires the owner while live; after CLOSED it is
 * an immutable snapshot and may be read even after the Engine is destroyed. */
XRUDP_API xrudpendpointstate xrtRudpEndpointState(const xrudpendpoint* pEndpoint);
XRUDP_API xrudppeerstate xrtRudpPeerState(const xrudppeer* pPeer);
XRUDP_API xrudpresult xrtRudpEndpointInfo(const xrudpendpoint* pEndpoint, xrudpendpointinfo* pInfo);
XRUDP_API xrudpresult xrtRudpPeerInfo(const xrudppeer* pPeer, xrudphandshakeinfo* pInfo, xnetaddr* pAddress);
/* Owner-only; success returns an additional owned peer reference. Account ACL,
 * peer pin and scope must come from trusted application discovery. */
XRUDP_API xrudpresult xrtRudpEndpointConnect(xrudpendpoint* pEndpoint, const xnetaddr* pRemote,
	const xrudpconnectconfig* pConfig, xrudppeer** ppPeer);
XRUDP_API xrudppeer* xrtRudpPeerRef(xrudppeer* pPeer);
XRUDP_API void xrtRudpPeerDestroy(xrudppeer* pPeer);
/* Owner-only nonblocking default stream. Write copies a bounded prefix;
 * success is buffering, not delivery. Finish forbids further writes. Read
 * returns DONE at clean EOF, CANCELLED at peer RESET. Confirmed allows the
 * first write; subsequent capacity/EOF changes arrive as Changed hints. */
XRUDP_API xrudpresult xrtRudpPeerWrite(xrudppeer* pPeer, const void* pData, size_t iSize, size_t* pWritten);
XRUDP_API xrudpresult xrtRudpPeerRead(xrudppeer* pPeer, void* pOutput, size_t iCapacity, size_t* pRead);
XRUDP_API xrudpresult xrtRudpPeerFinish(xrudppeer* pPeer);
/* Owner while live; immutable snapshot after CLOSED, safe after Engine destroy.
 * BACKPRESSURE before admission, CLOSED when no session was ever admitted. */
XRUDP_API xrudpresult xrtRudpPeerSessionState(const xrudppeer* pPeer, xrudpsessionstate* pState);
XRUDP_API xrudpresult xrtRudpPeerAbort(xrudppeer* pPeer);
/* Thread-safe, idempotent request, no allocation. Close callback runs once after
 * peer callbacks and timer cleanup. Dropping a handle alone does not close it.
 * XRT refuses Engine Stop while UDP objects are live: request endpoint Close
 * and await its terminal callback BEFORE Stop. Closed external handles retain
 * no engine resources and may be destroyed after the Engine. */
XRUDP_API xrudpresult xrtRudpEndpointClose(xrudpendpoint* pEndpoint);

XRT_EXTERN_C_END
#endif
#endif
