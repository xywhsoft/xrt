#include "test.h"
typedef struct deobserver {const xerror* Ambient;unsigned Old,Fresh,Calls,Emits;bool Notice,Nested;size_t Bytes;} deobserver;
static void deNotice(const xerror* E,ptr P)
{deobserver* O=P;if(E==O->Ambient&&xrtErrorFind(E,"dispatch.ambient",1))++O->Old;else ++O->Fresh;}
static bool deEmit(const xrpcoutgoing* R,bool Batch,size_t Bytes,ptr P)
{(void)R;(void)Batch;deobserver* O=P;++O->Emits;O->Bytes+=Bytes;return true;}
static bool deCall(const xrpcmessage* R,xrpcreply* Reply,ptr P)
{
    (void)R;deobserver* O=P;++O->Calls;
    if(O->Notice)xrtSetErrorInfo(XERR_STATE,"dispatch.callback",2,"successful business diagnostic");
    if(O->Nested){deobserver Child={0};xrpcmessage Scratch[2];xrpcdispatchinfo I={.Size=sizeof(I)};
        testRequire(xrpcDispatch(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":7}"),NULL,Scratch,2,deCall,&Child,deEmit,&Child,&I)&&Child.Calls==1&&Child.Emits==1,"nested independent dispatch");}
    return xrpcReplyResult(Reply,XRT_STR_LITERAL("true"));
}
static void deSuccess(bool Notice,bool Nested,bool Batch,bool Notification)
{
    deobserver O={.Notice=Notice,.Nested=Nested};xrtSetErrorInfo(XERR_STATE,"dispatch.ambient",1,"old diagnostic");O.Ambient=xrtGetError();testRequire(O.Ambient!=NULL,"caller error");xrtSetErrorHandler(deNotice,&O);
    xstrview Input=Notification?XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\"}"):Batch?XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":2}]"):XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"method\":\"m\",\"id\":1}");
    xrpcmessage Scratch[4];xrpcdispatchinfo I={.Size=sizeof(I)};testRequire(xrpcDispatch(Input,NULL,Scratch,4,deCall,&O,deEmit,&O,&I),"successful dispatch");
    testRequire(xrtGetError()==O.Ambient&&O.Calls==(Batch&&!Notification?2u:1u)&&O.Emits==(Notification?0u:O.Calls)&&I.Invoked==O.Calls&&I.Replies==O.Emits,"exact ambient owner and real callback effects");
    unsigned Expected=(Notice?O.Calls:0);printf("[DISPATCH NOTICES] old=%u fresh=%u expected=%u\n",O.Old,O.Fresh,Expected);
    xrtSetErrorHandler(NULL,NULL);testRequire(!O.Old&&O.Fresh==Expected,"successful or nested dispatch never re-notifies old diagnostics");xrtClearError();
}
int main(void)
{for(unsigned N=0;N<2;++N)for(unsigned R=0;R<2;++R)for(unsigned B=0;B<2;++B)for(unsigned Q=0;Q<2;++Q)deSuccess(N!=0,R!=0,B!=0,Q!=0);puts("[PASS]16 dispatch success/batch/notification/nesting/diagnostic ownership cases");return 0;}
