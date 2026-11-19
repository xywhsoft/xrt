#ifndef XRPC_EXAMPLE_APP_DISPATCH_H
#define XRPC_EXAMPLE_APP_DISPATCH_H
#include <xrpc.h>
#include <string.h>

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

static bool appEcho(const xrpcmessage* Request, xrpcreply* Reply, ptr Data)
{
    (void)Data;
    if (Request->Method.Raw.Size != 6 || memcmp(Request->Method.Raw.Data, "\"echo\"", 6))
        return false;
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
    xrpcmessage Scratch[16];
    xrpcdispatchinfo Info = {.Size = sizeof(Info)};
    if (!xrpcDispatch((xstrview){Job->Input, Job->InputBytes}, NULL, Scratch, 16,
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
