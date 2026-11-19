
#ifndef XREDIS_EXAMPLE_SENTINEL_REPLICA_POLICY_H
#define XREDIS_EXAMPLE_SENTINEL_REPLICA_POLICY_H

#include <xredis/sentinel.h>
#include <math.h>
#include <string.h>

enum { APP_REPLICA_ROWS = 64 };
typedef struct appreplicapick {
    xredissentinelstatus Status;
    size_t Seed, SeedAttempts, Candidates, Stale, Rounds;
    uint16 Port;
    unsigned char Host[256];
    size_t HostBytes;
} appreplicapick;

static int64 appReplicaRemaining(double Deadline)
{
    double Milliseconds = ceil((Deadline - xrtTimer()) * 1000.0);
    return Milliseconds > 0 ? (int64)Milliseconds : 0;
}

static bool appReplicaClose(xredisclient* Client, double Deadline)
{
    xfuture* Closed = xredisClientClosed(Client);
    xredisClientClose(Client);
    bool Retired = Closed && xrtFutureWaitFor(Closed, appReplicaRemaining(Deadline)) == XWAIT_OK;
    xrtFutureDestroy(Closed);
    xredisClientRelease(Client);
    return Retired;
}

static bool appReplicaWord(xbytesview Value, cstr Word)
{
    size_t Size = strlen(Word);
    return Value.Size == Size && !memcmp(Value.Data, Word, Size);
}

static bool appReplicaText(const xredisreplyvalue* Value)
{
    return (Value->Type == XREDIS_REPLY_SIMPLE_STRING || Value->Type == XREDIS_REPLY_BULK_STRING) &&
        !(Value->Flags & XREDIS_REPLY_NULL_VALUE);
}

static bool appReplicaEndpointEqual(xredissentinelendpoint A, xredissentinelendpoint B)
{
    return A.Port == B.Port && A.Host.Size == B.Host.Size && !memcmp(A.Host.Data, B.Host.Data, A.Host.Size);
}

static bool appReplicaRow(const xredisreply* Reply, const xredisreplyvalue* Row, size_t HostLimit,
    xredissentinelendpoint* Endpoint, bool* Candidate, bool* Promoted)
{
    if ((Row->Type != XREDIS_REPLY_ARRAY && Row->Type != XREDIS_REPLY_MAP) ||
        Row->Flags & XREDIS_REPLY_NULL_VALUE || Row->ChildCount % 2 || Row->ChildCount > 256) return false;
    xbytesview Fields[3] = {0};
    const cstr Names[] = {"ip", "port", "flags"};
    unsigned Seen = 0;
    for (size_t Index = Row->FirstChild; Index != XREDIS_REPLY_NONE;) {
        xredisreplyvalue Key, Value;
        if (!xredisReplyGet(Reply, Index, &Key) || !appReplicaText(&Key) ||
            !xredisReplyGet(Reply, Key.Next, &Value)) return false;
        for (size_t Field = 0; Field < 3; ++Field) {
            if (!appReplicaWord(Key.Data, Names[Field])) continue;
            unsigned Bit = 1u << Field;
            if ((Seen & Bit) || !appReplicaText(&Value) || !Value.Data.Size) return false;
            Seen |= Bit;
            Fields[Field] = Value.Data;
            break;
        }
        Index = Value.Next;
    }
    if (Seen != 7 || Fields[0].Size > HostLimit || Fields[1].Size > 5) return false;
    for (size_t I = 0; I < Fields[0].Size; ++I)
        if (Fields[0].Data[I] <= 32 || Fields[0].Data[I] == 127) return false;
    unsigned Port = 0;
    for (size_t I = 0; I < Fields[1].Size; ++I) {
        unsigned Digit = Fields[1].Data[I];
        if (Digit < '0' || Digit > '9') return false;
        Port = Port * 10 + Digit - '0';
    }
    if (!Port || Port > 65535) return false;

    bool Slave = false, Down = false, Promotion = false;
    size_t Start = 0;
    for (size_t I = 0; I <= Fields[2].Size; ++I) {
        if (I < Fields[2].Size && Fields[2].Data[I] != ',') {
            if (Fields[2].Data[I] <= 32 || Fields[2].Data[I] == 127) return false;
            continue;
        }
        if (Start == I) return false;
        xbytesview Flag = {Fields[2].Data + Start, I - Start};
        Slave |= appReplicaWord(Flag, "slave");
        Promotion |= appReplicaWord(Flag, "promoted");
        Down |= appReplicaWord(Flag, "s_down") || appReplicaWord(Flag, "o_down") ||
            appReplicaWord(Flag, "disconnected");
        Start = I + 1;
    }
    *Endpoint = (xredissentinelendpoint){Fields[0], (uint16)Port};
    *Promoted = Slave && Promotion;
    *Candidate = Slave && !Down && !Promotion;
    return true;
}

static xredissentinelstatus appReplicaList(const xredisrequestresult* Result,
    const xredissentinelconfig* Config, size_t Limit, xredissentinelendpoint* Candidates,
    size_t* Count, xredissentinelendpoint* Promoted)
{
    xredisreplyinfo Info = {.Size = sizeof(Info)};
    xredisreplyvalue Root;
    if (!Result->Reply || !xredisReplyInfo(Result->Reply, &Info) ||
        !xredisReplyGet(Result->Reply, Info.Root, &Root)) return XREDIS_SENTINEL_PROTOCOL;
    if (Result->Status == XREDIS_REQUEST_SERVER_ERROR) {
        return Root.Type == XREDIS_REPLY_SIMPLE_ERROR &&
            appReplicaWord(Root.Data, "ERR No such master with that name") ?
            XREDIS_SENTINEL_UNKNOWN_SERVICE : XREDIS_SENTINEL_SERVER_ERROR;
    }
    if (Root.Type == XREDIS_REPLY_NULL || (Root.Flags & XREDIS_REPLY_NULL_VALUE))
        return XREDIS_SENTINEL_UNKNOWN_SERVICE;
    if (Root.Type != XREDIS_REPLY_ARRAY) return XREDIS_SENTINEL_PROTOCOL;
    if (Root.ChildCount > Limit || Info.PayloadBytes > Config->Role.MaxPayloadBytes)
        return XREDIS_SENTINEL_LIMIT;
    size_t HostLimit = Config->MaxHostBytes < sizeof(((appreplicapick*)0)->Host) ?
        Config->MaxHostBytes : sizeof(((appreplicapick*)0)->Host);
    bool Conflict = false;
    for (size_t I = Root.FirstChild; I != XREDIS_REPLY_NONE;) {
        xredisreplyvalue Row;
        xredissentinelendpoint Endpoint;
        bool Candidate, Promotion;
        if (!xredisReplyGet(Result->Reply, I, &Row) ||
            !appReplicaRow(Result->Reply, &Row, HostLimit, &Endpoint, &Candidate, &Promotion))
            return XREDIS_SENTINEL_PROTOCOL;
        if (Promotion) {
            Conflict |= Promoted->Port && !appReplicaEndpointEqual(*Promoted, Endpoint);
            *Promoted = Endpoint;
        } else if (Candidate) Candidates[(*Count)++] = Endpoint;
        I = Row.Next;
    }
    return *Count && !Conflict ? XREDIS_SENTINEL_OK : XREDIS_SENTINEL_UNREACHABLE;
}

static xredissentinelstatus appReplicaCall(xredisclient* Client, xredisprotocol Protocol,
    const xbytesview* Args, size_t Count, double Deadline, xfuture** Request)
{
    xfuture* Ready = xredisClientReady(Client);
    xwaitresult Wait = Ready ? xrtFutureWaitFor(Ready, appReplicaRemaining(Deadline)) : XWAIT_ERROR;
    const xredisreadyresult* Value = Wait == XWAIT_OK ? xrtFutureValue(Ready) : NULL;
    xredissentinelstatus Status = XREDIS_SENTINEL_PROTOCOL;
    if (Wait == XWAIT_TIMEOUT) Status = XREDIS_SENTINEL_TIMEOUT;
    else if (Value) {
        switch (Value->Status) {
        case XREDIS_READY_OK:
            if (Value->Protocol == Protocol) Status = XREDIS_SENTINEL_OK;
            break;
        case XREDIS_READY_TIMEOUT: Status = XREDIS_SENTINEL_TIMEOUT; break;
        case XREDIS_READY_DISCONNECTED:
        case XREDIS_READY_CLOSED: Status = XREDIS_SENTINEL_UNREACHABLE; break;
        case XREDIS_READY_AUTH_FAILED: Status = XREDIS_SENTINEL_AUTH_FAILED; break;
        case XREDIS_READY_SERVER_ERROR: Status = XREDIS_SENTINEL_SERVER_ERROR; break;
        case XREDIS_READY_MEMORY: Status = XREDIS_SENTINEL_MEMORY; break;
        case XREDIS_READY_LIMIT: Status = XREDIS_SENTINEL_LIMIT; break;
        default: break;
        }
    }
    xrtFutureDestroy(Ready);
    if (Status != XREDIS_SENTINEL_OK) return Status;
    int64 Remaining = appReplicaRemaining(Deadline);
    if (!Remaining) return XREDIS_SENTINEL_TIMEOUT;
    xredisadmission Admitted = xredisClientCallAsync(Client, Args, Count, Remaining, Request);
    if (Admitted == XREDIS_ADMISSION_CLOSED) return XREDIS_SENTINEL_UNREACHABLE;
    if (Admitted == XREDIS_ADMISSION_OVERLOADED) return XREDIS_SENTINEL_LIMIT;
    if (Admitted != XREDIS_ADMISSION_ACCEPTED) return XREDIS_SENTINEL_MEMORY;
    if (xrtFutureWaitFor(*Request, appReplicaRemaining(Deadline)) != XWAIT_OK)
        return XREDIS_SENTINEL_TIMEOUT;
    const xredisrequestresult* Result = xrtFutureValue(*Request);
    if (!Result) return XREDIS_SENTINEL_PROTOCOL;
    switch (Result->Status) {
    case XREDIS_REQUEST_OK:
    case XREDIS_REQUEST_SERVER_ERROR: return XREDIS_SENTINEL_OK;
    case XREDIS_REQUEST_TIMEOUT: return XREDIS_SENTINEL_TIMEOUT;
    case XREDIS_REQUEST_CLOSED:
    case XREDIS_REQUEST_DISCONNECTED: return XREDIS_SENTINEL_UNREACHABLE;
    case XREDIS_REQUEST_MEMORY: return XREDIS_SENTINEL_MEMORY;
    case XREDIS_REQUEST_LIMIT: return XREDIS_SENTINEL_LIMIT;
    default: return XREDIS_SENTINEL_PROTOCOL;
    }
}

static xredissentinelstatus appReplicaTry(xnetengine* Engine, const xredissentinelconfig* Config,
    double Deadline, double CloseDue, xredissentinelendpoint Endpoint, xredissentinelendpoint Promoted,
    bool* IsMaster, xredisclient** Output)
{
    int64 Remaining = appReplicaRemaining(Deadline);
    if (!Remaining) return XREDIS_SENTINEL_TIMEOUT;
    xredisclient* Client = NULL;
    xredissentinelstatus Status = Config->ConnectMaster(Engine, &Endpoint, Remaining, Config->Data, &Client);
    if (Status != XREDIS_SENTINEL_OK) return Status;
    if (!Client) return XREDIS_SENTINEL_FACTORY;
    const xbytesview Role = XRT_BYTES_LITERAL("ROLE");
    xfuture* Request = NULL;
    Status = appReplicaCall(Client, Config->Protocol, &Role, 1, Deadline, &Request);
    if (Status == XREDIS_SENTINEL_OK) {
        const xredisrequestresult* Result = xrtFutureValue(Request);
        xredisroleinfo Info;
        xredisrolestatus Parsed = Result->Status == XREDIS_REQUEST_OK ?
            xredisRoleRead(Result->Reply, &Config->Role, &Info) : XREDIS_ROLE_MALFORMED;
        if (Parsed == XREDIS_ROLE_VALUE) {
            *IsMaster = Info.Kind == XREDIS_ROLE_MASTER;
            xredissentinelendpoint Upstream = {Info.MasterHost, Info.MasterPort};
            if (Info.Kind == XREDIS_ROLE_REPLICA && Info.Link == XREDIS_ROLE_LINK_CONNECTED &&
                (!Promoted.Port || appReplicaEndpointEqual(Upstream, Promoted))) {
                xrtFutureDestroy(Request);
                *Output = Client;
                return XREDIS_SENTINEL_OK;
            }
            Status = XREDIS_SENTINEL_STALE_ROLE;
        } else if (Result->Status == XREDIS_REQUEST_SERVER_ERROR) Status = XREDIS_SENTINEL_SERVER_ERROR;
        else Status = Parsed == XREDIS_ROLE_LIMIT ? XREDIS_SENTINEL_LIMIT : XREDIS_SENTINEL_PROTOCOL;
    }
    xrtFutureDestroy(Request);
    return appReplicaClose(Client, CloseDue) ? Status : XREDIS_SENTINEL_TIMEOUT;
}

/* Run on the APP owner. The supplied factories apply endpoint and credential policy.
 * Result host bytes belong to Pick. Only a successful pick transfers a native client. */
static bool appReplicaPick(xnetengine* Engine, const xredissentinelconfig* Config, size_t Limit,
    xredissentinelendpoint Preferred, double Deadline, appreplicapick* Pick, xredisclient** Output)
{
    *Pick = (appreplicapick){.Status = XREDIS_SENTINEL_FACTORY};
    if (!Config->ConnectSeed || !Config->ConnectMaster || !Config->SeedCount ||
        Config->PreferredSeed >= Config->SeedCount || Config->SeedTimeout <= 0 ||
        Config->RetryDelay <= 0 || !Config->MaxRounds || !Limit || Limit > APP_REPLICA_ROWS)
        return false;

    const xbytesview Args[] = {XRT_BYTES_LITERAL("SENTINEL"), XRT_BYTES_LITERAL("REPLICAS"), Config->Service};
    xredissentinelstatus Last = XREDIS_SENTINEL_UNREACHABLE;
    for (size_t Round = 0; Round < Config->MaxRounds && appReplicaRemaining(Deadline); ++Round) {
        Pick->Rounds = Round + 1;
        size_t Unknown = 0;
        for (size_t Offset = 0; Offset < Config->SeedCount && appReplicaRemaining(Deadline); ++Offset) {
            size_t Seed = (Config->PreferredSeed + Offset) % Config->SeedCount;
            double SeedDue = fmin(Deadline, xrtTimer() + Config->SeedTimeout * .001);
            int64 SeedBudget = appReplicaRemaining(SeedDue);
            if (!SeedBudget) break;
            xredisclient* Control = NULL;
            ++Pick->SeedAttempts;
            Last = Config->ConnectSeed(Engine, Seed, SeedBudget, Config->Data, &Control);
            xfuture* List = NULL;
            if (Last == XREDIS_SENTINEL_OK)
                Last = Control ? appReplicaCall(Control, Config->Protocol, Args, 3, SeedDue, &List) : XREDIS_SENTINEL_FACTORY;
            if (Control && !appReplicaClose(Control, Deadline)) Last = XREDIS_SENTINEL_TIMEOUT;
            xredissentinelendpoint Candidates[APP_REPLICA_ROWS], Promoted = {0};
            size_t Count = 0;
            if (Last == XREDIS_SENTINEL_OK)
                Last = appReplicaList(xrtFutureValue(List), Config, Limit, Candidates, &Count, &Promoted);
            if (Last == XREDIS_SENTINEL_UNKNOWN_SERVICE) ++Unknown;
            if (Last == XREDIS_SENTINEL_OK) {
                for (size_t I = 1; I < Count; ++I) {
                    if (!Preferred.Port || !appReplicaEndpointEqual(Candidates[I], Preferred)) continue;
                    xredissentinelendpoint Selected = Candidates[I];
                    memmove(Candidates + 1, Candidates, I * sizeof(*Candidates));
                    Candidates[0] = Selected;
                    break;
                }
                Last = XREDIS_SENTINEL_UNREACHABLE;
                for (size_t I = 0; I < Count && appReplicaRemaining(Deadline); ++I) {
                    const xredissentinelendpoint Endpoint = Candidates[I];
                    if (Promoted.Port && appReplicaEndpointEqual(Endpoint, Promoted)) continue;
                    ++Pick->Candidates;
                    bool IsMaster = false;
                    Last = appReplicaTry(Engine, Config, fmin(Deadline, xrtTimer() + Config->SeedTimeout * .001),
                        Deadline, Endpoint, Promoted, &IsMaster, Output);
                    if (Last == XREDIS_SENTINEL_OK) {
                        Pick->Status = Last;
                        Pick->Seed = Seed;
                        Pick->Port = Endpoint.Port;
                        Pick->HostBytes = Endpoint.Host.Size;
                        memcpy(Pick->Host, Endpoint.Host.Data, Endpoint.Host.Size);
                        xrtFutureDestroy(List);
                        return true;
                    }
                    if (Last == XREDIS_SENTINEL_STALE_ROLE) {
                        ++Pick->Stale;
                        if (IsMaster && !Promoted.Port && Preferred.Port &&
                            appReplicaEndpointEqual(Endpoint, Preferred)) break;
                    } else if (Last != XREDIS_SENTINEL_UNREACHABLE && Last != XREDIS_SENTINEL_TIMEOUT) break;
                }
            }
            xrtFutureDestroy(List);
            if (Last != XREDIS_SENTINEL_UNKNOWN_SERVICE && Last != XREDIS_SENTINEL_UNREACHABLE &&
                Last != XREDIS_SENTINEL_STALE_ROLE && Last != XREDIS_SENTINEL_TIMEOUT) {
                Pick->Status = Last;
                return false;
            }
        }
        if (Unknown == Config->SeedCount) break;
        int64 Remaining = appReplicaRemaining(Deadline);
        if (Round + 1 < Config->MaxRounds && Remaining > Config->RetryDelay)
            xrtSleep((uint32)Config->RetryDelay);
    }
    Pick->Status = appReplicaRemaining(Deadline) ? Last : XREDIS_SENTINEL_TIMEOUT;
    return false;
}
#endif
