#include "../duplex/app_dispatch.h"
#include <xrt/executor.h>
#include <xrt/net.h>
#include <xrt/sync.h>
#include <xrt/thread.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
enum { MAX_PEERS = 32, MAX_WORKERS = 8 };
#if defined(_WIN32) || defined(_WIN64)
enum { MAX_RECORDS = 8 };
#else
enum { MAX_RECORDS = 4 };
#endif

typedef struct appecho {
    xstrview Id, Result;
} appecho;

typedef struct apprecord {
    xstrview Input, Reply;
} apprecord;

typedef struct appexecution {
    apprecord Records[MAX_RECORDS];
    appecho Echo[APP_MAX_MESSAGES];
    size_t EchoCount;
    bool EchoBatch;
    char Input[APP_INPUT_CAPACITY + 1], Replies[APP_REPLY_CAPACITY];
    size_t First, Count, Prepared;
    uint64 Token, Submitted;
    xevent* Done;
} appexecution;

typedef struct apppeer {
    xnetsocket Socket;
    xexecutor* Workers;
    appexecution Job;
    bool Duplex;
    uint64 Requests;
} apppeer;

static bool takeRequest(uint64 Token, xstrview Input, xbytesview Context,
    const xrpcdocument* Document, const xrpcmessage* Messages, ptr Data)
{
    appexecution* Job = Data;
    (void)Input;
    (void)Context;
    if (Job->Token) return false;
    Job->Token = Token;
    Job->EchoCount = 0;
    if (Job->Count != 1) return true;
    if (!Document || !Document->Count || Document->Count > APP_MAX_MESSAGES) return true;

    /* Both the peer and its input buffer live through executor retirement. */
    for (size_t Index = 0; Index < Document->Count; ++Index) {
        const xrpcmessage* Message = Messages + Index;
        if (Message->Kind != XRPC_MESSAGE_CALL || Message->Method.Raw.Size != 6 ||
            memcmp(Message->Method.Raw.Data, "\"echo\"", 6)) return true;
        xstrview Id = Message->Id.Raw;
        xstrview Params = Message->Params.Raw;
        Job->Echo[Index].Id = Id;
        Job->Echo[Index].Result = Params.Size
            ? Params
            : XRT_STR_LITERAL("true");
    }
    Job->EchoBatch = Document->Batch;
    Job->EchoCount = Document->Count;
    return true;
}

static void execute(ptr Data)
{
    appexecution* Job = Data;
    if (Job->EchoCount) {
        xrpcoutgoing Responses[APP_MAX_MESSAGES];
        for (size_t Index = 0; Index < Job->EchoCount; ++Index) {
            Responses[Index] = (xrpcoutgoing){
                .Kind = XRPC_MESSAGE_RESULT,
                .Id = Job->Echo[Index].Id,
                .Result = Job->Echo[Index].Result,
            };
        }
        size_t Bytes;
        if (xrpcEncodeWrite(Job->Replies, sizeof(Job->Replies) - 1,
            Responses, Job->EchoCount, Job->EchoBatch, NULL, &Bytes)) {
            Job->Records[0].Reply = (xstrview){Job->Replies, Bytes};
            Job->Prepared = 1;
        } else Job->Prepared = 0;
        xrtEventSet(Job->Done);
        return;
    }

    size_t Used = 0, Index = Job->First;
    /* The owner leaves the input intact until the job and its replies retire. */
    for (; Index < Job->Count; ++Index) {
        apprecord* Record = &Job->Records[Index];
        char* Reply = Job->Replies + Used;
        size_t Bytes;
        if (!appPrepareReply(Record->Input, Reply, sizeof(Job->Replies) - Used, &Bytes))
            break;
        Record->Reply = (xstrview){Reply, Bytes};
        Used += Bytes;
    }
    Job->Prepared = Index;
    xrtEventSet(Job->Done);
}

static bool sendRecord(xnetsocket Peer, xstrview Wire)
{
    xnetspan Spans[] = {{(cbytes)Wire.Data, Wire.Size}, {(cbytes)"\n", 1}};
    while (Spans[0].Size) {
        size_t Sent;
        if (xrtNetSocketSendVec(Peer, Spans, 2, &Sent) != XNET_RESULT_OK || !Sent)
            return false;
        if (Sent > Spans[0].Size) return true;
        Spans[0].Data += Sent;
        Spans[0].Size -= Sent;
    }
    size_t Sent;
    return xrtNetSocketSend(Peer, Spans[1].Data, 1, &Sent) == XNET_RESULT_OK && Sent == 1;
}

static bool publish(xnetsocket Peer, xrpcduplex* Endpoint, appexecution* Job, const apprecord* Record)
{
    if (!Endpoint) return !Record->Reply.Size || sendRecord(Peer, Record->Reply);

    /* Admit and send each reply before a later record can close the endpoint. */
    if ((!Job->Token && xrpcDuplexReceive(Endpoint, Record->Input, (xbytesview){0}) != XRPC_DUPLEX_ACCEPTED) ||
        !Job->Token || xrpcDuplexReply(Endpoint, Job->Token, Record->Reply) != XRPC_DUPLEX_ACCEPTED)
        return false;
    Job->Token = 0;
    xrpcduplexoutput* Output = NULL;
    uint64 Token;
    int Count = xrpcDuplexNext(Endpoint, &Output, &Token);
    if (Count != 1) return Count == 0;
    bool Sent = xrpcDuplexStarted(Endpoint, Token) &&
        sendRecord(Peer, xrpcDuplexOutputWire(Output)) && xrpcDuplexSent(Endpoint, Token);
    xrpcDuplexOutputRelease(Output);
    return Sent;
}

static bool perform(xnetsocket Peer, xrpcduplex* Endpoint, xexecutor* Workers,
    appexecution* Job, uint64* Requests)
{
    Job->EchoCount = 0;
    if (Job->Count == 1 && Endpoint &&
        (xrpcDuplexReceive(Endpoint, Job->Records[0].Input, (xbytesview){0}) != XRPC_DUPLEX_ACCEPTED ||
            !Job->Token)) return false;
    for (size_t First = 0; First < Job->Count;) {
        Job->First = First;
        if (!xrtExecutorSubmit(Workers, execute, Job, NULL, NULL)) return false;
        ++Job->Submitted;
        if (xrtEventWait(Job->Done) != XWAIT_OK || Job->Prepared == First) return false;
        for (; First < Job->Prepared; ++First) {
            if (!publish(Peer, Endpoint, Job, &Job->Records[First])) return false;
            ++*Requests;
        }
        /* A reply that could not fit gets an empty arena on the next job. */
    }
    Job->Count = 0;
    return true;
}

static bool receive(xnetsocket Peer, xrpcduplex* Endpoint, xexecutor* Workers,
    appexecution* Job, uint64* Requests)
{
    char* Record = Job->Input;
    size_t Used = 0, Scanned = 0;
    for (;;) {
        size_t Count;
        xnetresult Result = xrtNetSocketRecv(Peer, Record + Used, sizeof(Job->Input) - Used, &Count);
        if (Result == XNET_RESULT_CLOSED) return Used == 0;
        if (Result != XNET_RESULT_OK || !Count) return false;
        Used += Count;

        size_t Start = 0;
        while (Scanned < Used) {
            const char* End = memchr(Record + Scanned, '\n', Used - Scanned);
            if (!End) break;
            size_t Bytes = (size_t)(End - (Record + Start));
            Job->Records[Job->Count++].Input = (xstrview){Record + Start, Bytes};
            if (Job->Count == MAX_RECORDS && !perform(Peer, Endpoint, Workers, Job, Requests))
                return false;
            Start += Bytes + 1;
            Scanned = Start;
        }
        if (!perform(Peer, Endpoint, Workers, Job, Requests)) return false;
        if (Start) {
            Used -= Start;
            if (Used) memmove(Record, Record + Start, Used);
        }
        /* The remaining bytes have already been searched for a delimiter. */
        Scanned = Used;
        if (Used == sizeof(Job->Input)) return false;
    }
}

static bool runPeer(apppeer* Peer)
{
    appexecution* Job = &Peer->Job;
    xrpcduplexconfig Config;
    xrpcduplex* Endpoint = NULL;
    if (Peer->Duplex) {
        if (!xrpcDuplexConfigInit(&Config)) return false;
        /* This APP completes one inbound job at a time and initiates no calls. */
        Config.MaxIncoming = 1;
        Config.MaxOperations = 1;
        Config.MaxPendingCalls = 1;
        Config.Response.Decode.MaxMessages = APP_MAX_MESSAGES;
        Config.OnRequest = takeRequest;
        Config.User = Job;
        if (!xrpcDuplexCreate(&Config, &Endpoint)) return false;
    }

    Job->Done = xrtEventCreate(false, false);
    bool OK = Job->Done && receive(Peer->Socket, Endpoint, Peer->Workers, Job, &Peer->Requests);
    bool Closed = !Endpoint || (xrpcDuplexClose(Endpoint, XRPC_CALL_CLOSED) &&
        xrpcDuplexStep(Endpoint) && xrpcDuplexDestroy(Endpoint));
    return OK && Closed;
}

static int32 drivePeer(ptr Data)
{
    apppeer* Peer = Data;
    bool OK = runPeer(Peer);
    bool EOFSignaled = xrtNetSocketShutdown(Peer->Socket, XNET_SHUTDOWN_WRITE);
    return OK && EOFSignaled ? 0 : 1;
}

static bool serve(xnetsocket Listener, bool Duplex, unsigned Clients, unsigned Threads, bool Report)
{
    apppeer* Peers = calloc(Clients, sizeof(*Peers));
    if (!Peers) return false;
    const xexecutorconfig Pool = {.Threads = Threads, .QueueLimit = Clients > 4 ? Clients : 4};
    xexecutor* Workers = xrtExecutorCreate(&Pool);
    if (!Workers) {
        free(Peers);
        return false;
    }
    xthread* Owners[MAX_PEERS] = {0};
    unsigned Accepted = 0, Started = 0;
    bool OK = true;
    while (Accepted < Clients) {
        xnetsocket Socket = NULL;
        if (xrtNetSocketAccept(Listener, &Socket, NULL) != XNET_RESULT_OK) {
            OK = false;
            break;
        }
        apppeer* Peer = &Peers[Accepted++];
        *Peer = (apppeer){.Socket = Socket, .Workers = Workers, .Duplex = Duplex};
        if (!xrtNetSocketSet(Socket, XNET_OPTION_NO_DELAY, 1)) {
            OK = false;
            break;
        }
        if (Clients > 1) {
            xthread* Owner = xrtThreadCreate(drivePeer, Peer, 0);
            if (!Owner) {
                OK = false;
                break;
            }
            Owners[Started++] = Owner;
        }
    }
    if (OK && Clients == 1) OK = drivePeer(&Peers[0]) == 0;
    if (!OK) {
        /* Unblock active owners before joining. The listener owner still owns the sockets. */
        for (unsigned Index = 0; Index < Started; ++Index)
            (void)xrtNetSocketShutdown(Peers[Index].Socket, XNET_SHUTDOWN_BOTH);
    }
    for (unsigned Index = 0; Index < Started; ++Index) {
        if (xrtThreadWait(Owners[Index]) != XWAIT_OK) abort();
        if (xrtThreadExitCode(Owners[Index])) OK = false;
        xrtThreadDestroy(Owners[Index]);
    }
    uint64 Requests = 0, Jobs = 0;
    for (unsigned Index = 0; Index < Accepted; ++Index) {
        Requests += Peers[Index].Requests;
        Jobs += Peers[Index].Job.Submitted;
        if (!xrtNetSocketClose(Peers[Index].Socket)) OK = false;
    }
    xexecutorstats Stats;
    if (!xrtExecutorClose(Workers) || xrtExecutorWait(Workers) != XWAIT_OK ||
        !xrtExecutorGet(Workers, &Stats) || Stats.Submitted != Jobs || Stats.Completed != Jobs ||
        Stats.Executed != Jobs || Stats.Queued || Stats.Running || Stats.Rejected || Stats.Cancelled) OK = false;
    if (!xrtExecutorDestroy(Workers)) abort();
    for (unsigned Index = 0; Index < Accepted; ++Index)
        if (Peers[Index].Job.Done && !xrtEventDestroy(Peers[Index].Job.Done)) OK = false;
    free(Peers);
    if (Report && OK) {
        if (printf("{\"connections\":%u,\"workers\":%u,\"requests\":%" PRIu64 "}\n",
            Accepted, Threads, Requests) < 0 || fflush(stdout)) OK = false;
    }
    return OK;
}

static bool argument(const char* Text, unsigned Limit, unsigned* Output)
{
    char* End;
    errno = 0;
    unsigned long Value = strtoul(Text, &End, 10);
    if (*Text < '0' || *Text > '9' || *End || errno || !Value || Value > Limit) return false;
    *Output = (unsigned)Value;
    return true;
}

int main(int Argc, char** Argv)
{
    bool Direct = Argc > 1 && !strcmp(Argv[1], "--dispatch-only");
    unsigned Clients = 1, Workers = 2;
    if (Argc != 1 && ((Argc != 2 && Argc != 4) || (!Direct && strcmp(Argv[1], "--serve")))) return 2;
    if (Argc == 4 && (!argument(Argv[2], MAX_PEERS, &Clients) || !argument(Argv[3], MAX_WORKERS, &Workers))) return 2;
    xnetaddr Address;
    xnetsocket Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
    if (!Listener) return 1;
    char Text[80];
    bool OK = xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
        xrtNetSocketBind(Listener, &Address) && xrtNetSocketListen(Listener, (int)Clients) &&
        xrtNetSocketLocal(Listener, &Address) &&
        xrtNetAddrEndpointText(&Address, Text, sizeof(Text)) != 0 &&
        puts(Text) != EOF && fflush(stdout) == 0;
    if (OK && Argc > 1) OK = serve(Listener, !Direct, Clients, Workers, Argc == 4);
    bool Closed = xrtNetSocketClose(Listener);
    return OK && Closed ? 0 : 1;
}
