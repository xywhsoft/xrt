#ifndef TEST_TLS_HOST_INFERENCE_H
#define TEST_TLS_HOST_INFERENCE_H
/* Synthetic local TLS/protocol fixture with exact certificate pinning and
 * independent observations of wire SNI, verification name and DNS route. */
typedef struct testhostobserver {
    xtlsidentity* Identity;xstrview VerifyName,SNI,Route;xnetaddr Address;
    xatomic32 Verified,Selected,Lookups;uint64 Caller;const xerror* Ambient;
    unsigned OldNotices,NewNotices;
} testhostobserver;
static void testHostNotice(const xerror* Error,ptr Data)
{
    testhostobserver* O=Data;if(xrtThreadCurrentId()!=O->Caller)return;
    if(Error==O->Ambient)++O->OldNotices;else ++O->NewNotices;
}
static bool testHostSelect(ptr Data,const xtlsserverrequest* R,xtlsserverchoice* Choice)
{
    (void)Choice;testhostobserver* O=Data;testRequire(R->ServerName.Size==O->SNI.Size&&(!O->SNI.Size||!memcmp(R->ServerName.Data,O->SNI.Data,O->SNI.Size)),"actual ClientHello SNI");
    xrtAtomic32FetchAdd(&O->Selected,1,XMEMORY_RELEASE);return true;
}
static xtlsverifydecision testHostVerify(const xtlspeer* Peer,ptr Data)
{
    testhostobserver* O=Data;xbytesview Pin={0};bool OK=Peer&&Peer->CertificateCount&&Peer->Name.Size==O->VerifyName.Size&&!memcmp(Peer->Name.Data,O->VerifyName.Data,O->VerifyName.Size)&&
        xrtTlsIdentityCertificate(O->Identity,0,&Pin)&&Peer->Certificates[0].Raw.Size==Pin.Size&&!memcmp(Peer->Certificates[0].Raw.Data,Pin.Data,Pin.Size);
    testRequire(OK,"exact verification identity and independently pinned certificate");xrtAtomic32FetchAdd(&O->Verified,1,XMEMORY_RELEASE);return XTLS_VERIFY_ACCEPT;
}
static xnetaddrlist* testHostLookup(cstr Host,xnetfamily Family,ptr Data)
{
    (void)Family;testhostobserver* O=Data;testRequire(strlen(Host)==O->Route.Size&&!memcmp(Host,O->Route.Data,O->Route.Size),"exact counted hostname snapshot at resolver");
    xrtAtomic32FetchAdd(&O->Lookups,1,XMEMORY_RELEASE);xnetaddr A=O->Address;A.Port=0;return xrtNetAddrListCreate(&A,1);
}
static void testHostInit(testhostobserver* O,xstrview Host,unsigned Policy,bool Numeric)
{
    memset(O,0,sizeof(*O));xrtAtomic32Init(&O->Verified,0);xrtAtomic32Init(&O->Selected,0);xrtAtomic32Init(&O->Lookups,0);O->Caller=xrtThreadCurrentId();O->Route=Host;
    O->SNI=Policy==2?XRT_STR_LITERAL("route.fixture"):Policy==3||Numeric?(xstrview){0}:Host;
    O->VerifyName=Policy==1?XRT_STR_LITERAL("verify.fixture"):Policy==2?O->SNI:Policy==3?XRT_STR_LITERAL("manual.fixture"):Host;
}
static xtlsverifier* testHostConfig(testhostobserver* O,unsigned Policy,xtlsclientconfig* Config,bool* Infer)
{
    Config->ResumeLimit=0;Config->ServerName=Policy==2?O->SNI:(xstrview){0};Config->VerifyName=Policy==1||Policy==3?O->VerifyName:(xstrview){0};*Infer=Policy!=3;
    xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=testHostVerify;V.Context=O;xtlsverifier* Verifier=xrtTlsVerifierCreate(&V);testRequire(Verifier!=NULL,"independent identity observer");Config->Verifier=Verifier;return Verifier;
}
static xnetresolver* testHostResolver(testhostobserver* O)
{xnetresolverconfig R;xrtNetResolverConfigInit(&R);R.Workers=1;R.Lookup=testHostLookup;R.LookupData=O;xnetresolver* Resolver=xrtNetResolverCreate(&R);testRequire(Resolver!=NULL,"private deterministic DNS route");return Resolver;}
static void testHostAmbient(testhostobserver* O)
{xrtSetErrorInfo(XERR_STATE,"tls.host.ambient",71,"existing caller diagnostic");O->Ambient=xrtGetError();testRequire(O->Ambient!=NULL,"ambient error");xrtSetErrorHandler(testHostNotice,O);}
static void testHostResult(testhostobserver* O)
{
    testRequire(xrtAtomic32Load(&O->Verified,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&O->Selected,XMEMORY_ACQUIRE)==1,"one wire selection and pinned verification");
    /* These are Host Dial factories: their resolver contract also receives
     * numeric text. URI/prepared-address fast paths are separate APIs. */
    testRequire(xrtAtomic32Load(&O->Lookups,XMEMORY_ACQUIRE)==1,"Host Dial keeps its exact resolver route for names and literals");
    printf("[HOST NOTICES] old=%u new=%u\n",O->OldNotices,O->NewNotices);testRequire(!O->OldNotices&&!O->NewNotices,"successful host inference must not publish or repeat a diagnostic");
}
#endif
