#ifndef XREDIS_CLUSTER_COMMAND_H
#define XREDIS_CLUSTER_COMMAND_H
#include <xredis/cluster_client.h>
#include <xredis/command_keys.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND) && (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS))
#error "Cluster commands require the asynchronous client and command key catalogue"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
typedef enum xredisclustercommandadmission {
    XREDIS_CLUSTER_COMMAND_ERROR=-1,XREDIS_CLUSTER_COMMAND_ACCEPTED=0,
    XREDIS_CLUSTER_COMMAND_OVERLOADED,XREDIS_CLUSTER_COMMAND_CLOSED,
    XREDIS_CLUSTER_COMMAND_UNKNOWN,XREDIS_CLUSTER_COMMAND_UNSUPPORTED,
    XREDIS_CLUSTER_COMMAND_CROSS_SLOT,XREDIS_CLUSTER_COMMAND_ARGUMENTS,
    XREDIS_CLUSTER_COMMAND_LIMIT,XREDIS_CLUSTER_COMMAND_KEYLESS
} xredisclustercommandadmission;
#define XREDIS_CLUSTER_COMMAND_NO_SLOT UINT16_MAX
XRT_EXTERN_C_BEGIN
/* Nonblocking metadata-checked Call. All routing keys must agree on one slot.
 * KeylessSlot is an explicit0..16383 fallback for NONE; NO_SLOT returns KEYLESS
 * without admission. It is ignored when actual routing keys exist. Unknown,
 * incomplete/future, cross-slot, arity/extraction or catalogue/client argument
 * limit failures return their typed admission without network work. No metadata
 * fetch, server fallback, replica inference or accepted-business replay is added.
 * Preflight refusals leave client admission counters unchanged; native capacity/
 * close decisions use existing counters and map to OVERLOADED/CLOSED/ERROR.
 * Only ACCEPTED publishes one existing xredisclusterclientresult Future.
 * Every other result preserves Future, inputs and ownership. Catalogue and
 * arguments are borrowed only during this function; no catalogue ref is held
 * afterward. Async command data is copied by the existing native Call path.
 * One function-entry monotonic deadline covers metadata/spans/slot validation,
 * allocation, bootstrap, queue/provider waits and redirects. Timeout-1 uses
 * Route.Timeout;0 is due;maximum24h. Metadata itself is a bounded synchronous
 * CPU phase, not an interruptible task; configure catalogue work/byte caps.
 * Future must be disjoint from Client/current map/Catalogue/Reply/input storage.
 * Caller holds stable Client/Catalogue owners throughout the call. Do not wait
 * on the progress Engine's workers. Existing close/native physical duties and
 * retained outcome guarantees are unchanged. Typed preflight refusals preserve
 * ambient error; ERROR reports invalid API/native admission failure as before. */
XREDIS_API xredisclustercommandadmission xredisClusterClientCallCommand(
    xredisclusterclient* Client,const xrediscommandcatalog* Catalogue,
    const xbytesview* Arguments,size_t Count,uint16 KeylessSlot,int64 Timeout,
    xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
