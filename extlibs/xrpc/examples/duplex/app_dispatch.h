#ifndef XRPC_EXAMPLE_APP_DISPATCH_H
#define XRPC_EXAMPLE_APP_DISPATCH_H
#include <xrpc.h>
#include <xrt/json.h>
#include <string.h>

enum { APP_MAX_MESSAGES = 16 };

typedef struct apprequest {
    uint64 Token;
    size_t InputBytes, ReplyBytes;
    bool Ready;
    char Input[4096], Reply[8192];
} apprequest;

/* This sample APP owns one queued document. A real APP can hand its owned
 * copy to a worker instead and send the completed reply from the owner. */
static bool appTakeRequest(uint64 Token, xstrview Input, xbytesview Context,
    const xrpcdocument* Document, const xrpcmessage* Messages, ptr Data)
{
    (void)Context;
    (void)Document;
    (void)Messages;
    apprequest* Job = Data;
    if (Job->Token || Input.Size > sizeof(Job->Input)) return false;
    if (Input.Size) memcpy(Job->Input, Input.Data, Input.Size);
    Job->InputBytes = Input.Size;
    Job->Token = Token;
    return true;
}

static xjsonvisitaction appMatchEcho(const xjsonevent* Event, ptr Data)
{
    bool* Match = Data;
    *Match = Event->Type == XJSON_EVENT_STRING && Event->Value.String.Size == 4 &&
        !memcmp(Event->Value.String.Data, "echo", 4);
    return XJSON_VISIT_NEXT;
}

static bool appEcho(const xrpcmessage* Request, xrpcreply* Reply, ptr Data)
{
    (void)Data;
    xstrview Method = Request->Method.Raw;
    if (Method.Size != 6 || memcmp(Method.Data, "\"echo\"", 6)) {
        if (!memchr(Method.Data, '\\', Method.Size)) return false;
        bool Match = false;
        xjsonreadconfig Config;
        xrtJsonReadConfigInit(&Config);
        if (xrtJsonVisit(Method, &Config, appMatchEcho, &Match) != XJSON_VISIT_DONE)
            return xrpcReplyAbort(Reply, xrtGetError());
        if (!Match) return false;
    }
    xstrview Value = Request->Params.Raw.Size ? Request->Params.Raw : XRT_STR_LITERAL("true");
    return xrpcReplyResult(Reply, Value);
}

static bool appEmit(const xrpcoutgoing* Message, bool Batch, size_t Bytes, ptr Data)
{
    apprequest* Job = Data;
    size_t Prefix = Batch ? 1u : 0u;
    if (Bytes >= sizeof(Job->Reply) - Job->ReplyBytes - Prefix) return false;
    if (Batch) {
        char Separator = Job->ReplyBytes ? ',' : '[';
        Job->Reply[Job->ReplyBytes++] = Separator;
    }
    if (!xrpcEncodeWrite(Job->Reply + Job->ReplyBytes, Bytes, Message, 1, false, NULL, NULL))
        return false;
    Job->ReplyBytes += Bytes;
    return true;
}

static bool appPrepare(apprequest* Job)
{
    xrpcmessage Scratch[APP_MAX_MESSAGES];
    xrpcdispatchinfo Info = {.Size = sizeof(Info)};
    if (!xrpcDispatch((xstrview){Job->Input, Job->InputBytes}, NULL, Scratch, APP_MAX_MESSAGES,
        appEcho, NULL, appEmit, Job, &Info)) return false;
    if (Job->ReplyBytes && Info.Batch) Job->Reply[Job->ReplyBytes++] = ']';
    Job->Ready = true;
    return true;
}

static bool appPublish(xrpcduplex* Endpoint, apprequest* Job)
{
    xrpcduplexadmission Status = xrpcDuplexReply(Endpoint, Job->Token,
        (xstrview){Job->Reply, Job->ReplyBytes});
    if (Status == XRPC_DUPLEX_OVERLOADED) return true;
    if (Status != XRPC_DUPLEX_ACCEPTED) return false;
    Job->Token = 0;
    Job->ReplyBytes = 0;
    Job->Ready = false;
    return true;
}
static inline bool appFinish(xrpcduplex* Endpoint, apprequest* Job)
{
    return !Job->Token || ((Job->Ready || appPrepare(Job)) && appPublish(Endpoint, Job));
}
#endif
