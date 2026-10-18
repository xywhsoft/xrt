#ifndef XRUDP_DATAGRAM_H
#define XRUDP_DATAGRAM_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

#if defined(XRUDP_FEATURE_RUDP_DATAGRAM)

#define XRUDP_DATAGRAM_CHANNELS_MAX 64u
#define XRUDP_DATAGRAM_QUEUE_MAX 256u
#define XRUDP_DATAGRAM_OVERHEAD (XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + 16u + 8u)
#define XRUDP_DATAGRAM_PAYLOAD_MAX (XRUDP_PACKET_MAX - XRUDP_DATAGRAM_OVERHEAD)
#define XRUDP_DATAGRAM_LIFETIME_MAX_US UINT64_C(600000000)
#define XRUDP_DATAGRAM_INDEPENDENT 0u
#define XRUDP_DATAGRAM_ORDERED 1u
#define XRUDP_DATAGRAM_LATEST 2u

typedef struct xrudpdatagrams xrudpdatagrams;
typedef struct xrudpdatagramconfig {
	uint32_t Channels, SendCapacity, ReceiveCapacity, PayloadCapacity;
} xrudpdatagramconfig;
typedef struct xrudpdatagramchannel {
	uint32_t Id;
	uint8_t Mode, Priority; /* INDEPENDENT/ORDERED/LATEST; priority 0..3. */
	uint16_t Weight; /* 1..16, bounded weighted deficit scheduling. */
	bool ReplacePending; /* Replace the one unsent record; never in-flight data. */
	uint64_t ReceiveLifetimeUs; /* Local arrival-time lifetime; 0 disables expiry. */
} xrudpdatagramchannel;
typedef struct xrudpdatagraminfo {
	uint32_t Channel, Size;
	uint64_t Sequence, Ticket, AtUs, DeadlineUs;
	bool Sequenced;
} xrudpdatagraminfo;
typedef struct xrudpdatagramview {
	xrudpdatagraminfo Info;
	const void* Data; /* Borrowed only until the next mutation of this pool. */
} xrudpdatagramview;
typedef struct xrudpdatagramstate {
	xrudpdatagramconfig Config;
	uint32_t Channels, SendQueued, ReceiveQueued, SendBytes, ReceiveBytes;
	uint64_t Sent, Received, Accepted, Read;
	uint64_t SendExpired, ReceiveExpired, SendReplaced, ReceiveReplaced;
	uint64_t DroppedUnknown, DroppedMode, DroppedStale, DroppedCapacity;
	uint64_t LastNowUs, NextDeadlineUs;
	uint64_t SendCancelled, ReceiveCancelled;
	uint64_t SendSizeDropped;
	bool Stopped;
} xrudpdatagramstate;
typedef struct xrudpdatagramchannelstate {
	xrudpdatagramchannel Config;
	uint64_t SendSequence, ReceiveSequence;
	uint32_t SendQueued, ReceiveQueued, Deficit;
} xrudpdatagramchannelstate;

XRT_EXTERN_C_BEGIN

/* Fixed-memory datagram queues, no reliability/recovery/cipher of their own.
 * Create allocates once; all other operations allocate nothing. One serialized
 * owner; caller buffers must be disjoint from the pool and all output objects.
 * ConfigInit selects 16 channels, 32 records each direction, 1112B payload.
 * Channel IDs/modes are agreed by the application, not implicitly opened by
 * incoming traffic. Configured IDs live until Destroy, so stale sequences
 * cannot resurrect after remove/reopen. Unknown channels are dropped. */
XRUDP_API void xrtRudpDatagramConfigInit(xrudpdatagramconfig* pConfig);
XRUDP_API void xrtRudpDatagramChannelInit(xrudpdatagramchannel* pChannel);
XRUDP_API xrudpresult xrtRudpDatagramAllocationSize(const xrudpdatagramconfig* pConfig, size_t* pSize);
XRUDP_API xrudpresult xrtRudpDatagramCreate(const xrudpdatagramconfig* pConfig, xrudpdatagrams** ppDatagrams);
XRUDP_API void xrtRudpDatagramDestroy(xrudpdatagrams* pDatagrams);
/* Discard and clear all pending plaintext, reject future mutable operations,
 * preserve the immutable statistics/channel history. Repeated Abort is DONE. */
XRUDP_API xrudpresult xrtRudpDatagramAbort(xrudpdatagrams* pDatagrams);
/* Same configuration is DUPLICATE; changing an existing ID is ARGUMENT.
 * Modes ORDERED and LATEST use the wire sequence flag. ORDERED queues newer
 * arrivals in increasing sequence, skipping gaps without waiting/retransmission;
 * LATEST replaces an unread accepted snapshot on the same channel.
 * There is no ordering promise between different channels. */
XRUDP_API xrudpresult xrtRudpDatagramConfigure(xrudpdatagrams* pDatagrams, const xrudpdatagramchannel* pChannel);
XRUDP_API xrudpresult xrtRudpDatagramChannelState(const xrudpdatagrams* pDatagrams,
	uint32_t iChannel, xrudpdatagramchannelstate* pState);
XRUDP_API xrudpresult xrtRudpDatagramState(const xrudpdatagrams* pDatagrams, xrudpdatagramstate* pState);
/* Local monotonic microseconds, never a peer wall clock. Deadline=0 means no
 * local expiry; otherwise >Now and at most 10min ahead. Empty datagrams are
 * valid. Temporary full queues return BACKPRESSURE without spending sequence
 * or ticket; oversized data is LIMIT. Replacement clears old plaintext.
 * Failed writes leave Sequence unchanged; clock-driven expiry may still remove
 * other records. Counters/tickets never wrap into an old identity. */
XRUDP_API xrudpresult xrtRudpDatagramWrite(xrudpdatagrams* pDatagrams, uint32_t iChannel,
	const void* pData, size_t iSize, uint64_t iNowUs, uint64_t iDeadlineUs, uint64_t* pSequence);
XRUDP_API xrudpresult xrtRudpDatagramMaintain(xrudpdatagrams* pDatagrams, uint64_t iNowUs);
/* Peek is pure. Sent removes exactly the matching immutable ticket after real
 * UDP admission; failed admission must not call Sent. WireSize includes every
 * protected packet byte and debits bounded weighted service. Loss never puts
 * this record back in the queue: packet ACK/recovery belongs to the session. */
XRUDP_API xrudpresult xrtRudpDatagramPeekSend(const xrudpdatagrams* pDatagrams, xrudpdatagramview* pView);
XRUDP_API xrudpresult xrtRudpDatagramSent(xrudpdatagrams* pDatagrams, uint64_t iTicket, uint32_t iWireSize);
/* Drop/clear unsent records larger than MaximumPayload after a path shrinks.
 * MaximumPayload=0 keeps empty records; max is DATAGRAM_PAYLOAD_MAX. One
 * bounded queue scan, no allocation/time/sequence/ticket reuse. Keeps FIFO and
 * service history of surviving records and all receive data. Count=0 on error.
 * Does not change the configured payload budget or enforce future Write size;
 * the transport enforces its current send limit separately. */
XRUDP_API xrudpresult xrtRudpDatagramTrimSend(xrudpdatagrams* pDatagrams,
	uint32_t iMaximumPayload, uint32_t* pCount);
/* Accept one structurally valid, AUTHENTICATED frame after packet-wide
 * preflight. Valid unreliable traffic always returns OK, even on policy drop:
 * unknown/mismatched channel, old sequence, oversize or full receive queue.
 * A newer sequenced frame advances the high-water mark even when dropped for
 * capacity; an older delayed snapshot must never resurrect after a later one.
 * Receive lifetimes start at this local arrival time, not at sender time. */
XRUDP_API xrudpresult xrtRudpDatagramInput(xrudpdatagrams* pDatagrams, uint32_t iChannel,
	bool bSequenced, uint64_t iSequence, const void* pData, size_t iSize, uint64_t iNowUs);
/* Peek is pure and reads the current maintained queue. Read first maintains at
 * Now, then copies/consumes one whole payload. TRUNCATED returns required Info
 * without writing output or consuming a record; NULL/0 consumes empty records.
 * Info is zero on other failures. No allocation on any processing path. */
XRUDP_API xrudpresult xrtRudpDatagramPeek(const xrudpdatagrams* pDatagrams, xrudpdatagraminfo* pInfo);
XRUDP_API xrudpresult xrtRudpDatagramRead(xrudpdatagrams* pDatagrams, void* pOutput,
	size_t iCapacity, uint64_t iNowUs, xrudpdatagraminfo* pInfo);

XRT_EXTERN_C_END
#endif
#endif
