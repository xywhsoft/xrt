#include <xrpc.h>
#include <stdio.h>

static bool echo(const xrpcmessage* Request,xrpcreply* Reply,ptr User)
{
	(void)User;
	return xrpcReplyResult(Reply,Request->Params.Kind==XRPC_VALUE_MISSING ? XRT_STR_LITERAL("null") : Request->Params.Raw);
}
typedef struct output {char Bytes[256];size_t Count;} output;
static bool emit(const xrpcoutgoing* Response,bool Batch,size_t Bytes,ptr User)
{
	output* Output=User;size_t Written=0;
	if(Batch || Output->Count || !xrpcEncodeWrite(Output->Bytes,sizeof(Output->Bytes),Response,1,false,NULL,&Written) || Written!=Bytes)return false;
	if(fwrite(Output->Bytes,1,Written,stdout)!=Written)return false;
	++Output->Count;putchar('\n');return true;
}
int main(void)
{
	xrpcmethod Bindings[]={{XRT_STR_LITERAL("echo"),echo,NULL}};xrpcmethods* Methods=NULL;
	if(!xrpcMethodsCreate(Bindings,1,NULL,&Methods))return 1;
	xrpcmessage Scratch[1];xrpcdispatchinfo Info={0};Info.Size=sizeof(Info);output Output={0};
	bool Done=xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"e\\u0063ho\",\"params\":[\"hello\"],\"id\":1e9999}"),
		NULL,Scratch,1,xrpcMethodsCall,Methods,emit,&Output,&Info);
	xrpcMethodsRelease(Methods);
	return Done && Output.Count==1 && Info.Replies==1 ? 0 : 2;
}
