/* Real TCP caller; independent stdlib peer asserts final bytes and EOF. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] graceful: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static const xmqttwriteresult* result(xfuture* F){check(F && xrtFutureWaitFor(F,8000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed result");return xrtFutureValue(F);}
typedef struct notifyContext {const char* Mode;xnetpost Post;xnetworker* Worker;xmqttclient* Client;bool Posted;} notifyContext;
static void abortTask(xnetworker* W,ptr Data){(void)W;notifyContext* N=Data;check(xmqttClientClose(N->Client),"posted abort");}
static void notify(xmqttclient* C,ptr Data){notifyContext* N=Data;xmqttclientinfo I;xmqttClientInfo(C,&I);if(I.State!=XMQTT_CLIENT_CLOSING || N->Posted)return;N->Posted=true;
    xfuture* F=(xfuture*)(uintptr_t)1;check(xmqttClientDisconnect(C,NULL,1000,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"worker blocking rejection");xrtClearError();
    if(!strcmp(N->Mode,"abort_notify"))check(xmqttClientClose(C),"reentrant abort");
    if(!strcmp(N->Mode,"abort_prefix")){N->Client=C;check(xrtNetPost(N->Worker,&N->Post,abortTask,N),"post abort after current byte budget");}
}
int main(int Argc,char** Argv){if(Argc!=4)return 2;unsigned Port=(unsigned)strtoul(Argv[1],NULL,10),V=(unsigned)strtoul(Argv[2],NULL,10);const char* Mode=Argv[3];
    bool Tiny=!strcmp(Mode,"receive_ack") || !strcmp(Mode,"byte") || !strcmp(Mode,"partial_pub") || !strcmp(Mode,"abort_prefix") || !strcmp(Mode,"timeout_prefix");
    bool Pub=!strcmp(Mode,"drain0") || !strcmp(Mode,"drain1") || !strcmp(Mode,"partial_pub") || !strcmp(Mode,"timeout_ack") || !strcmp(Mode,"cancel_ack") || !strcmp(Mode,"abort_ack") || !strcmp(Mode,"requests");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)V;notifyContext N={0};N.Mode=Mode;N.Worker=xrtNetEngineWorker(Engine,1);check(xrtNetPostInit(&N.Post),"abort post");
    if(!strcmp(Mode,"abort_notify") || !strcmp(Mode,"abort_prefix")){K.Notify=notify;K.User=&N;}
    if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
    if(!strcmp(Mode,"objects"))K.MaxObjects=2;
    if(!strcmp(Mode,"requests"))K.MaxRequests=1;
    if(!strcmp(Mode,"outgoing"))K.MaxOutgoingBytes=1;
    K.Codec.MaxPacketBytes=K.Codec.MaxPropertyBytes=2u*1024u*1024u;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("grace-probe");
    xmqttproperty Expiry={XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{0}};Expiry.Value.Number=60;if(!strcmp(Mode,"expiry_zero"))Connect.Properties=(xmqttpropertylist){&Expiry,1};
    xnetaddr Address;xrtNetAddrParse(&Address,"127.0.0.1",(uint16)Port);xmqttclient* C=NULL;check(xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);xfuture *F=(xfuture*)(uintptr_t)1,*Published=NULL;
    if(!strcmp(Mode,"before_ready")){check(xmqttClientDisconnectAsync(C,NULL,1000,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"before CONNACK rejected");xrtClearError();check(xmqttClientClose(C),"pre-ready abort");F=NULL;goto Cleanup;}
    check(((const xmqttreadyresult*)result(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");
    unsigned char* Payload=NULL;if(Pub){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=!strcmp(Mode,"drain0")?0:2;P.Body.Publish.Topic=XRT_STR_LITERAL("t");
        if(!strcmp(Mode,"partial_pub")){Payload=malloc(32768);check(Payload!=NULL,"payload");memset(Payload,'x',32768);P.Body.Publish.Payload=(xbytesview){Payload,32768};}
        check(xmqttClientPublishAsync(C,&P,5000,&Published)==XMQTT_ADMISSION_ACCEPTED,"prior publish");free(Payload);}
    xmqttpacket D;xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&D);char Text[]="before";xmqttproperty Props[21];size_t Count=0;char* Large=NULL;
    if(!strcmp(Mode,"reason4") || !strcmp(Mode,"cap3"))D.Reason=4;
    if(!strcmp(Mode,"snapshot") || !strcmp(Mode,"cap")){Props[Count++]=(xmqttproperty){XMQTT_PROPERTY_REASON_STRING,XMQTT_PROPERTY_STRING,{0}};Props[0].Value.Text=(xstrview){Text,6};Props[Count++]=(xmqttproperty){XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}};Props[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("k"),XRT_STR_LITERAL("v")};}
    if(!strcmp(Mode,"expiry_bad") || !strcmp(Mode,"expiry_zero")){Expiry.Value.Number=!strcmp(Mode,"expiry_bad")?1:0;Props[Count++]=Expiry;}
    if(!strcmp(Mode,"abort_prefix") || !strcmp(Mode,"timeout_prefix")){size_t Len=!strcmp(Mode,"abort_prefix")?8192:60000;Large=malloc(Len);check(Large!=NULL,"large diagnostic");memset(Large,'p',Len);
        if(!strcmp(Mode,"abort_prefix")){Props[Count]=(xmqttproperty){XMQTT_PROPERTY_REASON_STRING,XMQTT_PROPERTY_STRING,{0}};Props[Count++].Value.Text=(xstrview){Large,Len};}
        else for(unsigned J=0;J<20;++J){Props[Count]=(xmqttproperty){XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}};Props[Count++].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("n"),(xstrview){Large,Len}};}}
    D.Properties=(xmqttpropertylist){Props,Count};bool Reject=!strcmp(Mode,"objects") || !strcmp(Mode,"requests") || !strcmp(Mode,"outgoing") || !strcmp(Mode,"cap") || !strcmp(Mode,"expiry_bad");
    int64 Timeout=!strcmp(Mode,"timeout_zero")?0:!strcmp(Mode,"timeout_ack") || !strcmp(Mode,"timeout_prefix")?50:5000;
    size_t WireSize=0;check(V==5?xmqttPacketSize5(&D,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireSize):xmqttPacketSize311(&D,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireSize),"disconnect wire size");
    xmqttclientadmission A=xmqttClientDisconnectAsync(C,&D,Timeout,&F);memset(Text,'z',6);free(Large);
    if(Reject){check(A==(!strcmp(Mode,"cap") || !strcmp(Mode,"expiry_bad")?XMQTT_ADMISSION_ERROR:XMQTT_ADMISSION_OVERLOADED) && F==(xfuture*)(uintptr_t)1,"rejected disconnect unchanged output");F=NULL;xrtClearError();xmqttclientinfo I;xmqttClientInfo(C,&I);check(I.State==XMQTT_CLIENT_OPEN,"rejection leaves open");check(xmqttClientClose(C),"rejected close fallback");goto Cleanup;}
    check(A==XMQTT_ADMISSION_ACCEPTED,"disconnect admission");
    if(!strcmp(Mode,"duplicate")){xfuture* Other=(xfuture*)(uintptr_t)1;xrtSetErrorInfo(XERR_STATE,"ambient",1,"preserve");const xerror* Error=xrtGetError();check(xmqttClientDisconnectAsync(C,NULL,5000,&Other)==XMQTT_ADMISSION_CLOSED && Other==(xfuture*)(uintptr_t)1 && xrtGetError()==Error,"duplicate close preserves owner/error");xrtClearError();}
    if(!strcmp(Mode,"cancel_ack"))check(xrtFutureCancel(F),"cancel close");
    if(!strcmp(Mode,"abort_ack"))check(xmqttClientClose(C),"abort close");

    {const xmqttwriteresult* R=result(F);xmqttclientstatus Expected=!strncmp(Mode,"timeout_",8)?XMQTT_STATUS_TIMEOUT:!strcmp(Mode,"cancel_ack")?XMQTT_STATUS_CANCELLED:!strncmp(Mode,"abort_",6)?XMQTT_STATUS_CLOSED:XMQTT_STATUS_OK;
        if(R->Status!=Expected)fprintf(stderr,"mode=%s status=%d bytes=%zu\n",Mode,R->Status,R->AcceptedBytes);
        check(R->Status==Expected && R->Reason==D.Reason && !R->Identifier && !R->Puback,"graceful typed outcome");
        if(!strcmp(Mode,"abort_prefix"))check(R->AcceptedBytes>0 && R->AcceptedBytes<WireSize,"accepted partial final frame");
        if(Published){const xmqttwriteresult* P=result(Published);check(P->Status==Expected,"prior work outcome");}}
Cleanup:
    check(xmqttClientWaitFor(C,8000)==XWAIT_OK,"physical close");xmqttclientinfo I;xmqttClientInfo(C,&I);check(!I.Requests && !I.OutgoingBytes && I.Accepted==I.Completed,"retired accounting");
    printf("{\"mode\":\"%s\",\"version\":%u,\"bytes\":%zu,\"status\":%d,\"timed_out\":%u}\n",Mode,V,I.DisconnectBytes,I.CloseStatus,I.DrainTimedOut?1u:0u);
    check(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"destruction");if(F)xrtFutureDestroy(F);xrtFutureDestroy(Published);xrtFutureDestroy(Ready);return 0;
}
