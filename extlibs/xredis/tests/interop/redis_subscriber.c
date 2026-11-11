/* Share public trust/factory/lifetime test helpers, not a reference client's implementation. */
#define INTEROP_TLS
#define main subscriberInitializationReference
#include "redis_init.c"
#undef main
#include <xredis/subscriber.h>
#include <xredis/subscriber_notify.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
static xfuture* SubCloseHints[2];
static void subscriberHintResult(xfuture* F)
{xfutureresult R;interopRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && !R.Value && !R.Error,"real shared readability signal");}
#endif
static xredisreply* subscriberTake(xredisclient* C)
{
    xredisreply* R=NULL;
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    size_t Taken=0;
    while(!xredisSubscriberTakeBatch(C,&R,1,&Taken)){xfuture* F=NULL;interopRequire(xredisSubscriberReadable(C,&F)==XREDIS_ADMISSION_ACCEPTED,"real message-only readiness");subscriberHintResult(F);xrtFutureDestroy(F);}
#else
    double End=xrtTimer()+10;while(!xredisClientTakePush(C,&R)){interopRequire(xrtTimer()<End,"real message deadline");xrtThreadYield();}
#endif
    return R;
}
static unsigned SubMode,SubTLS,SubAffinity;
static int SubAuth;
static void subscriberIdle(xredisclient* C)
{double End=xrtTimer()+10;for(;;){xredisclientinfo I={0};I.Size=sizeof(I);interopRequire(xredisClientInfo(C,&I),"idle snapshot");if(!I.Requests)return;interopRequire(xrtTimer()<End,"physical idle deadline");xrtThreadYield();}}
static xredisclient* subscriberConnect(xnetengine* Engine,const xnetaddr* A,xredisinitmode Mode,cstr Run)
{
    xredisclientinitconfig Init;interopOptions(&Init,SubAuth);Init.Mode=Mode;xredisclientconfig K;xredisClientConfigInit(&K);K.WorkRequests=1;K.ReadBytes=3;K.ReadFrames=1;
    xredisclient* C=NULL;bool OK;
    if(SubMode==2){xnetaddrlist* L=xrtNetAddrListCreate(A,1);interopRequire(L!=NULL,"prepared address list");
        OK=SubTLS?xredisClientTlsDialAddresses(Engine,L,SubAffinity++%2,&K,&Init,&InteropTLS,&C):xredisClientDialAddresses(Engine,L,SubAffinity++%2,&K,&Init,NULL,&C);xrtNetAddrListDestroy(L);
    }else if(SubMode==1)OK=SubTLS?xredisClientTlsDial(Engine,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,SubAffinity++%2,&K,&Init,&InteropTLS,&C):xredisClientDial(Engine,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,SubAffinity++%2,&K,&Init,NULL,&C);
    else OK=SubTLS?xredisClientTlsConnect(Engine,A,SubAffinity++%2,&K,&Init,&InteropTLS,&C):xredisClientConnectInit(Engine,A,SubAffinity++%2,&K,&Init,&C);
    interopRequire(OK,"subscriber transport factory");xfuture* F=xredisClientReady(C);const xredisreadyresult* R=interopWait(F);
    interopRequire(R->Status==XREDIS_READY_OK && R->Protocol==(Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3),"actual RESP readiness");xrtFutureDestroy(F);
    xbytesview Identify[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};F=interopCall(C,Identify,2);xredisreplyvalue V=interopValue(F);bool Match=false;
    for(size_t N=0;N+47<=V.Data.Size;++N)if(!memcmp(V.Data.Data+N,"run_id:",7) && !memcmp(V.Data.Data+N+7,Run,40)){Match=true;break;}
    interopRequire(Match,"owned disposable server identity before subscription or ACL mutation");xrtFutureDestroy(F);subscriberIdle(C);return C;
}
static xfuture* subscriberSubmit(xredisclient* C,xredispubsubcommand Command,const xbytesview* Names,size_t Count)
{xfuture* F=NULL;interopRequire(xredisSubscriberCommandAsync(C,Command,Names,Count,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"native subscriber admission");return F;}
static void subscriberOutcome(xfuture* F,size_t Acks,xredisrequeststatus Status)
{const xredissubscriberresult* R=interopWait(F);if(R->Request.Status!=Status)fprintf(stderr,"subscriber outcome status=%d expected=%d ack=%zu\n",R->Request.Status,Status,R->Acknowledged);interopRequire(R->Request.Status==Status && R->Request.Delivery==XREDIS_REPLIED && R->Acknowledged==Acks,"native subscriber exact outcome");}
static bool subscriberSame(xbytesview A,xbytesview B)
{return A.Size==B.Size && (!A.Size || !memcmp(A.Data,B.Data,A.Size));}
static xredisreply* subscriberMessage(xredisclient* C,xredispubsubkind Kind,xbytesview Channel,xbytesview Pattern,xbytesview Payload)
{
    xredisreply* R=subscriberTake(C);
    xredispubsubevent E;interopRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && E.Kind==Kind && subscriberSame(E.Channel,Channel) && subscriberSame(E.Pattern,Pattern) && subscriberSame(E.Payload,Payload),"real binary message/pattern/shard fields");return R;
}
static void subscriberPublish(xredisclient* Writer,bool Shard,xbytesview Channel,xbytesview Payload)
{xbytesview Args[]={Shard?(xbytesview)XRT_BYTES_LITERAL("SPUBLISH"):(xbytesview)XRT_BYTES_LITERAL("PUBLISH"),Channel,Payload};xfuture* F=interopCall(Writer,Args,3);xrtFutureDestroy(F);}
static xfuture* subscriberFlow(xnetengine* Engine,const xnetaddr* A,xredisinitmode Mode,cstr Run,xredisreply** Retained)
{
    xredisclient* C=subscriberConnect(Engine,A,Mode,Run);xredisclient* Writer=subscriberConnect(Engine,A,XREDIS_INIT_RESP3,Run);
    interopRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"real idle connection adoption");
    xbytesview Channels[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:\0c"),{NULL,0}},Patterns[]={XRT_BYTES_LITERAL("xs:*"),{NULL,0}},Shards[]={XRT_BYTES_LITERAL("xs:\0c"),XRT_BYTES_LITERAL("xs:s2")},Payload=XRT_BYTES_LITERAL("v\0\r\n");
    xfuture* F[3];F[0]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,Channels,3);F[1]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_PSUBSCRIBE,Patterns,2);F[2]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_SSUBSCRIBE,Shards,2);
    for(size_t N=0;N<3;++N){subscriberOutcome(F[N],N?2:3,XREDIS_REQUEST_OK);xrtFutureDestroy(F[N]);}
    xredissubscriberinfo Info={0};Info.Size=sizeof(Info);interopRequire(xredisSubscriberInfo(C,&Info) && Info.Subscriptions==4 && Info.ShardSubscriptions==2 && Info.Entries==6,"separate real count domains and unique names");
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    xfuture* Pending=NULL;interopRequire(xredisSubscriberReadable(C,&Pending)==XREDIS_ADMISSION_ACCEPTED && !xrtFutureDone(Pending),"real pre-publication pending epoch");
#endif
    subscriberPublish(Writer,false,Channels[0],Payload);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    subscriberHintResult(Pending);xrtFutureDestroy(Pending);
#endif
    /* Direct and pattern deliveries have no specified cross-family order. */
    bool Direct=false,Pattern=false;
    for(unsigned N=0;N<2;++N){xredisreply* R=subscriberTake(C);
        xredispubsubevent E;interopRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && subscriberSame(E.Channel,Channels[0]) && subscriberSame(E.Payload,Payload),"exact binary delivery");
        if(E.Kind==XREDIS_PUBSUB_MESSAGE){interopRequire(!Direct,"one direct delivery");Direct=true;if(!*Retained)*Retained=R;else xredisReplyRelease(R);}
        else{interopRequire(E.Kind==XREDIS_PUBSUB_PMESSAGE && !Pattern && subscriberSame(E.Pattern,Patterns[0]),"one matching pattern");Pattern=true;xredisReplyRelease(R);}
    }
    interopRequire(Direct && Pattern,"both real delivery families");subscriberPublish(Writer,true,Shards[0],Payload);
    xredisreply* Message=subscriberMessage(C,XREDIS_PUBSUB_SMESSAGE,Shards[0],(xbytesview){NULL,0},Payload);xredisReplyRelease(Message);
    subscriberPublish(Writer,false,Channels[2],Payload);Direct=Pattern=false;
    for(unsigned N=0;N<2;++N){Message=subscriberTake(C);
        xredispubsubevent E;interopRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && !E.Channel.Size && subscriberSame(E.Payload,Payload),"empty-name message bytes");
        if(E.Kind==XREDIS_PUBSUB_MESSAGE){interopRequire(!Direct,"one empty direct");Direct=true;}else{interopRequire(E.Kind==XREDIS_PUBSUB_PMESSAGE && !Pattern && !E.Pattern.Size,"empty pattern");Pattern=true;}xredisReplyRelease(Message);
    }
    F[0]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_PING,NULL,0);F[1]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_PING,&Channels[2],1);F[2]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_PING,&Payload,1);
    for(unsigned N=0;N<3;++N){subscriberOutcome(F[N],0,XREDIS_REQUEST_OK);xrtFutureDestroy(F[N]);}
    xbytesview Remove[]={Channels[0],Channels[0],XRT_BYTES_LITERAL("unknown")};xfuture* U[7]={0};
    const xredispubsubcommand Commands[]={XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PUNSUBSCRIBE,XREDIS_PUBSUB_CMD_SUNSUBSCRIBE,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PUNSUBSCRIBE,XREDIS_PUBSUB_CMD_SUNSUBSCRIBE};size_t Counts[]={3,1,2,2,1,1,1};
    for(unsigned N=0;N<7;++N)U[N]=subscriberSubmit(C,Commands[N],N?NULL:Remove,N?0:3);
    for(unsigned N=0;N<7;++N){subscriberOutcome(U[N],Counts[N],XREDIS_REQUEST_OK);xrtFutureDestroy(U[N]);}
    subscriberIdle(C);interopRequire(xredisSubscriberInfo(C,&Info) && !Info.Entries && !Info.Subscriptions && !Info.ShardSubscriptions,"all real names retired including null ACKs");
    F[0]=subscriberSubmit(C,XREDIS_PUBSUB_CMD_PING,NULL,0);subscriberOutcome(F[0],0,XREDIS_REQUEST_OK);xrtFutureDestroy(F[0]);
    /* Owned server identity was checked before changing only this fixture ACL. */
    xbytesview User=SubAuth==2?(xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME):(xbytesview)XRT_BYTES_LITERAL("default");
    xbytesview ACL[]={XRT_BYTES_LITERAL("ACL"),XRT_BYTES_LITERAL("SETUSER"),User,XRT_BYTES_LITERAL("resetchannels"),XRT_BYTES_LITERAL("&xs:*")};
    F[0]=interopCall(Writer,ACL,5);xrtFutureDestroy(F[0]);xbytesview Denied=XRT_BYTES_LITERAL("denied");xfuture* Error=subscriberSubmit(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Denied,1);subscriberOutcome(Error,0,XREDIS_REQUEST_SERVER_ERROR);
    ACL[4]=(xbytesview)XRT_BYTES_LITERAL("&*");F[0]=interopCall(Writer,ACL,5);xrtFutureDestroy(F[0]);subscriberIdle(C);interopRequire(xredisSubscriberInfo(C,&Info) && !Info.Entries,"server rejection releases persistent-name reservation");
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    unsigned Slot=Mode==XREDIS_INIT_RESP2?0:1;interopRequire(xredisSubscriberReadable(C,&SubCloseHints[Slot])==XREDIS_ADMISSION_ACCEPTED && !xrtFutureDone(SubCloseHints[Slot]),"real pending physical-close epoch");
#endif
    interopClose(C);interopClose(Writer);return Error;
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
    xfuture* Errors[2];Errors[0]=subscriberFlow(Engine,&A,XREDIS_INIT_RESP2,Argv[6],&Retained);Errors[1]=subscriberFlow(Engine,&A,XREDIS_INIT_RESP3,Argv[6],&Retained);
    interopRequire(xrtNetResolverDestroy(InteropResolver) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"all native physical owners retired");
    xredispubsubevent E;interopRequire(Retained && xredisPubSubEventRead(Retained,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==4,"owned message survives every transport and Engine");xredisReplyRelease(Retained);
    for(unsigned N=0;N<2;++N){subscriberOutcome(Errors[N],0,XREDIS_REQUEST_SERVER_ERROR);xrtFutureDestroy(Errors[N]);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
        subscriberHintResult(SubCloseHints[N]);xrtFutureDestroy(SubCloseHints[N]);
#endif
    }
    if(Trust)xrtTlsVerifierRelease(Trust);
    if(Context)xrtTlsContextRelease(Context);
    printf("[PASS] real native subscriber RESP2/3: binary/empty/duplicate patterns and shards, publish sets, pipelined ACKs/counts, unsub-all/null/PONG, ACL error and retained owners; mode=%u TLS=%u auth=%d\n",SubMode,SubTLS,SubAuth);return 0;
}
