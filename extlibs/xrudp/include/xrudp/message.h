#ifndef XRUDP_MESSAGE_H
#define XRUDP_MESSAGE_H

#include <xrudp/features.h>
#include <xrudp/transmit.h>

#if defined(XRUDP_FEATURE_RUDP_MESSAGE)

typedef struct xrudpmessage xrudpmessage;
typedef struct xrudpmessagestate {
	xrudpresult Reason;
	uint32_t Maximum, Expected, Buffered, HeaderBytes;
	bool Ready, Finished;
} xrudpmessagestate;

XRT_EXTERN_C_BEGIN

/* Ordered reliable-message decoder: u32 big-endian payload length. Owns one
 * fixed Maximum-byte staging area, not a second reliability/recovery core.
 * Maximum is explicit, 0..1MiB (zero accepts empty messages only). Reserve
 * aggregate memory BEFORE Create; only Create allocates. One serialized owner,
 * no callbacks, and all caller buffers/outputs must be disjoint from the object.
 * The session adapter must release byte-stream flow credit as Push consumes
 * bytes, and stop pulling when one complete message is ready. This staging
 * allows partial records on multiple streams to progress without reserving a
 * whole record in shared connection credit. It remains bounded when unread. */
XRUDP_API xrudpresult xrtRudpMessageAllocationSize(uint32_t iMaximum, size_t* pSize);
XRUDP_API xrudpresult xrtRudpMessageCreate(uint32_t iMaximum, xrudpmessage** ppMessage);
XRUDP_API void xrtRudpMessageDestroy(xrudpmessage* pMessage);
XRUDP_API xrudpresult xrtRudpMessageState(const xrudpmessage* pMessage, xrudpmessagestate* pState);
/* Copies only through the end of the next record. *pConsumed is the exact
 * prefix taken; caller retains any suffix. OK may consume an incomplete
 * header/body. BACKPRESSURE means a complete unread record already exists.
 * Oversized wire length is sticky MALFORMED and clears retained plaintext;
 * consumed still reports the header prefix taken before that error.
 * No allocation on success, malformed input, backpressure, EOF or reset. */
XRUDP_API xrudpresult xrtRudpMessagePush(xrudpmessage* pMessage,
	const void* pData, size_t iSize, size_t* pConsumed);
/* Pure query. OK includes a complete empty record; BACKPRESSURE is incomplete,
 * DONE is consumed clean EOF. Output size is zero on other results. */
XRUDP_API xrudpresult xrtRudpMessagePeek(const xrudpmessage* pMessage, size_t* pSize);
/* Copies exactly one complete payload and clears its staging storage.
 * TRUNCATED reports the required payload size without consuming/writing any
 * bytes. NULL/0 may query a nonempty record or consume an empty one. */
XRUDP_API xrudpresult xrtRudpMessageRead(xrudpmessage* pMessage,
	void* pOutput, size_t iCapacity, size_t* pSize);
/* FIN is valid only on a record boundary or after a complete unread record;
 * a partial length/body at FIN is sticky MALFORMED, with no partial delivery. */
XRUDP_API xrudpresult xrtRudpMessageFinish(xrudpmessage* pMessage);
/* Local/peer reset discards retained plaintext and makes future reads CANCELLED.
 * A consumed clean EOF cannot become a second cancellation. */
XRUDP_API xrudpresult xrtRudpMessageReset(xrudpmessage* pMessage);
/* Reuse only fresh, failed/reset, or fully consumed clean EOF storage. A live
 * partial or unread complete record returns BACKPRESSURE. No allocation. */
XRUDP_API xrudpresult xrtRudpMessageRestart(xrudpmessage* pMessage);

XRT_EXTERN_C_END
#endif
#endif
