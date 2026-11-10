#include <xmqtt.h>
#include "../../examples/tls/material.h"
#include <stdlib.h>
/* Test-only automatic-controller transport adapter. Protocol assertions are
 * shared with the independent managed broker probe. Trust/key material is bounded runtime input, never a fixture key. */
static xtlscontext* sessionContext;
static xtlsverifier* sessionVerifier;
static xtlsidentity* sessionIdentity;
static xtlsresume* sessionTicket;
static xnetresolver* sessionResolver;
static xnetaddr sessionAddress;
static unsigned sessionFactoryIndex,sessionTlsVersion,sessionResumeMode,sessionRound;
static bool sessionPrefix;
static size_t sessionAccepted,sessionWireBytes;
static xatomic32 sessionLookups;
static void sessionRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"[FAIL] TLS Session broker: %s cause=%s\n",Message,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static xnetaddrlist* sessionLookup(cstr Host,xnetfamily Family,ptr Data)
{
    (void)Data;sessionRequire(!strcmp(Host,"session.tls.fixture"),"copied named endpoint");if(Family==XNET_FAMILY_IPV6)return NULL;
    xnetaddr A=sessionAddress;A.Port=0;xrtAtomic32FetchAdd(&sessionLookups,1,XMEMORY_RELEASE);return xrtNetAddrListCreate(&A,1);
}
static bool sessionConnect(xnetengine* E,const xnetaddr* Address,uint64 Affinity,const xmqttclientconfig* Given,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** C)
{
    xmqttclientconfig K=*Given;if(sessionPrefix && !sessionRound){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=65536;K.Stream.WriteHighWater=32768;K.Stream.WriteLowWater=16384;}
    xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=sessionContext;T.Session.Verifier=sessionVerifier;T.Session.ServerName=T.Session.VerifyName=XRT_STR_LITERAL("localhost");
    char Alpn[4]={'m','q','t','t'};xstrview Protocol={Alpn,4};T.Session.Protocols=&Protocol;T.Session.ProtocolCount=1;T.Session.Resume=sessionTicket;T.Session.ResumeOnly=sessionRound && sessionResumeMode==1;
    xtlsclientauthconfigv1 Auth;if(sessionIdentity){xrtTlsClientAuthConfigInitV1(&Auth);Auth.Identity=sessionIdentity;T.ClientAuth=&Auth;}
    if(sessionPrefix && !sessionRound){T.Stream.AsyncBytesLimit=65536;T.Stream.AsyncCountLimit=64;T.Stream.AsyncBatch=8;}
    bool OK;
    if(!sessionFactoryIndex)OK=xmqttClientTlsConnectSession(E,Address,Affinity,&K,&T,S,I,Q,C);
    else if(sessionFactoryIndex==1){
        char Host[19];memcpy(Host,"session.tls.fixture",19);OK=xmqttClientTlsDialSession(E,sessionResolver,(xstrview){Host,19},Address->Port,Affinity,&K,&T,S,I,Q,C);memset(Host,'z',sizeof(Host));}
    else {xnetaddrlist* L=xrtNetAddrListCreate(Address,1);if(!L)return false;OK=xmqttClientTlsDialAddressesSession(E,L,Affinity,&K,&T,S,I,Q,C);xrtNetAddrListDestroy(L);}
    if(OK){xrtTlsResumeRelease(sessionTicket);sessionTicket=NULL;}
    memset(&T,0,sizeof(T));memset(&Auth,0,sizeof(Auth));memset(Alpn,0,sizeof(Alpn));return OK;
}
static void sessionReady(xmqttclient* C,const xmqttreadyresult* R)
{
    (void)R;xmqttclienttlsresumeinfo Info;sessionRequire(xmqttClientTlsResumeInfo(C,&Info) && Info.Established && Info.Resumed==(sessionRound && sessionResumeMode==1),"authenticated full/resumed observation independent of MQTT SessionPresent");
    if(sessionIdentity)sessionRequire(!Info.Enabled,"native mutual-auth sessions do not publish credentials");
    if(!sessionRound && sessionResumeMode){double End=xrtTimer()+10;while(!xmqttClientTlsTakeResume(C,&sessionTicket)){sessionRequire(xrtTimer()<End,"bounded ticket handoff before interruption");xrtSleep(1);}}
    ++sessionRound;
}
static void sessionOld(const xmqttwriteresult* R,const xmqttpacket* Given)
{
    sessionAccepted=R->AcceptedBytes;if(!sessionPrefix)return;xmqttpacket Q=*Given;Q.Identifier=R->Identifier;
    bool OK=Q.Properties.Count?xmqttPacketSize5(&Q,XMQTT_CLIENT_TO_SERVER,NULL,&sessionWireBytes):xmqttPacketSize311(&Q,XMQTT_CLIENT_TO_SERVER,NULL,&sessionWireBytes);
    if(!OK || !sessionAccepted || sessionAccepted>=sessionWireBytes)fprintf(stderr,"partial precondition: accepted=%zu wire=%zu valid=%u\n",sessionAccepted,sessionWireBytes,OK?1u:0u);
    sessionRequire(OK && sessionAccepted && sessionAccepted<sessionWireBytes,"interrupted authenticated MQTT prefix is locally partial and remains owned");
}
#define xmqttClientConnectSession sessionConnect
#define XMQTT_RECONNECT_BROKER_READY sessionReady
#define XMQTT_RECONNECT_BROKER_OLD_RESULT sessionOld
#define XMQTT_RECONNECT_BROKER_PAYLOAD_SIZE(Standard) (sessionPrefix?512u*1024u:(Standard))
#define main sessionProtocolMain
#include "reconnect_broker_probe.c"
#undef main
#undef xmqttClientConnectSession
int main(int Argc,char** Argv)
{
    /* Base PORT MQTT drop fragmented HOST, then TLS FACTORY CA CERT|- KEY|-
     * RESUME(0 full/1 resume-only/2 full-fallback) PREFIX(0/1). */
    if(Argc!=13)return 2;
    sessionTlsVersion=(unsigned)strtoul(Argv[6],NULL,10);sessionFactoryIndex=(unsigned)strtoul(Argv[7],NULL,10);sessionResumeMode=(unsigned)strtoul(Argv[11],NULL,10);sessionPrefix=atoi(Argv[12])!=0;
    sessionRequire((sessionTlsVersion==12 || sessionTlsVersion==13) && sessionFactoryIndex<3 && sessionResumeMode<3 && (!sessionResumeMode || (sessionTlsVersion==13 && !strcmp(Argv[9],"-"))),"TLS driver arguments");
    size_t Size=0;bytes CA=xmqttExampleTlsRead(Argv[8],65536,&Size);xx509store* Store=xrtX509StoreCreate();sessionRequire(CA && Store && xrtX509StoreAddData(Store,(xbytesview){CA,Size},NULL),"runtime trust anchors");xrtSecureZero(CA,Size);xrtFree(CA);
    xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);sessionVerifier=xrtTlsVerifierCreateLimited(&VC,&Limits);xrtX509StoreFree(Store);sessionRequire(sessionVerifier!=NULL,"strict bounded verifier");
    xtlsversion Version=sessionTlsVersion==12?XTLS_VERSION_12:XTLS_VERSION_13;xtlscipher Cipher=sessionTlsVersion==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;
    xtlspolicy Policy;xrtTlsPolicyInit(&Policy);Policy.Versions=&Version;Policy.VersionCount=1;Policy.Ciphers=&Cipher;Policy.CipherCount=1;Policy.Signatures=&Signature;Policy.SignatureCount=1;xtlscontextconfig TC;xrtTlsContextConfigInit(&TC);TC.Policy=&Policy;if(sessionPrefix)TC.Limits.SendLimit=65536;sessionContext=xrtTlsContextCreate(&TC);sessionRequire(sessionContext!=NULL,"version-specific TLS context");
    if(strcmp(Argv[9],"-")){sessionIdentity=xmqttExampleTlsIdentity(Argv[9],Argv[10]);sessionRequire(sessionIdentity!=NULL,"runtime client identity");}
    sessionRequire(xrtNetAddrParse(&sessionAddress,Argv[5],(uint16)strtoul(Argv[1],NULL,10)),"immutable resolver target");
    if(sessionFactoryIndex==1){xnetresolverconfig R;xrtNetResolverConfigInit(&R);R.Workers=1;R.CacheEntries=0;R.Lookup=sessionLookup;xrtAtomic32Init(&sessionLookups,0);sessionResolver=xrtNetResolverCreate(&R);sessionRequire(sessionResolver!=NULL,"one borrowed asynchronous Resolver for both generations");}
    int Result=sessionProtocolMain(6,Argv);
    if(sessionResolver){sessionRequire(xrtAtomic32Load(&sessionLookups,XMEMORY_ACQUIRE)==2 && xrtNetResolverDestroy(sessionResolver),"two independent lookups and physical Resolver join after controller Destroy");sessionResolver=NULL;}
sessionRequire(sessionRound==2,"two authenticated connections");xrtTlsResumeRelease(sessionTicket);xrtTlsIdentityRelease(sessionIdentity);xrtTlsVerifierRelease(sessionVerifier);xrtTlsContextRelease(sessionContext);
    printf("[PASS] TLS Session factory%u TLS%u resume%u prefix%u accepted%zu wire%zu\n",sessionFactoryIndex,sessionTlsVersion,sessionResumeMode,sessionPrefix?1u:0u,sessionAccepted,sessionWireBytes);return Result;
}
