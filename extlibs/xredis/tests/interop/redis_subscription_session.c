#define XREDIS_SUBSCRIBER_INTEROP_MAIN sessionNativeReference
#include "redis_subscriber.c"
#undef XREDIS_SUBSCRIBER_INTEROP_MAIN
#include <xredis/subscription_session.h>
typedef struct sessioninterop {
    xnetaddr Address;xredisinitmode Mode;uint64 Attempts;unsigned Drops;xfuture* Closed[8];
} sessioninterop;
static bool sessionInteropFactory(xnetengine* E,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** C)
{
    sessioninterop* F=Data;interopRequire(xrtNetEngineCurrent(E)!=NULL && Attempt==++F->Attempts && Attempt<=8,"serialized bounded interop factory");
    if(Attempt>1)interopRequire(xrtFutureDone(F->Closed[Attempt-2]),"new attempt follows native physical retirement");
    xredisclientinitconfig Init;interopOptions(&Init,SubAuth);Init.Mode=F->Mode;Init.Timeout=Timeout;xredisclientconfig K;xredisClientConfigInit(&K);K.WorkRequests=1;K.ReadBytes=3;K.ReadFrames=1;bool OK;
    if(SubMode==2){xnetaddrlist* L=xrtNetAddrListCreate(&F->Address,1);if(!L)return false;OK=SubTLS?xredisClientTlsDialAddresses(E,L,Attempt%2,&K,&Init,&InteropTLS,C):xredisClientDialAddresses(E,L,Attempt%2,&K,&Init,NULL,C);xrtNetAddrListDestroy(L);}
    else if(SubMode==1)OK=SubTLS?xredisClientTlsDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),F->Address.Port,Attempt%2,&K,&Init,&InteropTLS,C):xredisClientDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),F->Address.Port,Attempt%2,&K,&Init,NULL,C);
    else OK=SubTLS?xredisClientTlsConnect(E,&F->Address,Attempt%2,&K,&Init,&InteropTLS,C):xredisClientConnectInit(E,&F->Address,Attempt%2,&K,&Init,C);
    if(OK)F->Closed[Attempt-1]=xredisClientClosed(*C);
    return OK;
}
static void sessionInteropDrop(ptr Data)
{sessioninterop* F=Data;for(uint64 N=0;N<F->Attempts;++N)if(F->Closed[N]){interopRequire(xrtFutureDone(F->Closed[N]),"interop context drop follows native physical close");xrtFutureDestroy(F->Closed[N]);F->Closed[N]=NULL;}++F->Drops;}
static xredissubscriptionsessioncurrent sessionInteropCurrent(xredissubscriptionsession* S,uint64 Generation,uint64 Revision)
{
    double End=xrtTimer()+25;for(;;){xredissubscriptionsessioncurrent C;xredisadmission A=xredisSubscriptionSessionCurrent(S,&C);if(A==XREDIS_ADMISSION_ACCEPTED){if(C.Generation>=Generation && C.Revision>=Revision)return C;xredisClientRelease(C.Client);}else interopRequire(A==XREDIS_ADMISSION_OVERLOADED,"interop pending current");interopRequire(xrtTimer()<End,"autonomous current deadline");xrtThreadYield();}
}
static xredissubscriptionsession* sessionInteropCreate(xnetengine* E,xredissubscriptions* Store,sessioninterop* F)
{xredissubscriptionsessionconfig K;xredisSubscriptionSessionConfigInit(&K);K.Connect=sessionInteropFactory;K.Drop=sessionInteropDrop;K.Data=F;K.Protocol=F->Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3;K.AttemptTimeout=10000;K.RetryMin=K.RetryMax=10;K.Replay.MaxBatchNames=2;xredissubscriptionsession* S=NULL;interopRequire(xredisSubscriptionSessionCreate(E,0,Store,&K,&S)==XREDIS_ADMISSION_ACCEPTED,"interop controller start");return S;}
static void sessionInteropMessages(xredisclient* C,xbytesview Channel,xbytesview Payload,xredisreply** Retained)
{
    bool Direct=false,Pattern=false;for(unsigned N=0;N<2;++N){xredisreply* R=subscriberTake(C);xredispubsubevent E;interopRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && subscriberSame(E.Channel,Channel) && subscriberSame(E.Payload,Payload),"real live/recovered binary delivery");if(E.Kind==XREDIS_PUBSUB_MESSAGE){interopRequire(!Direct,"one direct");Direct=true;if(!*Retained)*Retained=R;else xredisReplyRelease(R);}else{interopRequire(E.Kind==XREDIS_PUBSUB_PMESSAGE && !Pattern && E.Pattern.Size==4 && !memcmp(E.Pattern.Data,"xs:*",4),"one matching pattern");Pattern=true;xredisReplyRelease(R);}}interopRequire(Direct && Pattern,"direct and pattern after controller restore");
}
static void sessionInteropNumSub(xredisclient* Writer,xbytesview Channel,int64 Expected)
{
    xbytesview Args[]={XRT_BYTES_LITERAL("PUBSUB"),XRT_BYTES_LITERAL("NUMSUB"),Channel};xfuture* F=interopCall(Writer,Args,3);xredisreplyvalue Root=interopValue(F),Name,Count;const xredisrequestresult* R=interopWait(F);interopRequire(Root.Type==XREDIS_REPLY_ARRAY && xredisReplyGet(R->Reply,Root.FirstChild,&Name) && xredisReplyGet(R->Reply,Name.Next,&Count) && Count.Type==XREDIS_REPLY_INTEGER && Count.Integer==Expected,"exact remote desired membership");xrtFutureDestroy(F);
}
static xfuture* sessionInteropFlow(xnetengine* E,const xnetaddr* A,xredisinitmode Mode,cstr Run,xredisreply** Retained)
{
    /* Verify the runner's owned run identity before any fixture ACL/KILL action. */
    xredisclient* Writer=subscriberConnect(E,A,XREDIS_INIT_RESP3,Run);sessioninterop F={0};F.Address=*A;F.Mode=Mode;xredissubscriptions* Store=NULL;interopRequire(xredisSubscriptionsCreate(NULL,&Store),"interop desired store");uint64 Revision=0;
    xbytesview Channels[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:\0c"),{NULL,0}},Patterns[]={XRT_BYTES_LITERAL("xs:*"),{NULL,0}},Shards[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:s2")},Payload=XRT_BYTES_LITERAL("v\0\r\n"),New=XRT_BYTES_LITERAL("xs:new\0c");
    interopRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Channels,3,&Revision)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_PSUBSCRIBE,Patterns,2,&Revision)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SSUBSCRIBE,Shards,2,&Revision)==XREDIS_ADMISSION_ACCEPTED,"three desired families");
    xredissubscriptionsession* S=sessionInteropCreate(E,Store,&F);xredissubscriptionsessioncurrent First=sessionInteropCurrent(S,1,Revision);subscriberPublish(Writer,false,Channels[0],Payload);sessionInteropMessages(First.Client,Channels[0],Payload,Retained);
    interopRequire(xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,Channels,1,&Revision)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionSessionApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&New,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"live binary desired delta");xredissubscriptionsessioncurrent Current=sessionInteropCurrent(S,1,Revision);interopRequire(Current.Client==First.Client && Current.Generation==1,"live diff retains original connection");sessionInteropNumSub(Writer,Channels[0],0);sessionInteropNumSub(Writer,New,1);sessionInteropNumSub(Writer,Channels[2],1);subscriberPublish(Writer,false,New,Payload);sessionInteropMessages(Current.Client,New,Payload,Retained);xredisClientRelease(Current.Client);
    xbytesview Kill[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("KILL"),XRT_BYTES_LITERAL("TYPE"),XRT_BYTES_LITERAL("PUBSUB"),XRT_BYTES_LITERAL("SKIPME"),XRT_BYTES_LITERAL("YES")};xfuture* Killed=interopCall(Writer,Kill,6);xredisreplyvalue K=interopValue(Killed);interopRequire(K.Type==XREDIS_REPLY_INTEGER && K.Integer==1,"owned server kills exactly this fixture subscriber");xrtFutureDestroy(Killed);
    Current=sessionInteropCurrent(S,2,Revision);interopRequire(Current.Client!=First.Client && Current.Generation==2,"automatic server-disconnect replacement");xredissubscriberinfo I={0};I.Size=sizeof(I);interopRequire(xredisSubscriberInfo(Current.Client,&I) && I.Entries==6 && I.Subscriptions==4 && I.ShardSubscriptions==2,"replacement exact regular/shard sets");sessionInteropNumSub(Writer,Channels[0],0);sessionInteropNumSub(Writer,New,1);subscriberPublish(Writer,false,New,Payload);sessionInteropMessages(Current.Client,New,Payload,Retained);subscriberPublish(Writer,true,Shards[0],Payload);xredisreply* Message=subscriberMessage(Current.Client,XREDIS_PUBSUB_SMESSAGE,Shards[0],(xbytesview){NULL,0},Payload);xredisReplyRelease(Message);xredisClientRelease(Current.Client);xredisClientRelease(First.Client);
    xfuture* Closed=xredisSubscriptionSessionClosed(S);interopRequire(xredisSubscriptionSessionClose(S) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==XREDIS_REQUEST_OK && F.Drops==1 && F.Attempts==2,"explicit close after recovered failure and factory retirement");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);xrtFutureDestroy(Closed);
    xbytesview User=SubAuth==2?(xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME):(xbytesview)XRT_BYTES_LITERAL("default");xbytesview ACL[]={XRT_BYTES_LITERAL("ACL"),XRT_BYTES_LITERAL("SETUSER"),User,XRT_BYTES_LITERAL("resetchannels"),XRT_BYTES_LITERAL("&xs:*")};xfuture* R=interopCall(Writer,ACL,5);xrtFutureDestroy(R);
    sessioninterop Bad={0};Bad.Address=*A;Bad.Mode=Mode;interopRequire(xredisSubscriptionsCreate(NULL,&Store),"denied store");xbytesview Denied=XRT_BYTES_LITERAL("denied");interopRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Denied,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"denied intent");S=sessionInteropCreate(E,Store,&Bad);Closed=xredisSubscriptionSessionClosed(S);interopRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && ((const xredissubscriptionsessionclosed*)xrtFutureValue(Closed))->Status==XREDIS_REQUEST_SERVER_ERROR && Bad.Attempts==1 && Bad.Drops==1,"ACL failure is fatal without retry loop");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);ACL[4]=(xbytesview)XRT_BYTES_LITERAL("&*");R=interopCall(Writer,ACL,5);xrtFutureDestroy(R);interopClose(Writer);return Closed;
}
int main(int Argc,char** Argv)
{
    if(Argc!=7 || strlen(Argv[6])!=40)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);SubAuth=atoi(Argv[2]);SubMode=(unsigned)atoi(Argv[3]);SubTLS=(unsigned)atoi(Argv[4]);
    if(!Port || Port>65535 || SubAuth<0 || SubAuth>2 || SubMode>2 || (SubTLS!=0 && SubTLS!=12 && SubTLS!=13))return 2;
    xtlsverifier* Trust=NULL;xtlscontext* Context=NULL;
    if(SubTLS){Trust=interopTrust(Argv[5]);xtlspolicy P;xrtTlsPolicyInit(&P);xtlsversion Version=SubTLS==12?XTLS_VERSION_12:XTLS_VERSION_13;xtlscipher Cipher=SubTLS==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
        P.Versions=&Version;P.VersionCount=1;P.Ciphers=&Cipher;P.CipherCount=1;P.Signatures=&Signature;P.SignatureCount=1;xtlscontextconfig K;xrtTlsContextConfigInit(&K);K.Policy=&P;Context=xrtTlsContextCreate(&K);interopRequire(Context!=NULL,"native TLS policy");
        xredisClientTlsConfigInit(&InteropTLS);InteropTLS.Session.Context=Context;InteropTLS.Session.Verifier=Trust;InteropTLS.Session.ServerName=XRT_STR_LITERAL("localhost");InteropTLS.Session.VerifyName=XRT_STR_LITERAL("localhost");InteropTLS.WriteBytes=3;}
    xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&K);interopRequire(Engine && xrtNetEngineStart(Engine),"native subscriber Engine");InteropResolver=xrtNetResolverCreate(NULL);interopRequire(InteropResolver!=NULL,"native subscriber Resolver");
    xnetaddr A;interopRequire(xrtNetAddrParse(&A,"127.0.0.1",(uint16)Port),"loopback endpoint");xredisreply* Retained=NULL;
    xfuture* Errors[2];Errors[0]=sessionInteropFlow(Engine,&A,XREDIS_INIT_RESP2,Argv[6],&Retained);Errors[1]=sessionInteropFlow(Engine,&A,XREDIS_INIT_RESP3,Argv[6],&Retained);
    interopRequire(xrtNetResolverDestroy(InteropResolver) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"all native physical owners retired");
    xredispubsubevent E;interopRequire(Retained && xredisPubSubEventRead(Retained,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==4,"owned message survives every transport and Engine");xredisReplyRelease(Retained);
    for(unsigned N=0;N<2;++N){const xredissubscriptionsessionclosed* R=interopWait(Errors[N]);interopRequire(R->Status==XREDIS_REQUEST_SERVER_ERROR,"retained terminal outcome after Engine");xrtFutureDestroy(Errors[N]);}
    if(Trust)xrtTlsVerifierRelease(Trust);
    if(Context)xrtTlsContextRelease(Context);
    printf("[PASS] real autonomous session RESP2/3: live same-transport deltas, exact remote membership, server KILL and automatic restore, ACL fatal close and retained owners; mode=%u TLS=%u auth=%d\n",SubMode,SubTLS,SubAuth);return 0;
}
