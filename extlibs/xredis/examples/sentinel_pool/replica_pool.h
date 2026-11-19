#ifndef XREDIS_EXAMPLE_SENTINEL_REPLICA_POOL_H
#define XREDIS_EXAMPLE_SENTINEL_REPLICA_POOL_H

#include "../sentinel/replica_policy.h"
#include <xredis/pool.h>

/* Prepare one replacement generation on the application owner. Close the old
 * generation before directing new leases to this pool. Active calls stay owned
 * by their original native connections. */
static bool appReplicaPoolOpen(xnetengine* Engine, const xredissentinelconfig* Discovery,
    const xredispoolconfig* Config, size_t Count, const appreplicapick* Previous, appreplicapick* Pick, xredispool** Output)
{
    xredisclient* Clients[8] = {0};
    if (!Count || Count > sizeof(Clients) / sizeof(Clients[0])) return false;

    double Deadline = xrtTimer() + Discovery->Timeout * .001;
    xredissentinelendpoint Preferred = {0};
    if (Previous) Preferred = (xredissentinelendpoint){{Previous->Host, Previous->HostBytes}, Previous->Port};
    if (!appReplicaPick(Engine, Discovery, 16, Preferred, Deadline, Pick, &Clients[0])) return false;

    size_t Ready = 1;
    xredissentinelendpoint Endpoint = {{Pick->Host, Pick->HostBytes}, Pick->Port};
    while (Ready < Count && appReplicaRemaining(Deadline)) {
        bool IsMaster = false;
        if (appReplicaTry(Engine, Discovery, Deadline, Deadline, Endpoint,
            (xredissentinelendpoint){0}, &IsMaster, &Clients[Ready]) != XREDIS_SENTINEL_OK) break;
        ++Ready;
    }
    xredisadmission Granted = XREDIS_ADMISSION_ERROR;
    if (Ready == Count) {
        do {
            Granted = xredisPoolCreate(Engine, 0, Clients, Count, Config, Output);
            if (Granted == XREDIS_ADMISSION_OVERLOADED) xrtSleep(1);
        } while (Granted == XREDIS_ADMISSION_OVERLOADED && appReplicaRemaining(Deadline));
    }
    for (size_t I = 0; I < Ready; ++I) {
        if (Granted == XREDIS_ADMISSION_ACCEPTED) xredisClientRelease(Clients[I]);
        else (void)appReplicaClose(Clients[I], Deadline);
    }
    return Granted == XREDIS_ADMISSION_ACCEPTED;
}
#endif
