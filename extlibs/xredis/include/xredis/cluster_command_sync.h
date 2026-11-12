#ifndef XREDIS_CLUSTER_COMMAND_SYNC_H
#define XREDIS_CLUSTER_COMMAND_SYNC_H
#include <xredis/cluster_client_sync.h>
#include <xredis/cluster_command.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC) && (!defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC) || !defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND))
#error "Synchronous metadata routing requires Cluster sync and command routing"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND_SYNC)
XRT_EXTERN_C_BEGIN
/* Blocking CallCommand with the same typed preflight decisions and one original
 * entry deadline through metadata and native work. Catalogue is borrowed only
 * for preflight/admission; the facade retains no catalogue reference. Result is
 * published only on ACCEPTED and also disjoint from Catalogue/retained Reply.
 * All progress/Nodes Engine workers are refused before metadata/admission.
 * Wait/Future ownership, native wait failure and physical retirement follow
 * CallSync. There is no automatic metadata fetch, exotic-command server query,
 * replica inference, independently cancellable wait or new replay policy. */
XREDIS_API xredisclustercommandadmission xredisClusterClientCallCommandSync(
    xredisclusterclient* Client,const xrediscommandcatalog* Catalogue,
    const xbytesview* Arguments,size_t Count,uint16 KeylessSlot,int64 Timeout,
    xredisclusterwait* Result);
XRT_EXTERN_C_END
#endif
#endif
