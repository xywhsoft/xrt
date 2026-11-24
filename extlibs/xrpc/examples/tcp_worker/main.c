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

typedef struct appexecution {
    apprequest Request;
    xstrview EchoId, EchoParams;
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
    if (!appTakeRequest(Token, Input, Context, Document, Messages, &Job->Request))
        return false;
    Job->EchoId = (xstrview){0};
    if (Document && !Document->Batch && Document->Count == 1 &&
        Messages[0].Kind == XRPC_MESSAGE_CALL && Messages[0].Method.Raw.Size == 6 &&
        !memcmp(Messages[0].Method.Raw.Data, "\"echo\"", 6)) {
        xstrview Id = Messages[0].Id.Raw;
        xstrview Params = Messages[0].Params.Raw;
        /* Rebase the borrowed fields onto the job's input copy before the worker runs. */
        Job->EchoId = (xstrview){Job->Request.Input + (Id.Data - Input.Data), Id.Size};
        Job->EchoParams = Params.Size
            ? (xstrview){Job->Request.Input + (Params.Data - Input.Data), Params.Size}
            : XRT_STR_LITERAL("true");
    }
    return true;
}

static void execute(ptr Data)
{
    appexecution* Job = Data;
    if (Job->EchoId.Size) {
        xrpcoutgoing Response = {
            .Kind = XRPC_MESSAGE_RESULT,
            .Id = Job->EchoId,
            .Result = Job->EchoParams,
        };
        Job->Request.Ready = xrpcEncodeWrite(Job->Request.Reply, sizeof(Job->Request.Reply) - 1,
            &Response, 1, false, NULL, &Job->Request.ReplyBytes);
    } else {
        (void)appPrepare(&Job->Request);
    }
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

static bool handle(xnetsocket Peer, xrpcduplex* Endpoint, xexecutor* Workers,
    appexecution* Job, xstrview Document)
{
    if (Endpoint) {
        if (xrpcDuplexReceive(Endpoint, Document, (xbytesview){0}) != XRPC_DUPLEX_ACCEPTED ||
            !Job->Request.Token) return false;
    } else {
        memcpy(Job->Request.Input, Document.Data, Document.Size);
        Job->Request.InputBytes = Document.Size;
    }
    if (!xrtExecutorSubmit(Workers, execute, Job, NULL, NULL) ||
        xrtEventWait(Job->Done) != XWAIT_OK || !Job->Request.Ready) return false;

    if (!Endpoint) {
        xstrview Reply = {Job->Request.Reply, Job->Request.ReplyBytes};
        Job->Request.ReplyBytes = 0;
        Job->Request.Ready = false;
        return !Reply.Size || sendRecord(Peer, Reply);
    }
    if (!appPublish(Endpoint, &Job->Request) || Job->Request.Token) return false;

    xrpcduplexoutput* Output = NULL;
    uint64 Token;
    int Count = xrpcDuplexNext(Endpoint, &Output, &Token);
    if (Count != 1) return Count == 0;
    bool Sent = xrpcDuplexStarted(Endpoint, Token) &&
        sendRecord(Peer, xrpcDuplexOutputWire(Output)) && xrpcDuplexSent(Endpoint, Token);
    xrpcDuplexOutputRelease(Output);
    return Sent;
}

static bool receive(xnetsocket Peer, xrpcduplex* Endpoint, xexecutor* Workers,
    appexecution* Job, uint64* Requests)
{
    char Record[sizeof(Job->Request.Input) + 1];
    size_t Used = 0, Scanned = 0;
    for (;;) {
        size_t Count;
        xnetresult Result = xrtNetSocketRecv(Peer, Record + Used, sizeof(Record) - Used, &Count);
        if (Result == XNET_RESULT_CLOSED) return Used == 0;
        if (Result != XNET_RESULT_OK || !Count) return false;
        Used += Count;

        size_t Start = 0;
        while (Scanned < Used) {
            const char* End = memchr(Record + Scanned, '\n', Used - Scanned);
            if (!End) break;
            size_t Bytes = (size_t)(End - (Record + Start));
            if (!handle(Peer, Endpoint, Workers, Job, (xstrview){Record + Start, Bytes}))
                return false;
            ++*Requests;
            Start += Bytes + 1;
            Scanned = Start;
        }
        if (Start) {
            Used -= Start;
            if (Used) memmove(Record, Record + Start, Used);
        }
        /* The remaining bytes have already been searched for a delimiter. */
        Scanned = Used;
        if (Used == sizeof(Record)) return false;
    }
}

static bool runPeer(apppeer* Peer)
{
    appexecution* Job = &Peer->Job;
    xrpcduplexconfig Config;
    xrpcduplex* Endpoint = NULL;
    if (Peer->Duplex) {
        if (!xrpcDuplexConfigInit(&Config)) return false;
        Config.MaxIncoming = 1;
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
    uint64 Requests = 0;
    for (unsigned Index = 0; Index < Accepted; ++Index) {
        Requests += Peers[Index].Requests;
        if (!xrtNetSocketClose(Peers[Index].Socket)) OK = false;
    }
    xexecutorstats Stats;
    if (!xrtExecutorClose(Workers) || xrtExecutorWait(Workers) != XWAIT_OK ||
        !xrtExecutorGet(Workers, &Stats) || Stats.Submitted != Requests || Stats.Completed != Requests ||
        Stats.Executed != Requests || Stats.Queued || Stats.Running || Stats.Rejected || Stats.Cancelled) OK = false;
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
