#ifndef XRUDP_WIRE_H
#define XRUDP_WIRE_H

#include <xrudp/features.h>
#include <xrudp/export.h>

#if defined(XRUDP_FEATURE_RUDP_WIRE)

#define XRUDP_HEADER_SIZE 48u
#define XRUDP_TAG_SIZE 16u
#define XRUDP_PACKET_MAX 1200u
#define XRUDP_FRAME_HEADER_SIZE 4u
#define XRUDP_STREAM_CHUNK_MAX (XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - XRUDP_FRAME_HEADER_SIZE - 12u)
#define XRUDP_ACK_RANGES_MAX 32u
#define XRUDP_FRAMES_MAX 64u
#define XRUDP_MESSAGE_SIZE_MAX (1024u * 1024u)
#define XRUDP_REPLAY_WORDS 16u
#define XRUDP_REPLAY_BITS (XRUDP_REPLAY_WORDS * 64u)

typedef enum xrudpresult {
	XRUDP_OK = 0,
	XRUDP_DONE,
	XRUDP_ARGUMENT,
	XRUDP_TRUNCATED,
	XRUDP_PROTOCOL_MISMATCH,
	XRUDP_MALFORMED,
	XRUDP_LIMIT,
	XRUDP_DUPLICATE,
	XRUDP_TOO_OLD,
	XRUDP_AUTH_FAILED,
	XRUDP_BACKPRESSURE,
	XRUDP_CANCELLED,
	XRUDP_TIMEOUT,
	XRUDP_CLOSED,
	XRUDP_NO_MEMORY,
	XRUDP_IO_ERROR
} xrudpresult;

/* API address only, never copied onto the wire. Port/Zone are host-order.
 * IPv4 uses four bytes followed by twelve zero bytes and Zone=0; IPv6 uses
 * all sixteen bytes and a local interface scope when needed. */
typedef struct xrudpaddress {
	uint8_t Family; /* 4 or 6. */
	uint16_t Port;
	uint32_t Zone;
	unsigned char Address[16];
} xrudpaddress;

typedef enum xrudppackettype {
	XRUDP_PACKET_HELLO = 1,
	XRUDP_PACKET_RETRY = 2,
	XRUDP_PACKET_HANDSHAKE = 3,
	XRUDP_PACKET_DATA = 4,
	XRUDP_PACKET_PATH = 5 /* Authenticated, path-only control, at most 128 bytes. */
} xrudppackettype;

typedef enum xrudpframetype {
	XRUDP_FRAME_PADDING = 0,
	XRUDP_FRAME_PING = 1,
	XRUDP_FRAME_ACK = 2,
	XRUDP_FRAME_STREAM = 3,
	XRUDP_FRAME_RESET = 4,
	XRUDP_FRAME_MAX_DATA = 5,
	XRUDP_FRAME_MAX_STREAM_DATA = 6,
	XRUDP_FRAME_MAX_STREAMS = 7,
	XRUDP_FRAME_DATAGRAM = 8,
	XRUDP_FRAME_CLOSE = 9,
	XRUDP_FRAME_PATH_CHALLENGE = 10,
	XRUDP_FRAME_PATH_RESPONSE = 11,
	XRUDP_FRAME_KEY_CONFIRM = 12,
	XRUDP_FRAME_OPEN = 13,
	XRUDP_FRAME_STOP_SENDING = 14,
	XRUDP_FRAME_SHUTDOWN = 15,
	XRUDP_FRAME_DRAINED = 16
} xrudpframetype;

#define XRUDP_STREAM_FIN 1u
#define XRUDP_DATAGRAM_SEQUENCED 1u

/* Wire integers are big-endian; no C structure is copied onto the wire.
 * PayloadSize includes the AEAD tag for DATA/PATH; other types use epoch 0.
 * ConnectionId is an opaque nonzero destination/rendezvous identifier, not auth.
 * The codec checks structure only. It never authenticates or authorizes a peer. */
typedef struct xrudpwireheader {
	xrudppackettype Type;
	uint16_t PayloadSize;
	uint32_t KeyEpoch;
	unsigned char ConnectionId[16];
	uint64_t PacketNumber;
} xrudpwireheader;

typedef struct xrudpwirepacket {
	xrudpwireheader Header;
	const unsigned char* Payload;
} xrudpwirepacket;

typedef struct xrudpwireframe {
	xrudpframetype Type;
	uint8_t Flags;
	uint16_t Size;
	const unsigned char* Data;
} xrudpwireframe;

/* Fixed-size replay state. Initialize before use; each receive direction owns
 * its own window. Do not commit a packet until authentication has succeeded. */
typedef struct xrudpreplay {
	uint64_t Largest;
	uint64_t Bits[XRUDP_REPLAY_WORDS];
} xrudpreplay;

XRT_EXTERN_C_BEGIN

/* No allocation, logging, I/O or state changes. Packet/frame views borrow input.
 * On failure outputs/cursors and destination bytes are unchanged, except that
 * an Encode/Append written count is set to zero. Overlapping output/input is
 * allowed, but header/frame structures must not overlap the destination. */
XRUDP_API xrudpresult xrtRudpPacketDecode(const void* pData, size_t iSize,
	xrudpwirepacket* pPacket);
XRUDP_API xrudpresult xrtRudpPacketEncode(void* pOutput, size_t iCapacity,
	size_t* pWritten, const xrudpwireheader* pHeader,
	const void* pPayload, size_t iPayloadSize);
XRUDP_API xrudpresult xrtRudpFrameNext(const void* pData, size_t iSize,
	size_t* pOffset, xrudpwireframe* pFrame);
XRUDP_API xrudpresult xrtRudpFrameAppend(void* pOutput, size_t iCapacity,
	size_t* pWritten, xrudpframetype iType, uint8_t iFlags,
	const void* pData, size_t iSize);
/* Validate an entire decrypted DATA payload before invoking any application
 * callbacks or changing connection state. Structural checks do not replace
 * connection-specific ACK, stream-credit or authenticated-peer validation. */
XRUDP_API xrudpresult xrtRudpFramesValidate(const void* pData, size_t iSize,
	size_t* pCount);
XRUDP_API void xrtRudpReplayInit(xrudpreplay* pWindow);
XRUDP_API xrudpresult xrtRudpReplayCheck(const xrudpreplay* pWindow,
	uint64_t iPacketNumber);
XRUDP_API xrudpresult xrtRudpReplayAccept(xrudpreplay* pWindow,
	uint64_t iPacketNumber);

XRT_EXTERN_C_END

#endif
#endif
