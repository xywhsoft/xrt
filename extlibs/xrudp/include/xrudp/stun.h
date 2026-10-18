#ifndef XRUDP_STUN_H
#define XRUDP_STUN_H
#include <xrudp/features.h>
#include <xrudp/wire.h>
#if defined(XRUDP_FEATURE_RUDP_STUN)

#define XRUDP_STUN_HEADER_SIZE 20u
#define XRUDP_STUN_TRANSACTION_SIZE 12u
#define XRUDP_STUN_PACKET_MAX 512u
#define XRUDP_STUN_ATTRIBUTES_MAX 64u
#define XRUDP_STUN_UNKNOWN_MAX 16u
#define XRUDP_STUN_BINDING_REQUEST 0x0001u
#define XRUDP_STUN_BINDING_INDICATION 0x0011u
#define XRUDP_STUN_BINDING_SUCCESS 0x0101u
#define XRUDP_STUN_BINDING_ERROR 0x0111u

/* Bounded RFC 8489 Binding wire view; NOT an identity/authentication proof.
 * Unknown-required types are collected uniquely; optional attributes ignored.
 * Integrity only reports presence, it NEVER means its HMAC was verified.
 * Attributes after integrity are ignored except integrity/fingerprint. */
typedef struct xrudpstunmessage {
	xrudpaddress Mapped;
	unsigned char Transaction[XRUDP_STUN_TRANSACTION_SIZE];
	uint16_t Type, ErrorCode, Unknown[XRUDP_STUN_UNKNOWN_MAX];
	uint8_t UnknownCount;
	bool HasMapped, Fingerprint, Integrity;
} xrudpstunmessage;

XRT_EXTERN_C_BEGIN
/* Exact UDP framing, no allocation/I/O/randomness. FINGERPRINT, when present,
 * is checked and must be last. First mapped/error attribute wins. No classic
 * STUN, redirects or credential verification. Failure preserves
 * outputs; encode counters become zero. Input/output buffers may overlap, but
 * counters/views must not alias them. Mapping is only a candidate address. */
XRUDP_API xrudpresult xrtRudpStunDecode(const void* pData, size_t iSize, xrudpstunmessage* pMessage);
XRUDP_API xrudpresult xrtRudpStunRequest(const void* pTransaction, bool bFingerprint,
	void* pOutput, size_t iCapacity, size_t* pWritten);
/* Binding indication: no response, no reachability or identity evidence. Each
 * admitted indication has a fresh transaction; it is not request retry. */
XRUDP_API xrudpresult xrtRudpStunIndication(const void* pTransaction, bool bFingerprint,
	void* pOutput, size_t iCapacity, size_t* pWritten);
/* Mapped must be unicast/nonzero with nonzero port, Zone=0. The reflector
 * separately enforces source routing, request/response and rate budgets. */
XRUDP_API xrudpresult xrtRudpStunResponse(const void* pTransaction, const xrudpaddress* pMapped,
	bool bFingerprint, void* pOutput, size_t iCapacity, size_t* pWritten);
/* Error 300..699. Only 420 takes a nonempty list (<=UNKNOWN_MAX) of required
 * attribute types. No peer-supplied diagnostic text is reflected. */
XRUDP_API xrudpresult xrtRudpStunError(const void* pTransaction, uint16_t iCode,
	const uint16_t* pUnknown, size_t iUnknownCount, bool bFingerprint,
	void* pOutput, size_t iCapacity, size_t* pWritten);
XRT_EXTERN_C_END
#endif
#endif
