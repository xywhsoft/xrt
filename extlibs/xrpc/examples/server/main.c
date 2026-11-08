#include <xrpc/server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void require(bool OK){if(!OK){fprintf(stderr,"server example failed: %s\n",xrtErrorMessage(xrtGetError()));exit(1);}}
static bool echo(const xrpcmessage* Request,xrpcreply* Reply,const xrpcservercontext* Context,ptr User)
{(void)User;require(Context->Data.Size==4 && !memcmp(Context->Data.Data,"demo",4));if(xrtCancelRequested(Context->Cancel) || xrtTimer()>=Context->Deadline)return false;return xrpcReplyResult(Reply,Request->Params.Raw.Size?Request->Params.Raw:XRT_STR_LITERAL("null"));}
int main(void)
{
    /* No network/service prerequisite. Both handler invocations run on a Core
     * executor worker. Only the call has a response; the notification does not. */
    xrpcserverconfig Config;xrpcServerConfigInit(&Config);Config.Call=echo;xrpcserver* Server=NULL;require(xrpcServerCreate(&Config,&Server));
    unsigned char Data[]={'d','e','m','o'};
    xfuture* Future=NULL;require(xrpcServerSubmitWithContext(Server,XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":\"c\",\"method\":\"echo\",\"params\":[7]},{\"jsonrpc\":\"2.0\",\"method\":\"echo\"}]"),(xbytesview){Data,sizeof(Data)},5000,&Future)==XRPC_SERVER_ACCEPTED);
    memset(Data,'x',sizeof(Data)); /* Both handlers borrow the independent snapshot. */
    require(xrtFutureWaitFor(Future,5000)==XWAIT_OK);xfutureresult Value;require(xrtFutureResult(Future,&Value) && Value.State==XFUTURE_RESOLVED && Value.Value);
    const xrpcserverresult* Result=Value.Value;require(Result->Status==XRPC_SERVER_DONE && Result->Dispatch.Replies==1 && Result->Dispatch.Notifications==1);
    require(xrpcServerDestroy(Server)); /* Joins workers; response stays owned. */
    printf("%.*s\n",(int)Result->Wire.Size,Result->Wire.Data);
    xrtFutureDestroy(Future);return 0;
}
