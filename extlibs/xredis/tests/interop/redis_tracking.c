/* Reuse only public fixture credentials, trust loading and lifecycle helpers. */
#define INTEROP_TLS
#define main trackingInitializationReference
#include "redis_init.c"
#undef main
#include <xredis/tracking.h>
#include <inttypes.h>
static unsigned TrackingMode,TrackingVersion,TrackingAffinity;
static int TrackingAuth;
static xredisclient* trackingRealConnect(xnetengine* E,const xnetaddr* A,xredisinitmode Mode)
{
    xredisclientinitconfig Init;interopOptions(&Init,TrackingAuth);Init.Mode=Mode;xredisclient* C=NULL;bool OK;
    if(TrackingMode==2){xnetaddrlist* List=xrtNetAddrListCreate(A,1);interopRequire(List!=NULL,"prepared addresses");
        OK=TrackingVersion?xredisClientTlsDialAddresses(E,List,TrackingAffinity++%2,NULL,&Init,&InteropTLS,&C):xredisClientDialAddresses(E,List,TrackingAffinity++%2,NULL,&Init,NULL,&C);xrtNetAddrListDestroy(List);
    }else if(TrackingMode==1)OK=TrackingVersion?xredisClientTlsDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,TrackingAffinity++%2,NULL,&Init,&InteropTLS,&C):xredisClientDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,TrackingAffinity++%2,NULL,&Init,NULL,&C);
    else OK=TrackingVersion?xredisClientTlsConnect(E,A,TrackingAffinity++%2,NULL,&Init,&InteropTLS,&C):xredisClientConnectInit(E,A,TrackingAffinity++%2,NULL,&Init,&C);
    interopRequire(OK,"tracking connection factory");xfuture* Ready=xredisClientReady(C);const xredisreadyresult* R=interopWait(Ready);interopRequire(R->Status==XREDIS_READY_OK && R->Protocol==(Mode==XREDIS_INIT_RESP2?XREDIS_RESP2:XREDIS_RESP3),"actual protocol barrier");xrtFutureDestroy(Ready);
    interopRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"tracking observer");return C;
}
static void trackingRealConfigure(xredisclient* C,const xredistrackingconfig* K)
{
    xfuture* F=NULL;interopRequire(xredisClientTrackingConfigureAsync(C,K,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"tracking configuration admission");const xredistrackingresult* R=interopWait(F);
    if(R->Request.Status!=XREDIS_REQUEST_OK){xredisreplyvalue V=interopValue(F);fprintf(stderr,"configuration status=%d reply=%.*s\n",R->Request.Status,(int)V.Data.Size,(const char*)V.Data.Data);}
    interopRequire(R->Request.Status==XREDIS_REQUEST_OK && R->Sequence,"tracking configuration ACK");xrtFutureDestroy(F);
}
static void trackingRealMode(xredisclient* C,uint32 Flags)
{
    xredistrackingconfig K;xredisTrackingConfigInit(&K);K.Enabled=false;trackingRealConfigure(C,&K);K.Enabled=true;K.Flags=Flags;trackingRealConfigure(C,&K);
}
static void trackingRealSet(xredisclient* C,xbytesview Key,cstr Value)
{xbytesview A[]={XRT_BYTES_LITERAL("SET"),Key,{(cbytes)Value,strlen(Value)}};xfuture* F=interopCall(C,A,3);xrtFutureDestroy(F);}
static xfuture* trackingRealGet(xredisclient* C,xbytesview Key)
{
    xbytesview A[]={XRT_BYTES_LITERAL("GET"),Key};xfuture* F=NULL;interopRequire(xredisClientTrackingCall(C,A,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"observed GET");const xredistrackingresult* R=interopWait(F);interopRequire(R->Request.Status==XREDIS_REQUEST_OK && R->Sequence,"observed GET outcome");return F;
}
static bool trackingRealHas(const xredistrackingevent* E,xbytesview Key)
{
    for(size_t N=E->FirstKey;N!=XREDIS_REPLY_NONE;){xredisreplyvalue V;interopRequire(xredisReplyGet(E->Reply,N,&V),"key iteration");if(V.Data.Size==Key.Size && !memcmp(V.Data.Data,Key.Data,Key.Size))return true;N=V.Next;}return false;
}
static xredistrackingpush trackingRealEvent(xredisclient* C,xredistrackingeventkind Kind,xbytesview Key)
{
    double End=xrtTimer()+5;xredistrackingpush P;
    for(;;){if(xredisClientTrackingTake(C,&P)){xredistrackingevent E;interopRequire(xredisTrackingEventRead(P.Reply,&E)==XREDIS_TRACKING_EVENT && E.Kind==Kind,"real invalidation kind");
            interopRequire(Kind!=XREDIS_TRACKING_KEYS || trackingRealHas(&E,Key),"real invalidation includes exact binary key");return P;}
        interopRequire(xrtTimer()<End,"real invalidation deadline");xrtSleep(1);}
}
static void trackingRealNone(xredisclient* C)
{
    xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* F=interopCall(C,&Ping,1);xrtFutureDestroy(F);double End=xrtTimer()+0.02;
    do {xredistrackingpush P;interopRequire(!xredisClientTrackingTake(C,&P),"no invalidation through causal ping and bounded observation");xrtSleep(1);}while(xrtTimer()<End);
}
typedef struct trackingPairProducer {xredisclient* Client;unsigned Id;bool OptOut;} trackingPairProducer;
static xbytesview trackingPairKey(unsigned char* Buffer,size_t Capacity,bool OptOut,unsigned Id,unsigned N,bool Group)
{
    int Bytes=snprintf((char*)Buffer,Capacity,"xp:%u:%u:%u:%c",OptOut?1u:0u,Id,N,Group?'g':'o');
    interopRequire(Bytes>0 && (size_t)Bytes<Capacity,"pair key encoding");return (xbytesview){Buffer,(size_t)Bytes};
}
static int32 trackingPairsProduce(ptr Data)
{
    trackingPairProducer* P=Data;
    for(unsigned N=0;N<8;++N){
        unsigned char Text[48];xbytesview Key=trackingPairKey(Text,sizeof(Text),P->OptOut,P->Id,N,true);
        xbytesview Args[]={XRT_BYTES_LITERAL("GET"),Key};xfuture* F=NULL;
        interopRequire(xredisClientCachingCallAsync(P->Client,!P->OptOut,Args,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"concurrent real group admission");
        const xrediscachingresult* R=interopWait(F);xredisreplyvalue Value;
        interopRequire(R->ControlReceived && R->Control.Request.Status==XREDIS_REQUEST_OK && R->Query.Request.Status==XREDIS_REQUEST_OK &&
            R->Control.Sequence && R->Query.Sequence==R->Control.Sequence+1 && xredisReplyGet(R->Query.Request.Reply,0,&Value) &&
            Value.Data.Size==4 && !memcmp(Value.Data.Data,"seed",4),"real group exact two statuses/order/value");xrtFutureDestroy(F);
        Key=trackingPairKey(Text,sizeof(Text),P->OptOut,P->Id,N,false);F=trackingRealGet(P->Client,Key);xrtFutureDestroy(F);
    }
    return 0;
}
static void trackingRealPairs(xredisclient* C,xredisclient* Writer,bool OptOut)
{
    for(unsigned Id=0;Id<4;++Id)for(unsigned N=0;N<8;++N)for(unsigned Group=0;Group<2;++Group){
        unsigned char Text[48];trackingRealSet(Writer,trackingPairKey(Text,sizeof(Text),OptOut,Id,N,Group!=0),"seed");
    }
    trackingRealMode(C,OptOut?XREDIS_TRACKING_OPTOUT:XREDIS_TRACKING_OPTIN);
    trackingPairProducer Work[4];xthread* Threads[4];
    for(unsigned Id=0;Id<4;++Id){Work[Id]=(trackingPairProducer){C,Id,OptOut};Threads[Id]=xrtThreadCreate(trackingPairsProduce,&Work[Id],0);interopRequire(Threads[Id]!=NULL,"real concurrent pair producer");}
    for(unsigned Id=0;Id<4;++Id){interopRequire(xrtThreadWaitFor(Threads[Id],10000)==XWAIT_OK,"real producers joined");xrtThreadDestroy(Threads[Id]);}
    for(unsigned Id=0;Id<4;++Id)for(unsigned N=0;N<8;++N)for(unsigned Group=0;Group<2;++Group){
        unsigned char Text[48];trackingRealSet(Writer,trackingPairKey(Text,sizeof(Text),OptOut,Id,N,Group!=0),"changed");
    }
    /* Every qualifying key, and no other key, must be invalidated. Messages
     * may group keys, so validate the complete set rather than push count. */
    uint64 Seen=0;double End=xrtTimer()+5;
    while(Seen!=UINT64_C(0xffffffff)){
        xredistrackingpush Push;if(!xredisClientTrackingTake(C,&Push)){interopRequire(xrtTimer()<End,"real grouped invalidation set deadline");xrtThreadYield();continue;}
        xredistrackingevent E;interopRequire(xredisTrackingEventRead(Push.Reply,&E)==XREDIS_TRACKING_EVENT && E.Kind==XREDIS_TRACKING_KEYS,"real group invalidation shape");
        for(size_t Node=E.FirstKey;Node!=XREDIS_REPLY_NONE;){
            xredisreplyvalue Key;interopRequire(xredisReplyGet(E.Reply,Node,&Key),"real group invalidation key");bool Found=false;
            for(unsigned Id=0;Id<4;++Id)for(unsigned N=0;N<8;++N){unsigned char Text[48];xbytesview Expected=trackingPairKey(Text,sizeof(Text),OptOut,Id,N,!OptOut);
                if(Key.Data.Size==Expected.Size && !memcmp(Key.Data.Data,Expected.Data,Expected.Size)){
                    uint64 Bit=UINT64_C(1)<<(Id*8+N);interopRequire(!(Seen&Bit),"each real tracking key invalidated once");Seen|=Bit;Found=true;
                }
            }
            interopRequire(Found,"CACHING flag never applied to unrelated producer query");Node=Key.Next;
        }
        xredisReplyRelease(Push.Reply);
    }
    trackingRealNone(C);
}
int main(int Argc,char** Argv)
{
    if(Argc!=7 || strlen(Argv[6])!=40)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);TrackingAuth=atoi(Argv[2]);TrackingMode=(unsigned)atoi(Argv[3]);TrackingVersion=(unsigned)atoi(Argv[4]);
    if(!Port || Port>65535 || TrackingMode>2 || (TrackingVersion!=0 && TrackingVersion!=12 && TrackingVersion!=13) || TrackingAuth<0 || TrackingAuth>2)return 2;
    xtlsverifier* Trust=NULL;xtlscontext* Context=NULL;
    if(TrackingVersion){Trust=interopTrust(Argv[5]);xtlspolicy Policy;xrtTlsPolicyInit(&Policy);xtlsversion V=TrackingVersion==12?XTLS_VERSION_12:XTLS_VERSION_13;xtlscipher Cipher=TrackingVersion==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
        Policy.Versions=&V;Policy.VersionCount=1;Policy.Ciphers=&Cipher;Policy.CipherCount=1;Policy.Signatures=&Signature;Policy.SignatureCount=1;xtlscontextconfig K;xrtTlsContextConfigInit(&K);K.Policy=&Policy;Context=xrtTlsContextCreate(&K);interopRequire(Context!=NULL,"tracking TLS policy");
        xredisClientTlsConfigInit(&InteropTLS);InteropTLS.Session.Context=Context;InteropTLS.Session.Verifier=Trust;InteropTLS.Session.ServerName=XRT_STR_LITERAL("localhost");InteropTLS.Session.VerifyName=XRT_STR_LITERAL("localhost");}
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&EC);interopRequire(Engine && xrtNetEngineStart(Engine),"tracking Engine");InteropResolver=xrtNetResolverCreate(NULL);interopRequire(InteropResolver!=NULL,"tracking Resolver");
    xnetaddr Address;interopRequire(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)Port),"tracking address");xredisclient* C=trackingRealConnect(Engine,&Address,XREDIS_INIT_RESP3);
    xbytesview Identify[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* Identity=interopCall(C,Identify,2);xredisreplyvalue Server=interopValue(Identity);bool Match=false;
    for(size_t N=0;N+47<=Server.Data.Size;++N)if(!memcmp(Server.Data.Data+N,"run_id:",7) && !memcmp(Server.Data.Data+N+7,Argv[6],40)){Match=true;break;}
    interopRequire(Match,"endpoint belongs to this disposable Redis instance before data mutation");xrtFutureDestroy(Identity);
    xredisclient* Writer=trackingRealConnect(Engine,&Address,XREDIS_INIT_RESP3);
    xbytesview Key=XRT_BYTES_LITERAL("xt:\0key"),Missing=XRT_BYTES_LITERAL("xt:missing"),Prefix=XRT_BYTES_LITERAL("xt:\0");
    trackingRealSet(Writer,Key,"old");trackingRealMode(C,0);xfuture* Old=trackingRealGet(C,Key);trackingRealSet(Writer,Key,"new");xfuture* New=trackingRealGet(C,Key);xredistrackingpush Invalidation=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Key);
    const xredistrackingresult *OldR=interopWait(Old),*NewR=interopWait(New);xredisreplyvalue V=interopValue(Old);interopRequire(V.Data.Size==3 && !memcmp(V.Data.Data,"old",3),"old GET payload retained");V=interopValue(New);
    interopRequire(V.Data.Size==3 && !memcmp(V.Data.Data,"new",3) && OldR->Sequence<Invalidation.Sequence && Invalidation.Sequence<NewR->Sequence,"real wire order distinguishes stale and fresh responses collected before invalidation");
    xbytesview Delete[]={XRT_BYTES_LITERAL("DEL"),Missing};xfuture* F=interopCall(Writer,Delete,2);xrtFutureDestroy(F);F=trackingRealGet(C,Missing);V=interopValue(F);interopRequire(V.Type==XREDIS_REPLY_NULL || (V.Flags&XREDIS_REPLY_NULL_VALUE),"negative cached reply");xrtFutureDestroy(F);trackingRealSet(Writer,Missing,"born");xredistrackingpush P=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Missing);xredisReplyRelease(P.Reply);
    trackingRealMode(C,0);F=trackingRealGet(C,Key);xrtFutureDestroy(F);xbytesview Flush=XRT_BYTES_LITERAL("FLUSHDB");F=interopCall(Writer,&Flush,1);xrtFutureDestroy(F);P=trackingRealEvent(C,XREDIS_TRACKING_FLUSH,Key);xredisReplyRelease(P.Reply);
    xredistrackingconfig K;xredisTrackingConfigInit(&K);K.Enabled=false;trackingRealConfigure(C,&K);K.Enabled=true;K.Flags=XREDIS_TRACKING_BCAST|XREDIS_TRACKING_NOLOOP;K.Prefixes=&Prefix;K.PrefixCount=1;trackingRealConfigure(C,&K);
    trackingRealSet(C,Key,"own");trackingRealNone(C);trackingRealSet(Writer,Key,"outside");P=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Key);xredisReplyRelease(P.Reply);trackingRealSet(Writer,Missing,"outside-prefix");trackingRealNone(C);
    trackingRealMode(C,XREDIS_TRACKING_NOLOOP);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(C,Key,"self");trackingRealNone(C);trackingRealSet(Writer,Key,"untracked-after-self");trackingRealNone(C);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"tracked-again");P=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Key);xredisReplyRelease(P.Reply);
    /* These one-shot commands are deliberately serialized on this dedicated
     * producer; no claim of atomic two-call composition on shared connections. */
    trackingRealMode(C,XREDIS_TRACKING_OPTIN);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"not-opted");trackingRealNone(C);
    xbytesview Caching[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("CACHING"),XRT_BYTES_LITERAL("YES")};F=interopCall(C,Caching,3);xrtFutureDestroy(F);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"opted-in");P=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Key);xredisReplyRelease(P.Reply);
    trackingRealMode(C,XREDIS_TRACKING_OPTOUT);Caching[2]=(xbytesview)XRT_BYTES_LITERAL("NO");F=interopCall(C,Caching,3);xrtFutureDestroy(F);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"opted-out");trackingRealNone(C);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"default-optout");P=trackingRealEvent(C,XREDIS_TRACKING_KEYS,Key);xredisReplyRelease(P.Reply);
    trackingRealPairs(C,Writer,false);trackingRealPairs(C,Writer,true);
    trackingRealMode(C,0);xbytesview PairArgs[]={XRT_BYTES_LITERAL("GET"),Key};xfuture* ControlError=NULL;
    interopRequire(xredisClientCachingCall(C,true,PairArgs,2,5000,&ControlError)==XREDIS_ADMISSION_ACCEPTED,"real refused prefix group");
    const xrediscachingresult* ErrorPair=interopWait(ControlError);
    interopRequire(ErrorPair->ControlReceived && ErrorPair->Control.Request.Status==XREDIS_REQUEST_SERVER_ERROR && ErrorPair->Query.Request.Status==XREDIS_REQUEST_OK && ErrorPair->Query.Sequence==ErrorPair->Control.Sequence+1,"real control rejection still executes query");
    xredisclient* Receiver=trackingRealConnect(Engine,&Address,XREDIS_INIT_RESP3);xbytesview ID[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("ID")};F=interopCall(Receiver,ID,2);V=interopValue(F);interopRequire(V.Type==XREDIS_REPLY_INTEGER && V.Integer>0,"recipient id");int64 Recipient=V.Integer;xrtFutureDestroy(F);
    trackingRealMode(C,0);xredisTrackingConfigInit(&K);K.Flags=XREDIS_TRACKING_REDIRECT;K.Redirect=Recipient;trackingRealConfigure(C,&K);F=trackingRealGet(C,Key);xrtFutureDestroy(F);trackingRealSet(Writer,Key,"redirected");P=trackingRealEvent(Receiver,XREDIS_TRACKING_KEYS,Key);xredisReplyRelease(P.Reply);trackingRealNone(C);
    xredisclient* Legacy=trackingRealConnect(Engine,&Address,XREDIS_INIT_RESP2);xfuture* Rejected=(xfuture*)(uintptr_t)1;xredistrackingconfig Direct;xredisTrackingConfigInit(&Direct);interopRequire(xredisClientTrackingConfigureAsync(Legacy,&Direct,5000,&Rejected)==XREDIS_ADMISSION_ERROR && Rejected==(xfuture*)(uintptr_t)1,"RESP2 direct mode rejected locally");xrtClearError();
    trackingRealConfigure(Legacy,&K);F=trackingRealGet(Legacy,Missing);xrtFutureDestroy(F);trackingRealSet(Writer,Missing,"resp2-redirect");P=trackingRealEvent(Receiver,XREDIS_TRACKING_KEYS,Missing);xredisReplyRelease(P.Reply);
    xredistrackingconfig LegacyOptIn=K;LegacyOptIn.Flags|=XREDIS_TRACKING_OPTIN;trackingRealConfigure(Legacy,&LegacyOptIn);PairArgs[1]=Missing;
    interopRequire(xredisClientCachingCall(Legacy,true,PairArgs,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"RESP2 redirected group");const xrediscachingresult* LegacyPair=interopWait(F);
    interopRequire(LegacyPair->Control.Request.Status==XREDIS_REQUEST_OK && LegacyPair->Query.Request.Status==XREDIS_REQUEST_OK,"RESP2 OPTIN prefix and query replies");xrtFutureDestroy(F);
    trackingRealSet(Writer,Missing,"resp2-group");P=trackingRealEvent(Receiver,XREDIS_TRACKING_KEYS,Missing);xredisReplyRelease(P.Reply);interopClose(Legacy);
    F=trackingRealGet(C,Key);xrtFutureDestroy(F);interopClose(Receiver);
    char RecipientText[32];int RecipientBytes=snprintf(RecipientText,sizeof(RecipientText),"%" PRIi64,Recipient);interopRequire(RecipientBytes>0 && (size_t)RecipientBytes<sizeof(RecipientText),"recipient id encoding");
    xbytesview Kill[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("KILL"),XRT_BYTES_LITERAL("ID"),{(cbytes)RecipientText,(size_t)RecipientBytes}};F=interopCall(Writer,Kill,4);xrtFutureDestroy(F);
    trackingRealSet(Writer,Key,"broken-recipient");P=trackingRealEvent(C,XREDIS_TRACKING_REDIRECT_BROKEN,Key);xredistrackingevent Event;interopRequire(xredisTrackingEventRead(P.Reply,&Event)==XREDIS_TRACKING_EVENT && Event.Redirect==(uint64)Recipient,"redirection failure identity");xredisReplyRelease(P.Reply);
    interopClose(C);interopClose(Writer);interopRequire(xrtNetResolverDestroy(InteropResolver) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"tracking physical owners joined");
    interopRequire(xredisTrackingEventRead(Invalidation.Reply,&Event)==XREDIS_TRACKING_EVENT && OldR->Sequence<Invalidation.Sequence && NewR->Sequence>Invalidation.Sequence,"real results survive all connections/Engine");
    interopRequire(ErrorPair->Control.Request.Status==XREDIS_REQUEST_SERVER_ERROR && ErrorPair->Query.Request.Status==XREDIS_REQUEST_OK,"both group replies survive every physical owner");
    xredisreplyvalue GroupValue;interopRequire(xredisReplyGet(ErrorPair->Control.Request.Reply,0,&GroupValue) && GroupValue.Type==XREDIS_REPLY_SIMPLE_ERROR && xredisReplyGet(ErrorPair->Query.Request.Reply,0,&GroupValue),"retained error and query allocations");
    xredisReplyRelease(Invalidation.Reply);xrtFutureDestroy(Old);xrtFutureDestroy(New);xrtFutureDestroy(ControlError);if(Trust)xrtTlsVerifierRelease(Trust);if(Context)xrtTlsContextRelease(Context);
    printf("[PASS] real Tracking: ordering, binary/missing keys, flush, BCAST/PREFIX/NOLOOP, OPTIN/OPTOUT, RESP2/3 redirect and broken redirect; mode=%u TLS=%u auth=%d\n",TrackingMode,TrackingVersion,TrackingAuth);return 0;
}
