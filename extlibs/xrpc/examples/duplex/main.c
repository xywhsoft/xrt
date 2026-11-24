#include "app_dispatch.h"
#include <stdio.h>

static bool transfer(xrpcduplex* From, xrpcduplex* To)
{
    uint64 Token;
    xrpcduplexoutput* Output = NULL;
    if (xrpcDuplexNext(From, &Output, &Token) != 1) return false;
    bool OK = xrpcDuplexStarted(From, Token) && xrpcDuplexSent(From, Token) &&
        xrpcDuplexReceive(To, xrpcDuplexOutputWire(Output), (xbytesview){0}) == XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(Output);
    return OK;
}

static bool closeEndpoint(xrpcduplex* D)
{
    return !D || (xrpcDuplexClose(D, XRPC_CALL_CLOSED) &&
        xrpcDuplexStep(D) && xrpcDuplexDestroy(D));
}

int main(void)
{
    xrpcduplex *Client = NULL, *Server = NULL;
    xfuture* Call = NULL;
    xrpcduplexconfig Config;
    apprequest Job = {0};
    if (!xrpcDuplexConfigInit(&Config)) return 1;
    Config.MaxIncoming = 1;
    Config.OnRequest = appTakeRequest;
    Config.User = &Job;
    bool OK = xrpcDuplexCreate(NULL, &Client) && xrpcDuplexCreate(&Config, &Server) &&
        xrpcDuplexCall(Client, XRT_STR_LITERAL("echo"), XRT_STR_LITERAL("[42]"), 5000, &Call) ==
            XRPC_DUPLEX_ACCEPTED &&
        xrpcDuplexStep(Client) && transfer(Client, Server) && Job.Token &&
        appFinish(Server, &Job) && transfer(Server, Client);
    const xrpccallresult* Result = OK ? xrtFutureValue(Call) : NULL;
    OK = Result && Result->Status == XRPC_CALL_OK && Result->Delivery == XRPC_REPLIED;
    if (Call) xrtFutureDestroy(Call);
    bool ClientClosed = closeEndpoint(Client);
    bool ServerClosed = closeEndpoint(Server);
    if (!OK || !ClientClosed || !ServerClosed) return 2;
    puts("APP completed a deferred JSON-RPC echo");
    return 0;
}
