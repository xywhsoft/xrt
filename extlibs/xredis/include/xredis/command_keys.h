#ifndef XREDIS_COMMAND_KEYS_H
#define XREDIS_COMMAND_KEYS_H
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "Command key specifications require immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
typedef struct xrediscommandcatalog xrediscommandcatalog;
typedef enum xrediskeysstatus {
    XREDIS_KEYS_ERROR=-1,XREDIS_KEYS_MALFORMED=-2,XREDIS_KEYS_LIMIT=-3,
    XREDIS_KEYS_MEMORY=-4,XREDIS_KEYS_SERVER_ERROR=-5,
    XREDIS_KEYS_OK=1,XREDIS_KEYS_NONE,XREDIS_KEYS_UNKNOWN,
    XREDIS_KEYS_UNSUPPORTED,XREDIS_KEYS_CROSS_SLOT,XREDIS_KEYS_ARGUMENTS
} xrediskeysstatus;
typedef enum xrediskeyflag {
    XREDIS_KEY_RO=1,XREDIS_KEY_RW=2,XREDIS_KEY_OW=4,XREDIS_KEY_RM=8,
    XREDIS_KEY_ACCESS=16,XREDIS_KEY_UPDATE=32,XREDIS_KEY_INSERT=64,
    XREDIS_KEY_DELETE=128,XREDIS_KEY_NOT_KEY=256,XREDIS_KEY_VARIABLE=512
} xrediskeyflag;
typedef struct xrediscommandkeylimits {
    size_t Size,MaxCommands,MaxSpecifications,MaxNameBytes,MaxDepth,
        MaxReplyNodes,MaxPayloadBytes,MaxOwnedBytes,MaxArguments,MaxArgumentBytes,MaxKeys,
        MaxKeyBytes,MaxSearchWork;
} xrediscommandkeylimits;
typedef struct xrediscommandcataloginfo {
    size_t Size,Commands,Specifications,LookupDepth,StorageBytes,RetainedReplyBytes;
} xrediscommandcataloginfo;
/* Flags are specification possibilities; VARIABLE is not an exact access decision.
 * NOT_KEY still participates in Cluster slot calculation (for example channels).
 * Specification order and repeated argument occurrences are preserved. */
typedef struct xrediscommandkey {size_t Argument;uint32 Flags;} xrediscommandkey;
typedef struct xrediscommandkeyresult {
    size_t Size,Command,Keys;uint16 Slot;bool HasSlot,Legacy,VariableFlags;
} xrediscommandkeyresult;
XRT_EXTERN_C_BEGIN
/* Defaults:8192 commands,32768 specs,256-byte names,16 levels,1M reply nodes,
 * 16MiB metadata payload,64MiB retained storage,65536 arguments/keys,64MiB args
 * and extracted key bytes,1M search inspections/matched bytes per pass.
 * Hard maxima:65535 commands,262144 specs,4096-byte names,16 levels,
 * 1M arguments/keys,1GiB byte caps. Zero is a limit, except Size. */
XREDIS_API bool xredisCommandKeyLimitsInit(xrediscommandkeylimits* Limits);
/* Complete COMMAND/COMMAND INFO collection, including nested subcommands and null
 * unknown entries. RESP3 sets are supported for non-positional collections;
 * command rows remain positional arrays. Retains Reply once; one additional immutable allocation holds
 * commands/specs/name hash. Two passes validate before publication. Legacy six-
 * field entries are supported; movable/incomplete/unknown key rules require a
 * server oracle. Unknown future metadata fields are preserved by the Reply.
 * No I/O. Failure leaves Catalog and ownership unchanged; OK includes empty. */
XREDIS_API xrediskeysstatus xredisCommandCatalogCreate(xredisreply* Reply,
    const xrediscommandkeylimits* Limits,xrediscommandcatalog** Catalog);
XREDIS_API bool xredisCommandCatalogRetain(xrediscommandcatalog* Catalog);
XREDIS_API void xredisCommandCatalogRelease(xrediscommandcatalog* Catalog);
XREDIS_API bool xredisCommandCatalogInfo(const xrediscommandcatalog* Catalog,
    xrediscommandcataloginfo* Info);
/* Arguments include the command and subcommand tokens. ASCII command/keyword
 * matching is case-insensitive; keys are binary and use Redis hash tags.
 * No allocation, lock, refcount, I/O or input retention. Immutable owners can
 * evaluate concurrently. NULL Keys/zero Capacity computes count/slot alone.
 * OK/NONE alone publish Result and optional Keys, atomically after validation;
 * short capacity and every other status leave all outputs unchanged.
 * UNKNOWN means absent metadata; UNSUPPORTED means full extraction needs
 * COMMAND GETKEYS/GETKEYSANDFLAGS (including incomplete/unknown specs/flags).
 * Neither may be treated as NONE or routed using a guessed first argument.
 * CROSS_SLOT checks every extracted routing key, including NOT_KEY. ARGUMENTS
 * checks arity and extraction indices/counts, not the command's entire grammar.
 * Flags may overapproximate access; they do not authorize replica routing.
 * All outputs must be disjoint from catalog/reply, descriptors and input bytes.
 * Successful and typed non-API outcomes preserve the ambient error. */
XREDIS_API xrediskeysstatus xredisCommandCatalogEvaluate(const xrediscommandcatalog* Catalog,
    const xbytesview* Arguments,size_t Count,xrediscommandkey* Keys,size_t Capacity,
    xrediscommandkeyresult* Result);
XRT_EXTERN_C_END
#endif
#endif
