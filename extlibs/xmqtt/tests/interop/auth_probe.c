/* Public client process driven by an independent Python socket peer. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct authProbe {cstr Case;xatomic32 Reauth,Checks;xatomicptr Held;} authProbe;
static void apCheck(bool B,cstr Message)
{if(!B){fprintf(stderr,"[FAIL] auth probe: %s (%s)\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static xmqttpacket apPacket(xmqttpackettype Type,uint8 Reason,xmqttproperty Props[3],bool Problem)
{
    static const unsigned char Token[]={'a',0,255,'z'};xmqttpacket P;xmqttPacketInit(Type,&P);P.Reason=Reason;
    Props[0]=(xmqttproperty){XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}};Props[0].Value.Text=XRT_STR_LITERAL("external");
    Props[1]=(xmqttproperty){XMQTT_PROPERTY_AUTH_DATA,XMQTT_PROPERTY_BINARY,{0}};Props[1].Value.Binary=(xbytesview){Token,sizeof(Token)};
    Props[2]=(xmqttproperty){XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,XMQTT_PROPERTY_BYTE,{0}};
    P.Properties=(xmqttpropertylist){Props,Problem?3u:2u};
    if(Type==XMQTT_PACKET_CONNECT){P.Body.Connect.CleanStart=true;P.Body.Connect.ClientId=XRT_STR_LITERAL("auth-probe");}return P;
}
static void apNotify(xmqttclient* C,ptr User)
{
    authProbe* A=User;bool Reauth=xrtAtomic32Load(&A->Reauth,XMEMORY_ACQUIRE)!=0;
    if(!strcmp(A->Case,Reauth?"reauth_timeout":"initial_timeout"))return;
    xmqttauthticket* T=NULL;
    while(xmqttClientTakeAuth(C,&T)){
        xmqttproperty Props[3];xmqttpacket P=apPacket(XMQTT_PACKET_AUTH,0x18,Props,false);xfuture* F=(xfuture*)(uintptr_t)1;
        P.Reason=0x19;
        apCheck(xmqttClientReauthenticate(C,&P,1,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1 && xrtErrorKind(xrtGetError())==XERR_STATE,"same-Engine blocking restriction with otherwise valid packet");xrtClearError();P.Reason=0x18;
        const xmqttpacket* Q=xmqttMessagePacket(xmqttAuthMessage(T));bool Proof=false;
        const unsigned char Expected[]={'p',0,255,'f'};
        for(size_t I=0;I<Q->Properties.Count;++I)if(Q->Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_DATA){xbytesview V=Q->Properties.Data[I].Value.Binary;Proof=V.Size==sizeof(Expected) && !memcmp(V.Data,Expected,sizeof(Expected));}
        apCheck(Proof,"server binary challenge/final proof snapshot");
        if(!Reauth && ((!strcmp(A->Case,"initial_keepalive") && xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE) ||
            (!strcmp(A->Case,"verify_keepalive") && xmqttAuthPhase(T)==XMQTT_AUTH_VERIFY_CONNECT))){
            apCheck(!xrtAtomicPtrLoad(&A->Held,XMEMORY_ACQUIRE),"one offloaded decision");xrtAtomicPtrStore(&A->Held,T,XMEMORY_RELEASE);return;}
        if(!strcmp(A->Case,Reauth?"local_reject_reauth":"local_reject_initial") && xmqttAuthPhase(T)!=XMQTT_AUTH_CHALLENGE)apCheck(xmqttAuthComplete(T,false)==XMQTT_ADMISSION_ACCEPTED,"local proof rejection");
        else if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE)apCheck(xmqttAuthRespond(T,&P)==XMQTT_ADMISSION_ACCEPTED,"AUTH continuation");
        else apCheck(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"local final proof");
        xmqttAuthRelease(T);xrtAtomic32FetchAdd(&A->Checks,1,XMEMORY_RELEASE);
    }
}
static const void* apResult(xfuture* F)
{apCheck(F && xrtFutureWaitFor(F,5000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed Future result");return xrtFutureValue(F);}
int main(int Argc,char** Argv)
{
    apCheck(Argc==3,"usage: auth_probe PORT CASE");authProbe A={0};A.Case=Argv[2];xrtAtomic32Init(&A.Reauth,0);xrtAtomic32Init(&A.Checks,0);xrtAtomicPtrInit(&A.Held,NULL);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);apCheck(Engine && xrtNetEngineStart(Engine),"engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=apNotify;K.User=&A;K.SetupTimeout=!strcmp(A.Case,"initial_timeout")?150:3000;K.InitialInputBytes=5;
    bool InitialKeep=!strcmp(A.Case,"initial_keepalive"),VerifyKeep=!strcmp(A.Case,"verify_keepalive");
    if(InitialKeep || VerifyKeep){K.SetupTimeout=5000;K.PingTimeout=500;}
    if(!strcmp(A.Case,"tiny")){K.WriteBytes=K.WriteChunkBytes=K.ReadBytes=1;K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
    xmqttproperty Props[3];xmqttpacket P=apPacket(XMQTT_PACKET_CONNECT,0,Props,!strncmp(A.Case,"problem_",8));xnetaddr Address;xmqttclient* C=NULL;
    if(InitialKeep){P.Body.Connect.KeepAlive=1;K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
    apCheck(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnectAuth(Engine,&Address,1,&K,NULL,&P,&C),"factory");xfuture* Ready=xmqttClientReady(C);
    if(InitialKeep || VerifyKeep){double End=xrtTimer()+3;xmqttauthticket* T=NULL;
        while(!(T=xrtAtomicPtrLoad(&A.Held,XMEMORY_ACQUIRE))){apCheck(xrtTimer()<End,"offloaded auth decision");xrtSleep(1);}
        xrtSleep(InitialKeep?2200:1200);apCheck(xrtFutureState(Ready)==XFUTURE_PENDING,"IO heartbeats progress without accepting local proof");
        P=apPacket(XMQTT_PACKET_AUTH,0x18,Props,false);apCheck((InitialKeep?xmqttAuthRespond(T,&P):xmqttAuthComplete(T,true))==XMQTT_ADMISSION_ACCEPTED,"offloaded decision remains live across Keep Alive");xmqttAuthRelease(T);
    }
    xmqttclientstatus Want=!strcmp(A.Case,"initial_timeout")?XMQTT_STATUS_TIMEOUT:!strcmp(A.Case,"initial_reject")?XMQTT_STATUS_REJECTED:!strcmp(A.Case,"initial_eof")?XMQTT_STATUS_TRANSPORT:!strcmp(A.Case,"local_reject_initial")?XMQTT_STATUS_AUTHENTICATION:((!InitialKeep && !strncmp(A.Case,"initial_",8)) || !strcmp(A.Case,"problem_reason"))?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_OK;
    const xmqttreadyresult* R=apResult(Ready);apCheck(R->Status==Want,"initial expected status");xfuture* Again=NULL;
    if(Want==XMQTT_STATUS_OK){xrtAtomic32Store(&A.Reauth,1,XMEMORY_RELEASE);P=apPacket(XMQTT_PACKET_AUTH,0x19,Props,false);
        apCheck(xmqttClientReauthenticateAsync(C,&P,!strcmp(A.Case,"reauth_timeout")?150:3000,&Again)==XMQTT_ADMISSION_ACCEPTED,"reauth admission");const xmqttauthresult* Q=apResult(Again);
        Want=!strcmp(A.Case,"reauth_timeout")?XMQTT_STATUS_TIMEOUT:!strcmp(A.Case,"local_reject_reauth")?XMQTT_STATUS_AUTHENTICATION:!strcmp(A.Case,"reauth_disconnect")?XMQTT_STATUS_SERVER_DISCONNECT:!strncmp(A.Case,"reauth_",7)?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_OK;
        apCheck(Q->Status==Want && Q->AcceptedBytes,"reauth typed expected status/accepted prefix");
        if(Want==XMQTT_STATUS_OK){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("probe");P.Body.Publish.Payload=XRT_BYTES_LITERAL("out");xfuture* Written=NULL;
            apCheck(xmqttClientPublish(C,&P,3000,&Written)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)apResult(Written))->Status==XMQTT_STATUS_OK,"post-auth publication");
            bool Seen=false;double End=xrtTimer()+3;while(!Seen){xmqttmessage* M=NULL;if(xmqttClientTakeMessage(C,&M)){const xmqttpacket* V=xmqttMessagePacket(M);Seen=V->Body.Publish.Payload.Size==3 && !memcmp(V->Body.Publish.Payload.Data,"out",3);xmqttMessageRelease(M);}else xrtSleep(1);apCheck(xrtTimer()<End,"independent peer observed publication");}xrtFutureDestroy(Written);
        }
    }
    if(Want!=XMQTT_STATUS_OK)apCheck(xmqttClientWaitFor(C,5000)==XWAIT_OK,"failed exchange naturally retires");
    apCheck(xmqttClientClose(C) && xmqttClientWaitFor(C,5000)==XWAIT_OK && xmqttClientDestroy(C),"physical Client retirement");apCheck(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"engine retirement");
    if(R->Status==XMQTT_STATUS_OK)apCheck(R->Negotiated.AuthMethod.Size==8 && !memcmp(R->Negotiated.AuthMethod.Data,"external",8),"retained negotiated method");
    xrtFutureDestroy(Again);xrtFutureDestroy(Ready);printf("PASS auth %s\n",A.Case);return 0;
}
