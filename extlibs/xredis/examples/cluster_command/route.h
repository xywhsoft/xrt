#ifndef XREDIS_EXAMPLE_CLUSTER_COMMAND_ROUTE_H
#define XREDIS_EXAMPLE_CLUSTER_COMMAND_ROUTE_H

#include "../command_keys/catalogue.h"
#include <xredis/cluster_client.h>
#include <xredis/cluster_client_sync.h>
#include <xrt/time.h>
#include <math.h>

typedef enum xredisclustercommandadmission {
    XREDIS_CLUSTER_COMMAND_ERROR=-1,XREDIS_CLUSTER_COMMAND_ACCEPTED=0,
    XREDIS_CLUSTER_COMMAND_OVERLOADED,XREDIS_CLUSTER_COMMAND_CLOSED,
    XREDIS_CLUSTER_COMMAND_UNKNOWN,XREDIS_CLUSTER_COMMAND_UNSUPPORTED,
    XREDIS_CLUSTER_COMMAND_CROSS_SLOT,XREDIS_CLUSTER_COMMAND_ARGUMENTS,
    XREDIS_CLUSTER_COMMAND_LIMIT,XREDIS_CLUSTER_COMMAND_KEYLESS
} xredisclustercommandadmission;

#define XREDIS_CLUSTER_COMMAND_NO_SLOT UINT16_MAX

/* The APP supplies a finite timeout and charges catalogue evaluation against
 * it. Round down to the native millisecond budget. Public calls still own
 * admission, redirects, wait guards and connection retirement. */
static int64 appClusterCommandRemaining(double Deadline)
{
    double Left = floor((Deadline - xrtTimer()) * 1000.0);
    return Left > 0 ? (int64)Left : 0;
}

static xredisclustercommandadmission appClusterCommandSlot(const xrediscommandcatalog* Catalogue,
    const xbytesview* Args, size_t Count, uint16 Keyless, int64 Timeout, const void* Out,
    size_t OutputBytes, uint16* Slot)
{
    if (!xredisKeysOutput(Catalogue, Out, OutputBytes) ||
        (Keyless >= XREDIS_CLUSTER_SLOTS && Keyless != XREDIS_CLUSTER_COMMAND_NO_SLOT) ||
        Timeout < 0 || Timeout > 86400000)
        return xredisKeysError("invalid APP command arguments"), XREDIS_CLUSTER_COMMAND_ERROR;
    if (Count > Catalogue->Limits.MaxArguments) return XREDIS_CLUSTER_COMMAND_LIMIT;
    if (!xrtMemRangeValid(Args, Count * sizeof(*Args)) ||
        xrtMemRangesOverlap(Args, Count * sizeof(*Args), Out, OutputBytes))
        return xredisKeysError("APP output aliases command descriptors"), XREDIS_CLUSTER_COMMAND_ERROR;
    for (size_t Index = 0; Index < Count; ++Index)
        if (xrtMemRangesOverlap(Args[Index].Data, Args[Index].Size, Out, OutputBytes))
            return xredisKeysError("APP output aliases command bytes"), XREDIS_CLUSTER_COMMAND_ERROR;
    xrediscommandkeyresult Keys = {.Size = sizeof(Keys)};
    switch (xredisCommandCatalogEvaluate(Catalogue, Args, Count, NULL, 0, &Keys)) {
    case XREDIS_KEYS_OK: *Slot = Keys.Slot; return XREDIS_CLUSTER_COMMAND_ACCEPTED;
    case XREDIS_KEYS_NONE:
        if (Keyless == XREDIS_CLUSTER_COMMAND_NO_SLOT) return XREDIS_CLUSTER_COMMAND_KEYLESS;
        *Slot = Keyless;
        return XREDIS_CLUSTER_COMMAND_ACCEPTED;
    case XREDIS_KEYS_UNKNOWN: return XREDIS_CLUSTER_COMMAND_UNKNOWN;
    case XREDIS_KEYS_UNSUPPORTED: return XREDIS_CLUSTER_COMMAND_UNSUPPORTED;
    case XREDIS_KEYS_CROSS_SLOT: return XREDIS_CLUSTER_COMMAND_CROSS_SLOT;
    case XREDIS_KEYS_ARGUMENTS: return XREDIS_CLUSTER_COMMAND_ARGUMENTS;
    case XREDIS_KEYS_LIMIT: return XREDIS_CLUSTER_COMMAND_LIMIT;
    default: return XREDIS_CLUSTER_COMMAND_ERROR;
    }
}

static xredisclustercommandadmission appClusterCommandAdmission(xredisadmission Status)
{
    switch (Status) {
    case XREDIS_ADMISSION_ACCEPTED: return XREDIS_CLUSTER_COMMAND_ACCEPTED;
    case XREDIS_ADMISSION_OVERLOADED: return XREDIS_CLUSTER_COMMAND_OVERLOADED;
    case XREDIS_ADMISSION_CLOSED: return XREDIS_CLUSTER_COMMAND_CLOSED;
    default: return XREDIS_CLUSTER_COMMAND_ERROR;
    }
}

static inline xredisclustercommandadmission appClusterCallCommand(xredisclusterclient* Client,
    const xrediscommandcatalog* Catalogue, const xbytesview* Args, size_t Count,
    uint16 Keyless, int64 Timeout, xfuture** Future)
{
    double Deadline = xrtTimer() + (double)Timeout * .001;
    uint16 Slot;
    xredisclustercommandadmission Selected = appClusterCommandSlot(Catalogue, Args, Count, Keyless,
        Timeout, Future, sizeof(*Future), &Slot);
    if (Selected != XREDIS_CLUSTER_COMMAND_ACCEPTED) return Selected;
    return appClusterCommandAdmission(xredisClusterClientCall(Client, Slot, Args, Count,
        appClusterCommandRemaining(Deadline), Future));
}

#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC)
static inline xredisclustercommandadmission appClusterCallCommandSync(xredisclusterclient* Client,
    const xrediscommandcatalog* Catalogue, const xbytesview* Args, size_t Count,
    uint16 Keyless, int64 Timeout, xredisclusterwait* Wait)
{
    double Deadline = xrtTimer() + (double)Timeout * .001;
    uint16 Slot;
    xredisclustercommandadmission Selected = appClusterCommandSlot(Catalogue, Args, Count, Keyless,
        Timeout, Wait, sizeof(*Wait), &Slot);
    if (Selected != XREDIS_CLUSTER_COMMAND_ACCEPTED) return Selected;
    return appClusterCommandAdmission(xredisClusterClientCallSync(Client, Slot, Args, Count,
        appClusterCommandRemaining(Deadline), Wait));
}
#endif
#endif
