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

static bool sendAll(xnetsocket Peer, xstrview Wire)
{
    while (Wire.Size) {
        size_t Sent;
        if (xrtNetSocketSend(Peer, Wire.Data, Wire.Size, &Sent) != XNET_RESULT_OK || !Sent)
            return false;
        Wire.Data += Sent;
        Wire.Size -= Sent;
    }
    return true;
}

static bool handle(xnetsocket Peer, xrpcduplex* Endpoint, xexecutor* Workers,
    appexecution* Job, xstrview Document)
{
    if (xrpcDuplexReceive(Endpoint, Document, (xbytesview){0}) != XRPC_DUPLEX_ACCEPTED ||
        !Job->Request.Token || !xrtExecutorSubmit(Workers, execute, Job, NULL, NULL) ||
        xrtEventWait(Job->Done) != XWAIT_OK || !Job->Request.Ready ||
        !appPublish(Endpoint, &Job->Request) || Job->Request.Token) return false;

    xrpcduplexoutput* Output = NULL;
    uint64 Token;
    int Count = xrpcDuplexNext(Endpoint, &Output, &Token);
    if (Count != 1) return Count == 0;
    bool Sent = xrpcDuplexStarted(Endpoint, Token) &&
        sendAll(Peer, xrpcDuplexOutputWire(Output)) &&
        sendAll(Peer, XRT_STR_LITERAL("\n")) && xrpcDuplexSent(Endpoint, Token);
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

static bool serve(xnetsocket Listener)
{
    appexecution Job = {0};
    const xexecutorconfig Pool = {.Threads = 2, .QueueLimit = 4};
    xrpcduplexconfig Config;
    xrpcduplex* Endpoint = NULL;
    if (!xrpcDuplexConfigInit(&Config)) return false;
    Config.OnRequest = appTakeRequest;
    Config.User = &Job.Request;
    if (!xrpcDuplexCreate(&Config, &Endpoint)) return false;

    Job.Done = xrtEventCreate(false, false);
    xexecutor* Workers = Job.Done ? xrtExecutorCreate(&Pool) : NULL;
    xnetsocket Peer = NULL;
    bool OK = Workers && xrtNetSocketAccept(Listener, &Peer, NULL) == XNET_RESULT_OK &&
        receive(Peer, Endpoint, Workers, &Job);
    if (Peer && !xrtNetSocketClose(Peer)) OK = false;
    if (Workers && !xrtExecutorDestroy(Workers)) OK = false;
    if (Job.Done && !xrtEventDestroy(Job.Done)) OK = false;
    bool Closed = xrpcDuplexClose(Endpoint, XRPC_CALL_CLOSED) &&
        xrpcDuplexStep(Endpoint) && xrpcDuplexDestroy(Endpoint);
    return OK && Closed;
}

int main(int Argc, char** Argv)
{
    if (Argc != 1 && (Argc != 2 || strcmp(Argv[1], "--serve") != 0)) return 2;
    xnetaddr Address;
    xnetsocket Listener = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
    if (!Listener) return 1;
    char Text[80];
    bool OK = xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) &&
        xrtNetSocketBind(Listener, &Address) && xrtNetSocketListen(Listener, 1) &&
        xrtNetSocketLocal(Listener, &Address) &&
        xrtNetAddrEndpointText(&Address, Text, sizeof(Text)) != 0 &&
        puts(Text) != EOF && fflush(stdout) == 0;
    if (OK && Argc == 2) OK = serve(Listener);
    bool Closed = xrtNetSocketClose(Listener);
    return OK && Closed ? 0 : 1;
}
