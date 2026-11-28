#ifndef XRPC_EXAMPLE_APP_DISPATCH_H
#define XRPC_EXAMPLE_APP_DISPATCH_H
#include <xrpc.h>
#include <xrt/json.h>
#include <string.h>

enum { APP_MAX_MESSAGES = 16, APP_INPUT_CAPACITY = 4096, APP_REPLY_CAPACITY = 8192 };

typedef struct apprequest {
    uint64 Token;
    size_t InputBytes, ReplyBytes;
    bool Ready;
    char Input[APP_INPUT_CAPACITY], Reply[APP_REPLY_CAPACITY];
} apprequest;

/* This sample APP owns one queued document. A real APP can hand its owned
 * copy to a worker instead and send the completed reply from the owner. */
static inline bool appTakeRequest(uint64 Token, xstrview Input, xbytesview Context,
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

typedef struct appreplybuffer {
    char* Data;
    size_t Capacity, Used;
} appreplybuffer;

static bool appEmit(const xrpcoutgoing* Message, bool Batch, size_t Bytes, ptr Data)
{
    appreplybuffer* Buffer = Data;
    size_t Prefix = Batch ? 1u : 0u;
    size_t Available = Buffer->Capacity - Buffer->Used;
    if (Available <= Prefix || Bytes >= Available - Prefix) return false;
    if (Batch) {
        char Separator = Buffer->Used ? ',' : '[';
        Buffer->Data[Buffer->Used++] = Separator;
    }
    if (!xrpcEncodeWrite(Buffer->Data + Buffer->Used, Bytes, Message, 1, false, NULL, NULL))
        return false;
    Buffer->Used += Bytes;
    return true;
}

static bool appPrepareReply(xstrview Input, char* Reply, size_t Capacity, size_t* Written)
{
    xrpcmessage Scratch[APP_MAX_MESSAGES];
    xrpcdispatchinfo Info = {.Size = sizeof(Info)};
    appreplybuffer Buffer = {.Data = Reply, .Capacity = Capacity};
    if (!xrpcDispatch(Input, NULL, Scratch, APP_MAX_MESSAGES, appEcho, NULL, appEmit, &Buffer, &Info))
        return false;
    if (Buffer.Used && Info.Batch) Reply[Buffer.Used++] = ']';
    *Written = Buffer.Used;
    return true;
}

static bool appPrepare(apprequest* Job)
{
    Job->Ready = appPrepareReply((xstrview){Job->Input, Job->InputBytes}, Job->Reply,
        sizeof(Job->Reply), &Job->ReplyBytes);
    return Job->Ready;
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
