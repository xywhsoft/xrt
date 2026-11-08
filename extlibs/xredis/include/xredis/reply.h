#ifndef XREDIS_REPLY_H
#define XREDIS_REPLY_H

#include <xredis/features.h>
#include <xrt/features.h>
#include <xredis/reader.h>

#if defined(XREDIS_FEATURE_XREDIS_REPLY) && \
	(!defined(XREDIS_FEATURE_XREDIS_READER) || !defined(XRT_FEATURE_ATOMIC) || !defined(XRT_FEATURE_NUMBER_FLOAT))
#error "XREDIS_FEATURE_XREDIS_REPLY requires reader, atomic and number_float features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreply xredisreply;
typedef struct xredisreplyreader xredisreplyreader;

#define XREDIS_REPLY_NONE SIZE_MAX

typedef enum xredisreplyerror {
	XREDIS_REPLY_OK = 0,
	XREDIS_REPLY_ARGUMENT = 2001,
	XREDIS_REPLY_STATE,
	XREDIS_REPLY_PROTOCOL,
	XREDIS_REPLY_LIMIT,
	XREDIS_REPLY_MEMORY,
	XREDIS_REPLY_TRUNCATED
} xredisreplyerror;

typedef enum xredisreplystatus {
	XREDIS_REPLY_ERROR = -1,
	XREDIS_REPLY_MORE = 0,
	XREDIS_REPLY_READY,
	XREDIS_REPLY_YIELD
} xredisreplystatus;

/* Indices belong to one immutable reply. NONE ends every chain. ChildCount
 * counts actual children (twice Length for maps/attributes); metadata maps do
 * not consume children. Attributes is an ordered chain of attribute nodes,
 * using Next just like the ordinary child chain. Preserve duplicate map keys,
 * wire order, null kinds and streamed flags. Data contains owned raw scalar
 * payload, including numeric lexemes and verbatim fmt:, without framing or
 * a trailing NUL. Integer/Boolean are native values of those types only. */
typedef struct xredisreplyvalue {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
	size_t ChildCount;
	size_t FirstChild;
	size_t Next;
	size_t Attributes;
} xredisreplyvalue;

typedef struct xredisreplyinfo {
	size_t Size;
	size_t Root;
	size_t Nodes;
	size_t PayloadBytes;
	/* Requested XRT bytes, including header/inline storage and unused vector
	 * capacity. Peak also includes old+new buffers during geometric growth.
	 * Allocator headers, size-class rounding and diagnostics are excluded. */
	size_t ReservedBytes;
	size_t PeakBytes;
} xredisreplyinfo;

typedef struct xredisreplyreaderconfig {
	size_t Size;
	xredisreaderconfig Reader;
	/* Per-frame peak requested owned bytes, including temporary growth copies.
	 * Does not include reader/control stacks or already transferred replies.
	 * Zero is invalid. Small limits can reject even an empty scalar. */
	size_t MaxOwnedBytes;
} xredisreplyreaderconfig;

typedef struct xredisreplyreaderinfo {
	size_t Size;
	xredisreaderinfo Reader;
	xredisreplyerror Error;
	size_t PartialNodes;
	size_t PartialPayloadBytes;
	size_t PartialReservedBytes;
	/* Current/most recently completed or failed frame; Reset clears it. */
	size_t PeakOwnedBytes;
} xredisreplyreaderinfo;

XRT_EXTERN_C_BEGIN

/* Default event-reader limits, plus 64 MiB peak owned bytes per frame. */
XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config);

/* Config is copied; NULL selects defaults. Two XRT allocations hold control
 * and bounded depth stacks. Result is unchanged on failure. Creation may
 * report the nested Reader's validation errors. All reader access is confined
 * to one thread. No user callback or input pointer is retained. */
XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config,
	xredisreplyreader** Result);
XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info);

/* Consume at most Reader.MaxFeedBytes and publish ONE fully validated semantic
 * frame on READY, transferring one strong reference. Prior attributes belong
 * to that root, including pushes. Result is unchanged on all other outcomes.
 * Resume YIELD with the unconsumed suffix; MORE needs more input. No retained
 * source bytes, per-node allocation, second parse or final flatten/copy.
 * Scalar bytes are copied into owned storage; vector growth can copy them
 * again. One small scalar needs one allocation. All resource/protocol/OOM/EOF
 * errors are terminal until Reset, release partial ownership immediately and
 * keep their typed cause in Info even if diagnostics cannot allocate.
 * Invalid API calls leave state/Consumed/Result unchanged. Otherwise Consumed
 * is always written, including ERROR. All outputs must be disjoint from each
 * other, Input and reader/partial-frame storage. Successful calls preserve
 * the ambient error. Actual readable objects are required, not just spans. */
XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result);
XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader);

/* Immutable replies may be shared across threads after a synchronized handoff.
 * Each user must already own a live reference when retaining/accessing it.
 * Retain uses a bounded 32-bit atomic count; overflow leaves it unchanged.
 * Release(NULL) is allowed. Final release frees all owned storage on its thread.
 * Refcounting does not publish a pointer or make a concurrent final release
 * safe without a reference. Views remain valid until that last release. */
XREDIS_API bool xredisReplyRetain(xredisreply* Reply);
XREDIS_API void xredisReplyRelease(xredisreply* Reply);
XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info);
XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value);

/* Lazy, locale-independent correctly rounded binary64 conversion of a DOUBLE
 * node. Raw text stays lossless; legacy NaN payloads normalize to signed NaN.
 * Finite overflow reports the public XRT number error and preserves Value;
 * it does not retroactively invalidate a syntactically valid wire reply.
 * Successful conversion allocates nothing and preserves the ambient error. */
XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value);

XRT_EXTERN_C_END

#endif
#endif
