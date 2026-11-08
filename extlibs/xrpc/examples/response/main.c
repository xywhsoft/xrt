#include <xrpc.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
	char Wire[]="{\"jsonrpc\":\"2.0\",\"id\":1e9999,\"result\":[1,2,3]}";xrpcmessage Scratch;xrpcresponses* R=NULL;
	if(xrpcResponsesRead((xstrview){Wire,strlen(Wire)},NULL,&Scratch,1,&R)!=XRPC_RESPONSE_DONE)return 1;
	memset(Wire,0,sizeof(Wire));xrpcresponseinfo I={0};I.Size=sizeof(I);
	bool OK=xrpcResponsesInfo(R,&I) && I.Count==1 && I.Messages[0].Result.Raw.Size==7 && !memcmp(I.Messages[0].Result.Raw.Data,"[1,2,3]",7);
	xrpcResponsesRelease(R);if(!OK)return 2;puts("[PASS] response payload stays valid after receive buffer reuse");return 0;
}
