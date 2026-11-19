#include "../duplex/app_dispatch.h"
#include <xrt/executor.h>
#include <xrt/net.h>
#include <xrt/sync.h>
#include <stdio.h>

typedef struct appexecution {
    apprequest Request;
    xevent* Done;
} appexecution;

static void execute(ptr Data)
{
    appexecution* Job = Data;
    (void)appPrepare(&Job->Request);
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
    appexecution* Job)
{
    char Record[sizeof(Job->Request.Input) + 1];
    size_t Used = 0;
    for (;;) {
        size_t Count;
        xnetresult Result = xrtNetSocketRecv(Peer, Record + Used, sizeof(Record) - Used, &Count);
        if (Result == XNET_RESULT_CLOSED) return Used == 0;
        if (Result != XNET_RESULT_OK || !Count) return false;
        Used += Count;

        size_t Start = 0;
        const char* End;
        while ((End = memchr(Record + Start, '\n', Used - Start)) != NULL) {
            size_t Bytes = (size_t)(End - (Record + Start));
            if (!handle(Peer, Endpoint, Workers, Job, (xstrview){Record + Start, Bytes}))
                return false;
            Start += Bytes + 1;
        }
        if (Start) {
            Used -= Start;
            memmove(Record, Record + Start, Used);
        }
        if (Used == sizeof(Record)) return false;
    }
}

static bool serve(xnetsocket Listener, bool Duplex)
{
    appexecution Job = {0};
    const xexecutorconfig Pool = {.Threads = 2, .QueueLimit = 4};
    xrpcduplexconfig Config;
    xrpcduplex* Endpoint = NULL;
    if (Duplex) {
        if (!xrpcDuplexConfigInit(&Config)) return false;
        Config.OnRequest = appTakeRequest;
        Config.User = &Job.Request;
        if (!xrpcDuplexCreate(&Config, &Endpoint)) return false;
    }

    Job.Done = xrtEventCreate(false, false);
    xexecutor* Workers = Job.Done ? xrtExecutorCreate(&Pool) : NULL;
    xnetsocket Peer = NULL;
    bool OK = Workers && xrtNetSocketAccept(Listener, &Peer, NULL) == XNET_RESULT_OK &&
        xrtNetSocketSet(Peer, XNET_OPTION_NO_DELAY, 1) && receive(Peer, Endpoint, Workers, &Job);
    if (Peer && !xrtNetSocketClose(Peer)) OK = false;
    if (Workers && !xrtExecutorDestroy(Workers)) OK = false;
    if (Job.Done && !xrtEventDestroy(Job.Done)) OK = false;
    bool Closed = !Endpoint || (xrpcDuplexClose(Endpoint, XRPC_CALL_CLOSED) &&
        xrpcDuplexStep(Endpoint) && xrpcDuplexDestroy(Endpoint));
    return OK && Closed;
}

int main(int Argc, char** Argv)
{
    bool Direct = Argc == 2 && !strcmp(Argv[1], "--dispatch-only");
    if (Argc != 1 && (Argc != 2 || (!Direct && strcmp(Argv[1], "--serve")))) return 2;
    xnetaddr Address;
    xnetsocket Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
    if (!Listener) return 1;
    char Text[80];
    bool OK = xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
        xrtNetSocketBind(Listener, &Address) && xrtNetSocketListen(Listener, 1) &&
        xrtNetSocketLocal(Listener, &Address) &&
        xrtNetAddrEndpointText(&Address, Text, sizeof(Text)) != 0 &&
        puts(Text) != EOF && fflush(stdout) == 0;
    if (OK && Argc == 2) OK = serve(Listener, !Direct);
    bool Closed = xrtNetSocketClose(Listener);
    return OK && Closed ? 0 : 1;
}
