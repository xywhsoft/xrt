#define XREDIS_SUBSCRIBER_INTEROP_MAIN subscriptionsNativeReference
#include "redis_subscriber.c"
#undef XREDIS_SUBSCRIBER_INTEROP_MAIN
#include <xredis/subscriptions.h>

static xredissubscriptionreplayresult subscriptionsDrive(xredissubscriptionreplay* R)
{
    double End=xrtTimer()+20;
    for(;;){xredissubscriptionstep Step=xredisSubscriptionReplayStep(R);interopRequire(Step!=XREDIS_SUBSCRIPTIONS_STEP_ERROR && xrtTimer()<End,"bounded replay turn");
        if(Step==XREDIS_SUBSCRIPTIONS_DONE)break;
        xfuture* F=xredisSubscriptionReplayPending(R);if(F){(void)interopWait(F);xrtFutureDestroy(F);}else xrtThreadYield();}
    xredissubscriptionreplayresult V={0};V.Size=sizeof(V);interopRequire(xredisSubscriptionReplayResult(R,&V),"replay result");return V;
}
static void subscriptionsEdit(xredissubscriptions* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision)
{interopRequire(xredisSubscriptionsApply(S,Command,Names,Count,Revision)==XREDIS_ADMISSION_ACCEPTED,"desired edit");}
static xredissubscriptionreplay* subscriptionsBegin(xredissubscriptions* S,xredisclient* C)
{
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=2;K.MaxBatchBytes=128;K.Timeout=10000;
    xredissubscriptionreplay* R=NULL;interopRequire(xredisSubscriptionReplayCreate(S,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"fresh replacement claim");return R;
}
static xredisreply* subscriptionsFlow(xnetengine* Engine,const xnetaddr* A,xredisinitmode Mode,cstr Run,xredisreply** Retained)
{
    xredissubscriptions* S=NULL;interopRequire(xredisSubscriptionsCreate(NULL,&S),"desired set");uint64 Revision=0;
    xbytesview Channels[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:\0c"),{NULL,0}},Patterns[]={XRT_BYTES_LITERAL("xs:*"),{NULL,0}},Shards[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:s2")},Payload=XRT_BYTES_LITERAL("v\0\r\n");
    subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Channels,3,&Revision);subscriptionsEdit(S,XREDIS_PUBSUB_CMD_PSUBSCRIBE,Patterns,2,&Revision);subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SSUBSCRIBE,Shards,2,&Revision);
    xredissubscriptionsnapshot* Snapshot=NULL;interopRequire(xredisSubscriptionsSnapshot(S,&Snapshot),"retained original revision");
    xredisclient* C=subscriberConnect(Engine,A,Mode,Run);xredisclient* Writer=subscriberConnect(Engine,A,XREDIS_INIT_RESP3,Run);
    xredissubscriptionreplay* R=subscriptionsBegin(S,C);xredissubscriptionreplayresult V=subscriptionsDrive(R);
    interopRequire(V.Status==XREDIS_SUBSCRIPTIONS_OK && V.Revision==Revision && V.Acknowledged==6 && V.Subscriptions==4 && V.ShardSubscriptions==2,"first generation exact unique set and count domains");
    subscriberPublish(Writer,false,Channels[0],Payload);bool Direct=false,Pattern=false;
    for(unsigned N=0;N<2;++N){xredisreply* Message=subscriberTake(C);xredispubsubevent E;interopRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && subscriberSame(E.Channel,Channels[0]) && subscriberSame(E.Payload,Payload),"restored binary delivery");
        if(E.Kind==XREDIS_PUBSUB_MESSAGE){interopRequire(!Direct,"one direct");Direct=true;if(!*Retained)*Retained=Message;else xredisReplyRelease(Message);}
        else{interopRequire(E.Kind==XREDIS_PUBSUB_PMESSAGE && !Pattern && subscriberSame(E.Pattern,Patterns[0]),"one pattern");Pattern=true;xredisReplyRelease(Message);}}
    interopRequire(Direct && Pattern,"both restored families");subscriberPublish(Writer,true,Shards[0],Payload);xredisreply* Message=subscriberMessage(C,XREDIS_PUBSUB_SMESSAGE,Shards[0],(xbytesview){NULL,0},Payload);xredisReplyRelease(Message);
    uint64 Original=Revision;subscriptionsEdit(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,Channels,1,&Revision);V=subscriptionsDrive(R);interopRequire(V.Status==XREDIS_SUBSCRIPTIONS_OK && V.Revision==Original,"terminal result immutable after desired edit");
    xredisSubscriptionReplayDestroy(R);interopClose(C);C=subscriberConnect(Engine,A,Mode,Run);R=subscriptionsBegin(S,C);V=subscriptionsDrive(R);
    interopRequire(V.Status==XREDIS_SUBSCRIPTIONS_OK && V.Revision==Revision && V.Acknowledged==5 && V.Subscriptions==3 && V.ShardSubscriptions==2,"second generation restores updated revision only");
    xbytesview NumSub[]={XRT_BYTES_LITERAL("PUBSUB"),XRT_BYTES_LITERAL("NUMSUB"),Channels[0]};double End=xrtTimer()+10;
    for(;;){xfuture* F=interopCall(Writer,NumSub,3);xredisreplyvalue Root=interopValue(F),Name,Count;const xredisrequestresult* Q=interopWait(F);
        interopRequire(Root.Type==XREDIS_REPLY_ARRAY && xredisReplyGet(Q->Reply,Root.FirstChild,&Name) && xredisReplyGet(Q->Reply,Name.Next,&Count) && Count.Type==XREDIS_REPLY_INTEGER,"NUMSUB exact tuple");bool Gone=Count.Integer==0;xrtFutureDestroy(F);if(Gone)break;interopRequire(xrtTimer()<End,"removed desired channel absent remotely");xrtThreadYield();}
    xredissubscriptionsinfo I={0};I.Size=sizeof(I);interopRequire(xredisSubscriptionSnapshotInfo(Snapshot,&I) && I.Revision==Original && I.Entries==6,"old snapshot unaffected by edits/replacement");
    xredisSubscriptionReplayDestroy(R);interopClose(C);xredisSubscriptionSnapshotRelease(Snapshot);xredisSubscriptionsRelease(S);
    /* The disposable server run_id was verified before this fixture-only ACL change. */
    C=subscriberConnect(Engine,A,Mode,Run);xbytesview User=SubAuth==2?(xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME):(xbytesview)XRT_BYTES_LITERAL("default");
    xbytesview ACL[]={XRT_BYTES_LITERAL("ACL"),XRT_BYTES_LITERAL("SETUSER"),User,XRT_BYTES_LITERAL("resetchannels"),XRT_BYTES_LITERAL("&xs:*")};xfuture* F=interopCall(Writer,ACL,5);xrtFutureDestroy(F);
    interopRequire(xredisSubscriptionsCreate(NULL,&S),"denied desired set");xbytesview Denied=XRT_BYTES_LITERAL("denied");subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Denied,1,&Revision);R=subscriptionsBegin(S,C);V=subscriptionsDrive(R);
    interopRequire(V.Status==XREDIS_SUBSCRIPTIONS_SERVER_ERROR && V.NativeStatus==XREDIS_REQUEST_SERVER_ERROR && V.ErrorReply && xredisReplyRetain(V.ErrorReply),"typed replay ACL failure");xredisreply* Error=V.ErrorReply;xredisSubscriptionReplayDestroy(R);xredisSubscriptionsRelease(S);
    ACL[4]=(xbytesview)XRT_BYTES_LITERAL("&*");F=interopCall(Writer,ACL,5);xrtFutureDestroy(F);interopClose(C);interopClose(Writer);return Error;
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
    xredisreply* Errors[2];Errors[0]=subscriptionsFlow(Engine,&A,XREDIS_INIT_RESP2,Argv[6],&Retained);Errors[1]=subscriptionsFlow(Engine,&A,XREDIS_INIT_RESP3,Argv[6],&Retained);
    interopRequire(xrtNetResolverDestroy(InteropResolver) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"all native physical owners retired");
    xredispubsubevent E;interopRequire(Retained && xredisPubSubEventRead(Retained,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==4,"owned message survives every transport and Engine");xredisReplyRelease(Retained);
    for(unsigned N=0;N<2;++N){xredisreplyvalue Error;interopRequire(xredisReplyGet(Errors[N],0,&Error) && (Error.Type==XREDIS_REPLY_SIMPLE_ERROR || Error.Type==XREDIS_REPLY_BULK_ERROR) && Error.Data.Size>0,"retained error survives all physical owners");xredisReplyRelease(Errors[N]);}
    if(Trust)xrtTlsVerifierRelease(Trust);
    if(Context)xrtTlsContextRelease(Context);
    printf("[PASS] real desired replay RESP2/3: two fresh generations, binary/empty/duplicate sets, exact counts, removed intent, snapshot and ACL error ownership; mode=%u TLS=%u auth=%d\n",SubMode,SubTLS,SubAuth);return 0;
}
