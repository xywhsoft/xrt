/* Public flow API with a controlled HTTP boundary. JSON, PEM, JWS, CSR,
 * allocation and cleanup execute the actual source. Stored-operation failures
 * use a controlled store boundary. Transport/storage interoperability is
 * tested separately by test_acme_mock.py, not claimed by this fixture. */
#define XACME_MODULE_ACME_FLOW
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../src/acme/xacme_core.c"
#include "../../src/acme/xacme_dns.c"
#include "../../src/acme/xacme_http.c"
#include "../../src/acme/xacme_jose.c"
#include "../../src/acme/xacme_csr.c"
#include "../../src/dns/xacme_dnstxt.c"
#include "../../src/acme/xacme_store.c"
#include "../test.h"
#include "../cert_fixture.h"

typedef enum flowop { FLOW_CREATE, FLOW_ISSUE, FLOW_REVOKE, FLOW_ROLLOVER, FLOW_DEACTIVATE, FLOW_STORED } flowop;
typedef enum flowkind { FLOW_ALLOC, FLOW_HTTP_FAIL, FLOW_INVALID, FLOW_SUCCESS } flowkind;
typedef struct flowcase { cstr Name; flowop Op; flowkind Kind; cstr Target; bool Oom; } flowcase;
static const flowcase Cases[] = {
    {"account-pem",FLOW_CREATE,FLOW_ALLOC,"account-pem",true},
    {"directory-transport",FLOW_CREATE,FLOW_HTTP_FAIL,"directory",true},
    {"directory-json",FLOW_CREATE,FLOW_ALLOC,"directory-json",true},
    {"directory-malformed",FLOW_CREATE,FLOW_INVALID,"directory-json",true},
    {"directory-nul",FLOW_CREATE,FLOW_INVALID,"directory-json",true},
    {"optional-endpoint-nul",FLOW_CREATE,FLOW_INVALID,"directory-json",true},
    {"eab-jwk",FLOW_CREATE,FLOW_ALLOC,"eab-jwk",true},
    {"eab-sign",FLOW_CREATE,FLOW_ALLOC,"eab-sign",true},
    {"account-sign",FLOW_CREATE,FLOW_ALLOC,"account-sign",true},
    {"account-transport",FLOW_CREATE,FLOW_HTTP_FAIL,"account",true},
    {"account-json",FLOW_CREATE,FLOW_ALLOC,"account-json",true},
    {"account-status-nul",FLOW_CREATE,FLOW_INVALID,"account-json",true},
    {"cert-key-alloc",FLOW_CREATE,FLOW_ALLOC,"cert-key-alloc",true},
    {"cert-key-pem",FLOW_CREATE,FLOW_ALLOC,"cert-key-pem",true},
    {"invalid-nonce",FLOW_CREATE,FLOW_INVALID,"nonce",true},
    {"nonce-status",FLOW_CREATE,FLOW_INVALID,"nonce",true},
    {"contact-long",FLOW_CREATE,FLOW_SUCCESS,"directory-json",true},
    {"order-sign",FLOW_ISSUE,FLOW_ALLOC,"order-sign",true},
    {"order-transport",FLOW_ISSUE,FLOW_HTTP_FAIL,"new-order",true},
    {"order-json",FLOW_ISSUE,FLOW_ALLOC,"new-order-json",true},
    {"authorization-url-nul",FLOW_ISSUE,FLOW_INVALID,"new-order-json",true},
    {"thumbprint",FLOW_ISSUE,FLOW_ALLOC,"thumbprint",true},
    {"authorization-transport",FLOW_ISSUE,FLOW_HTTP_FAIL,"authz",true},
    {"authorization-json",FLOW_ISSUE,FLOW_ALLOC,"authz-json",true},
    {"authorization-status-nul",FLOW_ISSUE,FLOW_INVALID,"authz-json",true},
    {"authorization-identifier-nul",FLOW_ISSUE,FLOW_INVALID,"authz-json",true},
    {"txt-base64",FLOW_ISSUE,FLOW_ALLOC,"txt-base64",true},
    {"propagate-memory",FLOW_ISSUE,FLOW_ALLOC,"propagate",true},
    {"resolver-memory-first",FLOW_ISSUE,FLOW_ALLOC,"resolver",true},
    {"resolver-memory-second",FLOW_ISSUE,FLOW_ALLOC,"resolver",true},
    {"resolver-fallback",FLOW_ISSUE,FLOW_SUCCESS,"resolver",false},
    {"resolver-advisory",FLOW_ISSUE,FLOW_SUCCESS,"resolver",false},
    {"challenge-transport",FLOW_ISSUE,FLOW_HTTP_FAIL,"challenge",true},
    {"auth-poll-json",FLOW_ISSUE,FLOW_ALLOC,"auth-poll-json",true},
    {"auth-poll-malformed",FLOW_ISSUE,FLOW_INVALID,"auth-poll-json",true},
    {"auth-status-copy",FLOW_ISSUE,FLOW_ALLOC,"auth-status-copy",true},
    {"order-poll-json",FLOW_ISSUE,FLOW_ALLOC,"order-ready-json",true},
    {"order-status-copy",FLOW_ISSUE,FLOW_ALLOC,"order-status-copy",true},
    {"finalize-url-missing",FLOW_ISSUE,FLOW_INVALID,"order-ready-json",true},
    {"csr",FLOW_ISSUE,FLOW_ALLOC,"csr",true},
    {"cert-key-export",FLOW_ISSUE,FLOW_ALLOC,"cert-key-export",true},
    {"csr-base64",FLOW_ISSUE,FLOW_ALLOC,"csr-base64",true},
    {"finalize-transport",FLOW_ISSUE,FLOW_HTTP_FAIL,"finalize",true},
    {"final-poll-json",FLOW_ISSUE,FLOW_ALLOC,"order-valid-json",true},
    {"certificate-url-json",FLOW_ISSUE,FLOW_ALLOC,"certificate-url-json",true},
    {"certificate-transport",FLOW_ISSUE,FLOW_HTTP_FAIL,"certificate",true},
    {"certificate-json",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-private-key",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-invalid-der",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-wrong-key",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-wrong-domain",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-der",FLOW_ISSUE,FLOW_ALLOC,"certificate-der",true},
    {"certificate-second-der",FLOW_ISSUE,FLOW_ALLOC,"certificate-second-der",true},
    {"certificate-name",FLOW_ISSUE,FLOW_ALLOC,"certificate-name",true},
    {"certificate-rsa-signature",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"certificate-type",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-missing-type",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-expired",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-future",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-extra-domain",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-no-san",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-ip-san",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-wrong-ca",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-bad-signature",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-not-ca",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-garbage",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-count",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"certificate-leaf-only",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"certificate-type-parameters",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"certificate-rsa",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"account-cert-key",FLOW_ISSUE,FLOW_INVALID,"domain",false},
    {"alternate-valid",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-json",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-private-key",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-different-leaf",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-wrong-ca",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-der",FLOW_ISSUE,FLOW_SUCCESS,"alternate-der",true},
    {"alternate-path",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-dot-path",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"alternate-network-path",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"revoke-der",FLOW_REVOKE,FLOW_ALLOC,"revoke-der",true},
    {"revoke-already",FLOW_REVOKE,FLOW_SUCCESS,"revoke-json",true},
    {"revoke-detail",FLOW_REVOKE,FLOW_INVALID,"revoke-json",true},
    {"revoke-type-nul",FLOW_REVOKE,FLOW_INVALID,"revoke-json",true},
    {"rollover-pem",FLOW_ROLLOVER,FLOW_ALLOC,"account-pem",true},
    {"rollover-lookup-json",FLOW_ROLLOVER,FLOW_ALLOC,"account-json",true},
    {"rollover-lookup-nonce",FLOW_ROLLOVER,FLOW_SUCCESS,"account-json",true},
    {"rollover-outer-nonce",FLOW_ROLLOVER,FLOW_SUCCESS,"key-change-json",true},
    {"rollover-store",FLOW_ROLLOVER,FLOW_HTTP_FAIL,"store-account",true},
    {"deactivate-transport",FLOW_DEACTIVATE,FLOW_HTTP_FAIL,"deactivate",true},
    {"badnonce-detail",FLOW_DEACTIVATE,FLOW_INVALID,"deactivate-json",true},
    {"badnonce-fresh",FLOW_DEACTIVATE,FLOW_SUCCESS,"deactivate-json",true},
    {"badnonce-missing",FLOW_DEACTIVATE,FLOW_INVALID,"deactivate-json",true},
    {"badnonce-invalid",FLOW_DEACTIVATE,FLOW_INVALID,"deactivate-json",true},
    {"badnonce-limit",FLOW_DEACTIVATE,FLOW_INVALID,"deactivate-json",true},
    {"deactivate-json",FLOW_DEACTIVATE,FLOW_ALLOC,"deactivate-json",true},
    {"deactivate-status-nul",FLOW_DEACTIVATE,FLOW_INVALID,"deactivate-json",true},
    {"stored-save",FLOW_STORED,FLOW_HTTP_FAIL,"store-grant",true},
    {"domain-overflow",FLOW_ISSUE,FLOW_INVALID,"domain",true},
    {"domain-nul",FLOW_ISSUE,FLOW_INVALID,"domain",true},
    {"domain-duplicates",FLOW_ISSUE,FLOW_SUCCESS,"order-sign",true},
    {"host-cert-key",FLOW_ISSUE,FLOW_SUCCESS,NULL,false},
    {"challenge-detail-shape",FLOW_ISSUE,FLOW_INVALID,NULL,false},
    {"cleanup-cause",FLOW_ISSUE,FLOW_HTTP_FAIL,"finalize",true}
};
static const flowcase* Active;
static bool Enabled, Inject, Armed, FirstReply, Finalized, InOperation;
static unsigned Requests, NonceRequests, AuthzRequests, OrderRequests, LookupRequests, KeyChangeRequests, Sleeps, Adds, Removes;
static unsigned ResolverQueries;
static cstr ResolverEndpointExpected;
static uint16 ResolverEndpointPort;
static unsigned CertificateDecodes, AlternateRequests;
static bool AlternateResponse;
static char ResolverTxt[256];
static double ResolverClock;
static cstr LastJson;
static xerror* FirstError;
/* Observe decoded EAB keys only while their storage is alive: inside the
 * actual SecureZero/free call. A missing wipe is detected by an outstanding
 * observation, never by reading a saved stack pointer after Init returns. */
static bool EabProbe, EabArmed, EabEnded, EabWire;
static int64 EabFailAfter;
static cstr EabText;
static uintptr_t EabDecodedAddress;
static bool EabPending;
static size_t EabCapacity, EabDecodedSize;
static unsigned EabWipes, EabLocalWipes, EabDirtyFrees, EabPosts;
static uint64 EabStartAlloc, EabAllocations;
typedef enum rolloverfaultstage { ROLLOVER_NO_FAULT, ROLLOVER_LOOKUP, ROLLOVER_WRITE } rolloverfaultstage;
static bool RolloverProbe, RolloverUncertain, RolloverRemoteApplied, RolloverApplyWrite, RolloverExportOom;
static rolloverfaultstage RolloverFaultStage;
static xerror* RolloverError;
static unsigned RolloverFailures, RolloverLookups, RolloverWrites, RolloverExports, RolloverSaves, RolloverDirtyFrees;
static xerrkind RolloverStoreFailure;
static uintptr_t RolloverPemAddress;
static size_t RolloverPemSize;
static bool RolloverPemPending;
static cstr RolloverExpectedPem;
static void eab_end_scan(void) {
    if(EabProbe && EabArmed && !EabEnded) {
        xmemdebugsnapshot S; xrtMemDebugSnapshot(&S);
        EabAllocations=S.AllocCount-EabStartAlloc; EabEnded=true;
        xrtMemDebugFailClear();
    }
}
static bool flow_eab_decode(cstr Text,size_t Size,bytes Output,size_t Capacity,size_t* Written,const xbase64config* Config) {
    bool DiagnosticFault=EabProbe && Text==EabText && EabFailAfter==-4;
    if(DiagnosticFault) testRequire(xrtMemDebugFailAfter(0),"EAB decoder diagnostic fault arm");
    bool Result=xrtBase64Decode(Text,Size,Output,Capacity,Written,Config);
    if(DiagnosticFault) {
        testRequire(!Result && xrtMemDebugFailTriggered(),"EAB decoder did not produce actual allocation failure");
        FirstError=xrtErrorRef(xrtGetError()); xrtMemDebugFailClear();
    }
    if(EabProbe && Text==EabText) {
        if(Result && Output) {
            testRequire(!EabPending,"EAB probe saw overlapping decoded keys");
            EabDecodedAddress=(uintptr_t)Output; EabPending=true;
            EabCapacity=Capacity; EabDecodedSize=*Written;
        }
        if(Result && !EabArmed) {
            xmemdebugsnapshot S; xrtMemDebugSnapshot(&S); EabStartAlloc=S.AllocCount; EabArmed=true;
            if(EabFailAfter>=0) testRequire(xrtMemDebugFailAfter((uint64)EabFailAfter),"EAB fault arm");
        }
    }
    return Result;
}
static void flow_eab_zero(ptr Address,size_t Size) {
    xrtSecureZero(Address,Size);
    if(EabProbe && Size==64u) EabLocalWipes++;
    if(EabProbe && EabPending && EabDecodedAddress==(uintptr_t)Address) {
        const uint8* Data=Address; uint8 Any=0;
        testRequire(Size>=EabCapacity,"EAB cleared only part of decoded key storage");
        for(size_t i=0;i<EabCapacity;i++) Any|=Data[i];
        testRequire(Any==0,"actual EAB secure wipe left bytes"); EabWipes++; EabPending=false;
    }
}
static void flow_eab_free(ptr Address) {
    if(EabProbe && Address && EabPending && EabDecodedAddress==(uintptr_t)Address) { EabDirtyFrees++; EabPending=false; }
    if(RolloverProbe && Address && RolloverPemPending && RolloverPemAddress==(uintptr_t)Address) {
        const uint8* Data=Address; uint8 Any=0;
        for(size_t i=0;i<RolloverPemSize;i++) Any|=Data[i];
        if(Any) RolloverDirtyFrees++;
        RolloverPemPending=false;
    }
    xrtFree(Address);
}
static str flow_account_export(const xacmees256key* Key) {
    if(RolloverProbe) {
        RolloverExports++;
        if(RolloverExportOom) testRequire(xrtMemDebugFailAfter(0),"rollover export fault arm");
    }
    str Result=xacmeKeyPemWrite(Key);
    if(RolloverProbe) {
        if(RolloverExportOom) {
            testRequire(!Result && xrtMemDebugFailTriggered(),"rollover export did not reach actual output allocation");
            RolloverError=xrtErrorRef(xrtGetError()); xrtMemDebugFailClear();
        }
        if(Result) {
            testRequire(!RolloverPemPending,"rollover exported overlapping private text");
            RolloverPemPending=true; RolloverPemAddress=(uintptr_t)Result; RolloverPemSize=strlen(Result);
        }
    }
    return Result;
}
static char LastNonce[512], Contact[401];
static const char KeyPem[]=
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGTAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBHkwdwIBAQQgya+p2EW6dRZrXCFX\n"
    "Z7HWk05Qw9s26JsSe4piKxIPZyGgCgYIKoZIzj0DAQehRANCAARg/tS6JVqdMclh\n"
    "63TGNW1owEm4kjth+mzmaWIuYPKftnkD/hAIuLyZpBrp6VYovGTy8bIMLX6fUXej\n"
    "wpTURiKZ\n-----END PRIVATE KEY-----\n";
static const char CertificatePem[]=ACME_FIXTURE_CHAIN;
static bool selected(cstr Name) { return Enabled && Active->Target && strcmp(Active->Target,Name)==0; }
static bool named(cstr Name) { return Enabled && strcmp(Active->Name,Name)==0; }
static void remember(void) { if(!FirstError && xrtGetError()) FirstError=xrtErrorRef(xrtGetError()); }
static void arm(cstr Target) {
    if(Inject && !Armed && selected(Target)) {
        testRequire(xrtMemDebugFailAfter(0),"flow arm failed"); Armed=true;
    }
}
static void* flow_malloc(size_t Size) {
    if(Size==sizeof(xacmecertkey)) arm("cert-key-alloc");
    else if(LastJson && strcmp(LastJson,"auth-poll-json")==0) arm("auth-status-copy");
    else if(LastJson && strcmp(LastJson,"order-ready-json")==0) arm("order-status-copy");
    void* Result=xrtMalloc(Size); if(!Result) remember(); return Result;
}
static xvalue* flow_json(xstrview Text) {
    if(LastJson) arm(LastJson);
    xvalue* Result=xrtJsonParse(Text); if(!Result) remember(); return Result;
}
static bool flow_key(cstr Pem,size_t Size,xacmees256key* Key) {
    arm("account-pem"); bool Result=xacmeKeyPemRead(Pem,Size,Key); if(!Result) remember(); return Result;
}
static bool flow_certkey(cstr Pem,size_t Size,xacmecertkey* Key) {
    arm("cert-key-pem"); bool Result=xacmeCertKeyReadPem(Pem,Size,Key); if(!Result) remember(); return Result;
}
static str flow_jwk(const xacmees256key* Key) {
    arm("eab-jwk"); str Result=xacmeJwkEcJson(Key); if(!Result) remember(); return Result;
}
static str flow_eab(cstr Kid,cstr Url,xstrview Jwk,const uint8* Mac,size_t Size) {
    arm("eab-sign"); str Result=xacmeJwsEabHs256(Kid,Url,Jwk,Mac,Size); if(!Result) remember(); return Result;
}
static str flow_sign(const xacmees256key* Key,const xacmejwsheader* H,xstrview Payload) {
    eab_end_scan();
    if(H->Url.Size==strlen("http://ca.example/alternate") && memcmp(H->Url.Data,"http://ca.example/alternate",H->Url.Size)==0)
        testRequire(Payload.Size==0u,"alternate was not a signed POST-as-GET");
    if(H->Nonce.Size<sizeof(LastNonce)) { if(H->Nonce.Size) memcpy(LastNonce,H->Nonce.Data,H->Nonce.Size); LastNonce[H->Nonce.Size]=0; }
    arm(Active && Active->Op==FLOW_CREATE?"account-sign":"order-sign");
    str Result=xacmeJwsEs256(Key,H,Payload); if(!Result) remember(); return Result;
}
static bool flow_thumb(const xacmees256key* Key,char* Output) {
    arm("thumbprint"); bool Result=xacmeJwkEcThumbprint(Key,Output); if(!Result) remember(); return Result;
}
static bool flow_csr(const xacmecertkey* Key,const xacmecsrconfig* Config,xbuffer* Output) {
    if(named("domain-duplicates")) testRequire(Config->DomainCount==1,"CSR SAN inputs differed from the distinct order identifiers");
    arm("csr"); bool Result=xacmeCsrBuild(Key,Config,Output); if(!Result) remember(); return Result;
}
static str flow_export(const xacmecertkey* Key) {
    arm("cert-key-export"); str Result=xacmeCertKeyPemWrite(Key); if(!Result) remember(); return Result;
}
static str flow_base64(const void* Data,size_t Size,const xbase64config* Config) {
    if(LastJson && strcmp(LastJson,"order-ready-json")==0) arm("csr-base64");
    else if(LastJson && strcmp(LastJson,"authz-json")==0) arm("txt-base64");
    str Result=xrtBase64EncodeNew(Data,Size,Config); if(!Result) remember(); return Result;
}
static bytes flow_decode(const xpemblock* Block,size_t* Size) {
    if(Active && Active->Op==FLOW_REVOKE) arm("revoke-der");
    else if(AlternateResponse) arm("alternate-der");
    else { CertificateDecodes++; arm(CertificateDecodes==2u?"certificate-second-der":"certificate-der"); }
    bytes Result=xrtPemDecodeNew(Block,Size); if(!Result) remember(); return Result;
}
static xx509result flow_name_equal(xbytesview Left,xbytesview Right) {
    arm("certificate-name"); xx509result Result=xrtX509NameEqual(Left,Right); if(Result==X509_ERROR) remember(); return Result;
}
/* Resolver fault controls must not depend on OS scheduling or wall time. */
static double flow_clock(void) { return selected("resolver")?ResolverClock:xrtTimer(); }
static double flow_after(int64 Ms) { return Ms == XRT_WAIT_FOREVER ? INFINITY : nextafter(flow_clock() + (double)Ms / 1000.0, INFINITY); }
static void flow_sleep(int64 Ms) { Sleeps++; if(selected("resolver")) ResolverClock+=(double)Ms/1000.0; }
static bool flow_init(xacmehttp* Http,struct xnetengine* Engine,cstr Ca,int64 Timeout) {
    (void)Engine; (void)Ca; memset(Http,0,sizeof(*Http)); Http->uTimeoutMs=Timeout; return true;
}
static str duplicate(cstr Text) {
    if(!Text) return NULL;
    size_t Size=strlen(Text)+1u; str Result=(str)xrtMalloc(Size);
    testRequire(Result!=NULL,"controlled response allocation failed"); memcpy(Result,Text,Size); return Result;
}
static xerror* ReadRetryError;
static unsigned ReadRetryCalls, ReadRetryFailures;
static bool ReadRetryProbe, ReadRetryUncertain;
static bool flow_exchange(xacmehttp* Http,cstr Method,cstr Url,cstr Type,xstrview Body,xacmehttpresponse* R) {
    (void)Http; (void)Method; (void)Type;
    cstr Stage=NULL,Json=NULL,Location=NULL,Nonce="bm9uY2U"; unsigned Status=200;
    memset(R,0,sizeof(*R)); Requests++;
    if(RolloverProbe && (strcmp(Url,"http://ca.example/account-new")==0 || strcmp(Url,"http://ca.example/key-change")==0)) {
        bool Lookup=strcmp(Url,"http://ca.example/account-new")==0;
        if(Lookup) RolloverLookups++;
        else { RolloverWrites++; RolloverRemoteApplied=RolloverApplyWrite; }
        if(RolloverFailures && RolloverFaultStage==(Lookup?ROLLOVER_LOOKUP:ROLLOVER_WRITE)) {
            RolloverFailures--; xrtSetError(RolloverError); Http->bWriteUncertain=RolloverUncertain; return false;
        }
        Http->bWriteUncertain=false;
        R->iStatus=(uint16)(Lookup?(RolloverRemoteApplied?200u:400u):200u);
        R->sBody=duplicate(Lookup?(RolloverRemoteApplied?"{\"status\":\"valid\"}":
            "{\"type\":\"urn:ietf:params:acme:error:accountDoesNotExist\"}"):"{}");
        R->iBodySize=strlen(R->sBody); R->sReplayNonce=duplicate("ZnJlc2g");
        if(Lookup && RolloverRemoteApplied) R->sLocation=duplicate("http://ca.example/account");
        return true;
    }
    if(EabProbe && strcmp(Url,"http://ca.example/nonce")==0) eab_end_scan();
    if(EabProbe && strcmp(Url,"http://ca.example/account-new")==0) {
        EabPosts++;
        if(EabWire) printf("[eab-wire] %zu %s %.*s\n",EabDecodedSize,EabText,(int)Body.Size,Body.Data);
    }
    if(ReadRetryProbe && strcmp(Url,"http://ca.example/read-retry")==0) {
        ReadRetryCalls++;
        if(ReadRetryFailures) {
            ReadRetryFailures--; xrtSetError(ReadRetryError);
            Http->bWriteUncertain=ReadRetryUncertain; return false;
        }
        Http->bWriteUncertain=false; R->iStatus=200; R->sBody=duplicate("{}"); R->iBodySize=2;
        R->sReplayNonce=duplicate("bm9uY2U"); return true;
    }
    if(strcmp(Url,"http://ca.example/dir")==0) {
        Stage="directory"; LastJson="directory-json";
        Json="{\"newNonce\":\"http://ca.example/nonce\",\"newAccount\":\"http://ca.example/account-new\",\"newOrder\":\"http://ca.example/order-new\",\"revokeCert\":\"http://ca.example/revoke\",\"keyChange\":\"http://ca.example/key-change\"}";
        if(named("directory-malformed")) Json="{";
        if(named("directory-nul")) Json="{\"newNonce\":\"http://ca.example/nonce\\u0000suffix\",\"newAccount\":\"http://ca.example/account-new\",\"newOrder\":\"http://ca.example/order-new\"}";
        if(named("optional-endpoint-nul")) Json="{\"newNonce\":\"http://ca.example/nonce\",\"newAccount\":\"http://ca.example/account-new\",\"newOrder\":\"http://ca.example/order-new\",\"revokeCert\":\"http://ca.example/revoke\\u0000suffix\"}";
    } else if(strcmp(Url,"http://ca.example/nonce")==0) {
        Stage="nonce"; LastJson=NULL; NonceRequests++; Status=204;
        if(named("invalid-nonce")) Nonce="invalid nonce+";
        if(named("nonce-status")) Status=503;
    } else if(strcmp(Url,"http://ca.example/account-new")==0) {
        Stage="account"; LastJson="account-json"; Status=201; Location="http://ca.example/account"; Json="{\"status\":\"valid\"}";
        if(InOperation && Active->Op==FLOW_ROLLOVER) {
            LookupRequests++; Status=400; Location=NULL; Json="{\"type\":\"urn:ietf:params:acme:error:accountDoesNotExist\"}";
            if(named("rollover-lookup-nonce")) {
                if(LookupRequests==1) { Json="{\"type\":\"urn:ietf:params:acme:error:badNonce\"}"; Nonce="ZnJlc2g"; }
                else testRequire(strcmp(LastNonce,"ZnJlc2g")==0,"rollover lookup did not use response nonce");
            }
        }
        if(named("account-status-nul")) Json="{\"status\":\"valid\\u0000deactivated\"}";
        if(named("contact-long")) {
            xvalue* Envelope=xrtJsonParse(Body); xstrview Encoded; size_t Size; static const xbase64config B64={NULL,XBASE64_URL|XBASE64_NO_PADDING};
            testRequire(Envelope && xrtValueGetString(xrtValueObjectGet(Envelope,XRT_STR_LITERAL("payload")),&Encoded),"contact JWS envelope invalid");
            bytes Data=xrtBase64DecodeNew(Encoded.Data,Encoded.Size,&Size,&B64); xvalue* Payload=Data?xrtJsonParse((xstrview){(cstr)Data,Size}):NULL; xstrview Mail;
            xvalue* Contacts=Payload?xrtValueObjectGet(Payload,XRT_STR_LITERAL("contact")):NULL;
            testRequire(Contacts && xrtValueGetString(xrtValueArrayGet(Contacts,0),&Mail) && Mail.Size==7u+strlen(Contact) &&
                memcmp(Mail.Data,"mailto:",7)==0 && memcmp(Mail.Data+7,Contact,strlen(Contact))==0,"flow silently truncated contact");
            xrtValueRelease(Payload); xrtFree(Data); xrtValueRelease(Envelope);
        }
    } else if(strcmp(Url,"http://ca.example/order-new")==0) {
        Stage="new-order"; LastJson="new-order-json"; Status=201; Location="http://ca.example/order"; AuthzRequests=OrderRequests=0; Finalized=false;
        Json="{\"status\":\"pending\",\"authorizations\":[\"http://ca.example/authz\"]}";
        if(named("authorization-url-nul")) Json="{\"status\":\"pending\",\"authorizations\":[\"http://ca.example/authz\\u0000suffix\"]}";
    } else if(strcmp(Url,"http://ca.example/authz")==0) {
        Stage="authz"; AuthzRequests++; LastJson=AuthzRequests==1?"authz-json":"auth-poll-json";
        Json=AuthzRequests==1?"{\"status\":\"pending\",\"identifier\":{\"type\":\"dns\",\"value\":\"example.com\"},\"challenges\":[{\"type\":\"dns-01\",\"token\":\"TOKEN\",\"url\":\"http://ca.example/challenge\"}]}":"{\"status\":\"valid\"}";
        if(named("authorization-status-nul") && AuthzRequests==1) Json="{\"status\":\"valid\\u0000invalid\"}";
        if(named("authorization-identifier-nul") && AuthzRequests==1) Json="{\"status\":\"pending\",\"identifier\":{\"type\":\"dns\",\"value\":\"example.com\\u0000suffix\"},\"challenges\":[{\"type\":\"dns-01\",\"token\":\"TOKEN\",\"url\":\"http://ca.example/challenge\"}]}";
        if(named("auth-poll-malformed") && AuthzRequests>1) Json="{";
        if(named("challenge-detail-shape") && AuthzRequests==2) Json="{\"status\":\"invalid\"}";
        if(named("challenge-detail-shape") && AuthzRequests>2) Json="[]";
    } else if(strcmp(Url,"http://ca.example/challenge")==0) { Stage="challenge"; LastJson=NULL; Json="{}";
    } else if(strcmp(Url,"http://ca.example/order")==0) {
        OrderRequests++; Stage="order"; LastJson=!Finalized?"order-ready-json":(OrderRequests==2?"order-valid-json":"certificate-url-json");
        Json=Finalized?"{\"status\":\"valid\",\"certificate\":\"http://ca.example/certificate\"}":"{\"status\":\"ready\",\"finalize\":\"http://ca.example/finalize\"}";
        if(named("finalize-url-missing") && !Finalized) Json="{\"status\":\"ready\"}";
    } else if(strcmp(Url,"http://ca.example/finalize")==0) { Stage="finalize"; LastJson=NULL; Json="{\"status\":\"valid\"}"; Finalized=true;
    } else if(strcmp(Url,"http://ca.example/certificate")==0) {
        Stage="certificate"; LastJson=NULL; Json=CertificatePem;
        CertificateDecodes=0u; AlternateResponse=false;
        if(named("certificate-json")) Json="{}";
        if(named("certificate-private-key")) Json=ACME_FIXTURE_CHAIN ACME_FIXTURE_KEY;
        if(named("certificate-invalid-der")) Json="-----BEGIN CERTIFICATE-----\nAQID\n-----END CERTIFICATE-----\n";
        if(named("certificate-wrong-key")) Json=ACME_FIXTURE_OTHER_KEY_LEAF ACME_FIXTURE_CA;
        if(named("certificate-wrong-domain")) Json=ACME_FIXTURE_OTHER_DOMAIN_LEAF ACME_FIXTURE_CA;
        if(named("certificate-name")) Json=ACME_FIXTURE_NAME_LEAF ACME_FIXTURE_NAME_CA;
        if(named("certificate-rsa-signature")) Json=ACME_FIXTURE_RSA_SIGNED_LEAF ACME_FIXTURE_RSA_CA;
        if(named("certificate-expired")) Json=ACME_FIXTURE_EXPIRED_LEAF ACME_FIXTURE_CA;
        if(named("certificate-future")) Json=ACME_FIXTURE_FUTURE_LEAF ACME_FIXTURE_CA;
        if(named("certificate-extra-domain")) Json=ACME_FIXTURE_EXTRA_DOMAIN_LEAF ACME_FIXTURE_CA;
        if(named("certificate-no-san")) Json=ACME_FIXTURE_NO_SAN_LEAF ACME_FIXTURE_CA;
        if(named("certificate-ip-san")) Json=ACME_FIXTURE_IP_SAN_LEAF ACME_FIXTURE_CA;
        if(named("certificate-wrong-ca")) Json=ACME_FIXTURE_LEAF ACME_FIXTURE_WRONG_CA;
        if(named("certificate-bad-signature")) Json=ACME_FIXTURE_BAD_SIGNATURE_LEAF ACME_FIXTURE_CA;
        if(named("certificate-not-ca")) Json=ACME_FIXTURE_CHAIN ACME_FIXTURE_LEAF;
        if(named("certificate-garbage")) Json=ACME_FIXTURE_CHAIN "untrusted trailing text\n";
        if(named("certificate-count")) Json=ACME_FIXTURE_CHAIN ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA ACME_FIXTURE_CA;
        if(named("certificate-leaf-only")) Json=ACME_FIXTURE_LEAF;
        if(Active && strcmp(Active->Name,"certificate-rsa")==0) Json=ACME_FIXTURE_RSA_LEAF ACME_FIXTURE_CA;
    } else if(strcmp(Url,"http://ca.example/alternate")==0) {
        Stage="certificate"; LastJson=NULL; AlternateResponse=true; AlternateRequests++;
        Json=ACME_FIXTURE_LEAF ACME_FIXTURE_ALT_CA;
        if(named("alternate-json")) Json="{}";
        if(named("alternate-private-key")) Json=ACME_FIXTURE_LEAF ACME_FIXTURE_ALT_CA ACME_FIXTURE_KEY;
        if(named("alternate-different-leaf")) Json=ACME_FIXTURE_OTHER_LEAF ACME_FIXTURE_ALT_CA;
        if(named("alternate-wrong-ca")) Json=ACME_FIXTURE_LEAF ACME_FIXTURE_WRONG_CA;
    } else if(strcmp(Url,"http://ca.example/revoke")==0) {
        Stage="revoke"; LastJson="revoke-json"; Json="{}";
        if(named("revoke-already")) { Status=400; Json="{\"type\":\"urn:ietf:params:acme:error:alreadyRevoked\"}"; }
        if(named("revoke-detail")) { Status=400; Json="{\"type\":\"urn:ietf:params:acme:error:unauthorized\",\"detail\":\"not alreadyRevoked\"}"; }
        if(named("revoke-type-nul")) { Status=400; Json="{\"type\":\"urn:ietf:params:acme:error:alreadyRevoked\\u0000suffix\"}"; }
    } else if(strcmp(Url,"http://ca.example/account")==0) {
        Stage="deactivate"; LastJson="deactivate-json"; Json="{\"status\":\"deactivated\"}";
        if(named("deactivate-status-nul")) Json="{\"status\":\"deactivated\\u0000valid\"}";
        if(named("badnonce-limit") || (!FirstReply && (named("badnonce-detail") || named("badnonce-fresh") || named("badnonce-missing") || named("badnonce-invalid")))) {
            FirstReply=true; Status=400; Json=named("badnonce-detail")?"{\"type\":\"urn:ietf:params:acme:error:unauthorized\",\"detail\":\"not badNonce\"}":"{\"type\":\"urn:ietf:params:acme:error:badNonce\"}";
            Nonce=named("badnonce-missing")?NULL:"ZnJlc2g";
            if(named("badnonce-invalid")) Nonce="invalid+nonce";
        } else if(named("badnonce-fresh")) testRequire(strcmp(LastNonce,"ZnJlc2g")==0,"badNonce retry did not use response nonce");
    } else if(strcmp(Url,"http://ca.example/key-change")==0) {
        Stage="key-change"; LastJson="key-change-json"; Json="{}"; KeyChangeRequests++;
        if(named("rollover-outer-nonce")) {
            if(KeyChangeRequests==1) { Status=400; Json="{\"type\":\"urn:ietf:params:acme:error:badNonce\"}"; Nonce="ZnJlc2g"; }
            else testRequire(strcmp(LastNonce,"ZnJlc2g")==0,"rollover outer JWS did not use response nonce");
        }
    } else { xrtSetErrorInfo(XERR_PROTOCOL,"test.flow.http",91,"unexpected controlled endpoint"); remember(); return false; }
    if(Active && Active->Kind==FLOW_HTTP_FAIL && selected(Stage)) {
        if(Inject) { arm(Stage); void* Allocation=xrtMalloc(1); testRequire(!Allocation && xrtMemDebugFailTriggered(),"transport cause injection failed"); }
        else xrtSetErrorInfo(XERR_IO,"test.flow.http",77,"controlled transport cause");
        remember(); return false;
    }
    R->iStatus=(uint16)Status; R->sBody=duplicate(Json); R->iBodySize=Json?strlen(Json):0;
    R->sReplayNonce=duplicate(Nonce); R->sLocation=duplicate(Location);
    if(Stage && strcmp(Stage,"certificate")==0) {
        R->sContentType=duplicate(named("certificate-type")?"application/json":
            named("certificate-missing-type")?NULL:named("certificate-type-parameters")?
            " Application/PEM-Certificate-Chain ; charset=us-ascii":"application/pem-certificate-chain");
        if(!AlternateResponse && Enabled && strncmp(Active->Name,"alternate-",10)==0)
            R->sLink=duplicate(named("alternate-path")?"<alternate>; rel=alternate":
                named("alternate-dot-path")?"<./path/../alternate>; rel=alternate":
                named("alternate-network-path")?"<//ca.example/alternate>; rel=alternate":
                "<http://ca.example/alternate>; rel=alternate");
    }
    if(selected("nonce")) arm("nonce");
    return true;
}
static bool flow_store_failure(cstr Stage) {
    if(!selected(Stage)) return false;
    if(Inject) {
        arm(Stage); void* Allocation=xrtMalloc(1);
        testRequire(!Allocation && xrtMemDebugFailTriggered(),"store cause injection failed");
    } else xrtSetErrorInfo(XERR_IO,"test.flow.store",78,"controlled persistence cause");
    remember(); return true;
}
static bool flow_need_renew(cstr Root,cstr Domain,int Days,bool* Need) {
    (void)Root;(void)Domain;(void)Days; *Need=true; return true;
}
static bool flow_save_grant(cstr Root,cstr Domain,const xacmeissuegrant* Grant,cstr Url) {
    (void)Root;(void)Domain;(void)Grant;(void)Url; return !flow_store_failure("store-grant");
}
static bool flow_save_account(cstr Root,cstr Url,cstr Pem) {
    if(RolloverProbe) {
        RolloverSaves++;
        testRequire(Root && strcmp(Root,"fixture-store")==0 && Url && strcmp(Url,"http://ca.example/dir")==0 &&
            Pem && RolloverPemPending && (uintptr_t)Pem==RolloverPemAddress && strcmp(Pem,RolloverExpectedPem)==0,
            "rollover store did not receive the new key's live owned export");
        if(RolloverStoreFailure!=XERR_NONE) { xrtSetError(RolloverError); return false; }
        return true;
    }
    (void)Root;(void)Url;(void)Pem; return !flow_store_failure("store-account");
}

static bool flow_dns_query(xacmedns* Dns,cstr Resolver,uint16 Port,cstr Fqdn,
    char (*Records)[XACME_TXT_RECORD_MAX],size_t Capacity,size_t* Count) {
    if(!selected("resolver")) return xacmeDnsTxtQuery(Dns,Resolver,Port,Fqdn,Records,Capacity,Count);
    ResolverQueries++; *Count=0;
    if(ResolverEndpointExpected) {
        xnetaddr Actual,Expected;
        testRequire(xrtNetAddrParse(&Actual,Resolver,Port) &&
            xrtNetAddrParse(&Expected,ResolverEndpointExpected,ResolverEndpointPort) &&
            xrtNetAddrEqual(&Actual,&Expected),"propagation queried a different resolver endpoint");
    }
    if(named("resolver-advisory") ||
       ((named("resolver-memory-second") || named("resolver-fallback")) && ResolverQueries==1u)) {
        xrtSetErrorInfo(XERR_TIMEOUT,"test.flow.resolver",91,"resolver temporarily unavailable"); return false;
    }
    if(Inject && !Armed) {
        arm("resolver"); void* Allocation=xrtMalloc(1);
        testRequire(!Allocation && xrtMemDebugFailTriggered(),"resolver memory cause injection failed"); remember(); return false;
    }
    xrtClearError(); strcpy(Records[0],ResolverTxt); *Count=1; return true;
}
#define xacmeDnsTxtQuery flow_dns_query
#define xacmeHttpInit flow_init
#define xacmeHttpExchange flow_exchange
#define xrtJsonParse flow_json
#undef xrtMalloc
#define xrtMalloc flow_malloc
#define xacmeKeyPemRead flow_key
#define xacmeCertKeyReadPem flow_certkey
#define xacmeJwkEcJson flow_jwk
#define xacmeJwsEabHs256 flow_eab
#define xacmeJwsEs256 flow_sign
#define xacmeJwkEcThumbprint flow_thumb
#define xacmeCsrBuild flow_csr
#define xacmeCertKeyPemWrite flow_export
#define xacmeKeyPemWrite flow_account_export
#define xrtBase64EncodeNew flow_base64
#define xrtPemDecodeNew flow_decode
#define xrtX509NameEqual flow_name_equal
#define xrtSleep flow_sleep
#define xrtTimer flow_clock
#define __xrtWaitAfter flow_after
#define xrtAcmeStoreNeedRenew flow_need_renew
#define xrtAcmeStoreSaveGrant flow_save_grant
#define xrtAcmeStoreSaveAccount flow_save_account
#define xrtBase64Decode flow_eab_decode
#define xrtSecureZero flow_eab_zero
#undef xrtFree
#define xrtFree flow_eab_free
#include "../../src/acme/xacme_flow.c"
#undef xrtFree
#define xrtFree(pMemory) xrtFreeAt((pMemory), __FILE__, (uint32)__LINE__)
#undef xrtSecureZero
#undef xrtBase64Decode
#undef xacmeDnsTxtQuery
#undef xacmeHttpInit
#undef xacmeHttpExchange
#undef xrtJsonParse
#undef xrtMalloc
#define xrtMalloc(iSize) xrtMallocAt((iSize), __FILE__, (uint32)__LINE__)
#undef xacmeKeyPemRead
#undef xacmeCertKeyReadPem
#undef xacmeJwkEcJson
#undef xacmeJwsEabHs256
#undef xacmeJwsEs256
#undef xacmeJwkEcThumbprint
#undef xacmeCsrBuild
#undef xacmeCertKeyPemWrite
#undef xacmeKeyPemWrite
#undef xrtBase64EncodeNew
#undef xrtPemDecodeNew
#undef xrtX509NameEqual
#undef xrtSleep
#undef xrtTimer
#undef __xrtWaitAfter
#undef xrtAcmeStoreNeedRenew
#undef xrtAcmeStoreSaveGrant
#undef xrtAcmeStoreSaveAccount

static bool dns_add(xacmednsprovider* D,xstrview F,xstrview T) {
    (void)D;(void)F; Adds++;
    testRequire(T.Size<sizeof(ResolverTxt),"resolver fixture TXT overflow");
    memcpy(ResolverTxt,T.Data,T.Size); ResolverTxt[T.Size]=0; return true;
}
static bool dns_remove(xacmednsprovider* D,xstrview F,xstrview T) {
    (void)D;(void)F;(void)T; Removes++;
    if(named("cleanup-cause")) { xrtSetErrorInfo(XERR_STATE,"test.flow.dns",99,"cleanup cause"); return false; }
    return true;
}
static bool dns_propagate(xacmednsprovider* D,xstrview F,xstrview T) {
    (void)D;(void)F;(void)T;
    if(Inject && selected("propagate")) {
        arm("propagate"); void* Allocation=xrtMalloc(1);
        testRequire(!Allocation && xrtMemDebugFailTriggered(),"propagation cause injection failed"); remember(); return false;
    }
    return true;
}
static struct xacmeclient* create_client(bool Special) {
    InOperation=false;
    xacmeaccountconfig Account; xacmeclientconfig Config;
    xrtAcmeAccountConfigInit(&Account); xrtAcmeClientConfigInit(&Config);
    Account.sDirectoryUrl="http://ca.example/dir"; Account.sAccountKeyPem=KeyPem;
    if(Special && (named("eab-jwk") || named("eab-sign"))) { Account.Eab.sKid="fixture-kid"; Account.Eab.sHmac="dGVzdC1zZWNyZXQ"; }
    if(Special && named("contact-long")) { memset(Contact,'a',400); Contact[400]=0; Account.sContactEmail=Contact; }
    /* The controlled CA signs the matching fixed leaf; independent mock/Pebble
     * tests exercise generation and validate the actual CSR. */
    Config.sCertKeyPem=ACME_FIXTURE_KEY;
    if(Active && strcmp(Active->Name,"certificate-rsa")==0) Config.sCertKeyPem=ACME_FIXTURE_RSA_KEY;
    if(Active && strncmp(Active->Name,"resolver-",9u)==0) {
        static const cstr Resolvers[]={"127.0.0.1:1","127.0.0.1:2","127.0.0.1:3"};
        Config.sPropagateResolvers=Resolvers; Config.iPropagateResolverCount=3u; Config.uPropagateTimeoutMs=1u;
    }
    Config.pAccount=&Account; Config.uIssueTimeoutMs=INT64_C(10000); return xrtAcmeClientCreate(&Config);
}
static bool operation(struct xacmeclient* Client) {
    InOperation=true;
    xacmednsprovider Dns={"fixture",XACME_DNS_CAP_PROPAGATE,NULL,dns_add,dns_remove,dns_propagate};
    if(selected("resolver")) { Dns.iCaps=0u; Dns.Propagate=NULL; }
    xacmeissuegrant Grant; xstrview Domain=XRT_STR_LITERAL("example.com"); char Large[600];
    if(Active->Op==FLOW_ISSUE || Active->Op==FLOW_STORED) {
        if(strcmp(Active->Name,"account-cert-key")==0) {
            cstr Pem=Enabled?KeyPem:ACME_FIXTURE_KEY;
            xacmeCertKeyUnit(Client->pCertKey);
            testRequire(xacmeCertKeyReadPem(Pem,strlen(Pem),Client->pCertKey),"account key reuse fixture failed");
        }
        if(named("domain-overflow")) { memset(Large,'a',sizeof(Large)); Domain=(xstrview){Large,sizeof(Large)}; }
        if(named("domain-nul")) Domain=(xstrview){"example.com\0suffix",18};
        if(selected("domain")) arm("domain");
        xstrview Duplicates[17];
        for(size_t i=0;i<17;i++) Duplicates[i]=Domain;
        bool Renewed=false;
        bool Result=Active->Op==FLOW_STORED?xrtAcmeClientIssueStored(Client,&Domain,1,&Dns,"fixture-store",30,&Grant,&Renewed):
            xrtAcmeClientIssueEx(Client,named("domain-duplicates")?Duplicates:&Domain,named("domain-duplicates")?17u:1u,&Dns,
                Enabled && strncmp(Active->Name,"alternate-",10)==0,&Grant);
        if(Active->Op==FLOW_STORED) testRequire(Renewed==Result,"stored operation reported a renewal after save failure");
        if(Result) testRequire(Grant.sFullchainPem && Grant.sKeyPem,"successful issue did not deliver a pair");
        else testRequire(!Grant.sFullchainPem && !Grant.sKeyPem,"failed issue delivered partial output");
        if(Result && Enabled && strncmp(Active->Name,"alternate-",10)==0) {
            testRequire(AlternateRequests==1u,"alternate download was not attempted exactly once");
            bool Adopt=strcmp(Active->Name,"alternate-valid")==0 || strcmp(Active->Name,"alternate-path")==0 ||
                strcmp(Active->Name,"alternate-dot-path")==0 || strcmp(Active->Name,"alternate-network-path")==0 ||
                (strcmp(Active->Name,"alternate-der")==0 && !Inject);
            testRequire(strcmp(Grant.sFullchainPem,Adopt?ACME_FIXTURE_LEAF ACME_FIXTURE_ALT_CA:CertificatePem)==0,
                "invalid alternate replaced primary or valid alternate was ignored");
            testRequire(!xrtGetError(),"alternate fallback leaked its advisory error");
        }
        xrtAcmeGrantUnit(&Grant); return Result;
    }
    if(Active->Op==FLOW_REVOKE) return xrtAcmeClientRevoke(Client,CertificatePem,-1);
    if(Active->Op==FLOW_DEACTIVATE) return xrtAcmeClientDeactivate(Client);
    xacmees256key NewKey; testRequire(xacmeEs256Generate(&NewKey),"rollover fixture key generation failed");
    str NewPem=xacmeKeyPemWrite(&NewKey); testRequire(NewPem!=NULL,"rollover fixture export failed");
    bool Result=xrtAcmeClientRollover(Client,NewPem,strcmp(Active->Name,"rollover-store")==0?"fixture-store":NULL);
    if(strcmp(Active->Name,"rollover-store")==0) {
        str Applied=xrtAcmeClientAccountPem(Client);
        testRequire(Applied && strcmp(Applied,NewPem)==0,"rollover persistence failure lost the applied new key");
        xrtFree(Applied);
    }
    xrtFree(NewPem); xrtSecureZero(&NewKey,sizeof(NewKey)); return Result;
}
static void read_retry_policy(cstr Only) {
    static const struct {
        cstr Name; xerrkind Kind; bool Uncertain; unsigned Failures; bool Success; unsigned Calls, Sleeps;
    } Rows[]={
        {"memory-once",XERR_MEMORY,true,1,false,1,0},
        {"memory-persistent",XERR_MEMORY,true,4,false,1,0},
        {"memory-plain",XERR_MEMORY,false,1,false,1,0},
        {"io-once",XERR_IO,true,1,true,2,1},
        {"io-exhausted",XERR_IO,true,4,false,3,2},
        {"timeout-once",XERR_TIMEOUT,true,1,true,2,1},
        {"timeout-exhausted",XERR_TIMEOUT,true,4,false,3,2},
        {"io-plain",XERR_IO,false,1,false,1,0}
    };
    bool Matched=Only==NULL;
    for(size_t i=0;i<sizeof(Rows)/sizeof(Rows[0]);++i) {
        if(Only && strcmp(Only,Rows[i].Name)!=0) continue;
        Matched=true;Active=&Cases[0];Enabled=Inject=Armed=false;FirstError=NULL;xrtClearError();
        struct xacmeclient* Client=create_client(false);testRequire(Client!=NULL,"read retry client fixture");
        if(Rows[i].Kind==XERR_MEMORY) {
            testRequire(xrtMemDebugFailAfter(0),"read worker allocation fault arm");
            testRequire(xrtMalloc(1)==NULL && xrtMemDebugFailTriggered(),"actual read worker MEMORY cause");
            xrtMemDebugFailClear();
        } else xrtSetErrorInfo(Rows[i].Kind,"test.flow.read.worker",82,"read producer failed");
        xerror* Root=xrtErrorRef(xrtGetError());testRequire(Root!=NULL,"read worker immutable cause");
        /* The producer's fault injection has ended before HTTP publishes its
         * immutable error. Retry is governed by the delivered error kind. */
        ReadRetryError=Rows[i].Uncertain?xrtErrorWrap(Root,Rows[i].Kind,"xrt.acme.http",
            XACME_HTTP_ERROR_UNCERTAIN,"read result unknown"):xrtErrorRef(Root);
        testRequire(ReadRetryError!=NULL,"read error context publication");xrtClearError();
        ReadRetryCalls=Requests=NonceRequests=Sleeps=0;ReadRetryFailures=Rows[i].Failures;
        ReadRetryProbe=true;ReadRetryUncertain=Rows[i].Uncertain;
        xacmehttpresponse Response;bool Result=xacmeFlowPostAsGet(Client,"http://ca.example/read-retry",&Response);
        printf("[read-retry] %s success=%d calls=%u nonce=%u sleeps=%u kind=%d same-cause=%d\n",
            Rows[i].Name,Result,ReadRetryCalls,NonceRequests,Sleeps,(int)xrtErrorKind(xrtGetError()),xrtGetError()==ReadRetryError);
        testRequire(Result==Rows[i].Success && ReadRetryCalls==Rows[i].Calls &&
            Sleeps==Rows[i].Sleeps && NonceRequests==Rows[i].Calls-1u,"read retry policy replayed terminal fault or lost bounded recovery");
        if(Result) testRequire(xrtGetError()==NULL && Response.iStatus==200u,"read recovery retained stale diagnostic");
        else testRequire(xrtGetError()==ReadRetryError && xrtErrorKind(xrtGetError())==Rows[i].Kind &&
            (!Rows[i].Uncertain || xrtErrorCause(ReadRetryError)==Root) && Response.sBody==NULL && Response.sReplayNonce==NULL,
            "read retry lost actual cause or delivered partial output");
        xacmeHttpResponseUnit(&Response);
        /* A separate caller request remains usable after a terminal failure. */
        ReadRetryFailures=ReadRetryCalls=0;unsigned PriorSleeps=Sleeps;
        testRequire(xacmeFlowPostAsGet(Client,"http://ca.example/read-retry",&Response) &&
            ReadRetryCalls==1u && Sleeps==PriorSleeps && xrtGetError()==NULL,"explicit read retry cannot recover on same client");
        xacmeHttpResponseUnit(&Response);ReadRetryProbe=false;xrtAcmeClientDestroy(Client);
        xrtErrorFree(ReadRetryError);ReadRetryError=NULL;xrtErrorFree(Root);xrtClearError();
        testMemoryDebugDrain("read retry policy retained logical allocations");
        printf("[PASS] FLOW read-retry/%s\n",Rows[i].Name);
    }
    testRequire(Matched,"unknown read retry policy row");
}
static void resolver_endpoints(void) {
    static const struct {cstr Text,Host;uint16 Port;} Rows[]={
        {"127.0.0.1","127.0.0.1",53},{"127.0.0.1:5353","127.0.0.1",5353},
        {"1.1.1.1:00053","1.1.1.1",53},{"::1","::1",53},{"[::1]","::1",53},
        {"[::1]:5353","::1",5353},{"2001:db8::53","2001:db8::53",53},
        {"2001:db8::1:5353","2001:db8::1:5353",53},
        {"[2001:db8::1:5353]:65535","2001:db8::1:5353",65535},
        {"::","::",53},{"::ffff:127.0.0.1","::ffff:127.0.0.1",53},
        {"[::ffff:127.0.0.1]:53","::ffff:127.0.0.1",53},
        {"fe80::1%7","fe80::1%7",53},{"[fe80::1%7]:5353","fe80::1%7",5353},
        {"2001:DB8:0:0:0:0:0:1","2001:db8::1",53},
        {"",NULL,0},{"127.0.0.1:",NULL,0},{"127.0.0.1:0",NULL,0},
        {"127.0.0.1:65536",NULL,0},{"127.0.0.1:999999999999999999999999",NULL,0},
        {"127.0.0.1:53junk",NULL,0},{"127.0.0.1:+53",NULL,0},{"127.0.0.1:-1",NULL,0},
        {"127.0.0.1: 53",NULL,0},{"127.0.0.1:53 ",NULL,0},{":53",NULL,0},
        {"[::1",NULL,0},{"[::1]junk",NULL,0},{"[::1]x:53",NULL,0},
        {"[::1]:",NULL,0},{"[::1]:0",NULL,0},{"[::1]:65536",NULL,0},
        {"resolver.example:53",NULL,0},{"127.1:53",NULL,0},{"127.000.000.001:53",NULL,0},
        {"[127.0.0.1]:53",NULL,0},{"2001:db8::1:99999",NULL,0}
    };
    size_t Failures=0;
    for(size_t i=0;i<sizeof(Rows)/sizeof(Rows[0]);++i) {
        char Host[64];memset(Host,0xa5,sizeof(Host));xrtClearError();
        xmemdebugsnapshot Before,After;xrtMemDebugSnapshot(&Before);
        uint16 Port=xacmeFlowResolverPort(Rows[i].Text,Host,sizeof(Host));
        bool Match=Port==Rows[i].Port;
        if(Rows[i].Host) {
            xnetaddr Actual,Expected;
            Match=Match && xrtNetAddrParse(&Actual,Host,Port) &&
                xrtNetAddrParse(&Expected,Rows[i].Host,Rows[i].Port) && xrtNetAddrEqual(&Actual,&Expected);
            xrtMemDebugSnapshot(&After);
            Match=Match && After.AllocCount==Before.AllocCount;
        } else Match=Match && Host[0]==0 && xrtGetError()!=NULL;
        if(!Match) {fprintf(stderr,"resolver endpoint row=%zu input=%s port=%u host=%s\n",i,Rows[i].Text,(unsigned)Port,Host);Failures++;}
        xrtClearError();testMemoryDebugDrain("resolver parser retained allocations");
    }
    testRequire(Failures==0,"resolver endpoint parser changed address or accepted malformed input");
    struct {char Host[4];uint8 Guard[8];} Exact;
    memset(&Exact,0xa5,sizeof(Exact));
    testRequire(xacmeFlowResolverPort("[::1]:5353",Exact.Host,sizeof(Exact.Host))==5353 &&
        strcmp(Exact.Host,"::1")==0,"resolver exact host capacity");
    testRequire(xacmeFlowResolverPort("[::1]:5353",Exact.Host,3u)==0 && Exact.Host[0]==0,"resolver must not use a truncated address");
    for(size_t i=0;i<sizeof(Exact.Guard);++i) testRequire(Exact.Guard[i]==0xa5,"resolver wrote past host capacity");
    xrtClearError();testRequire(xacmeFlowResolverPort(NULL,Exact.Host,sizeof(Exact.Host))==0 && Exact.Host[0]==0 &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"resolver NULL input");
    xrtClearError();testRequire(xacmeFlowResolverPort("::1",NULL,4u)==0 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"resolver NULL output");
    xrtClearError();Exact.Host[0]='x';testRequire(xacmeFlowResolverPort("::1",Exact.Host,0)==0 && Exact.Host[0]=='x' &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"resolver zero capacity");
    xrtClearError();testMemoryDebugDrain("resolver argument probes retained allocations");
    printf("[PASS] FLOW resolver endpoint parsing (%zu vectors plus capacity/argument checks)\n",sizeof(Rows)/sizeof(Rows[0]));
    {
        const flowcase Probe={"resolver-endpoint",FLOW_ISSUE,FLOW_SUCCESS,"resolver",false};
        xacmeclient Client={0};xacmedns Dns={0};
        strcpy(Client.sPropagateResolvers[0],"127.0.0.1:53junk");
        strcpy(Client.sPropagateResolvers[1],"[::1]:5353");Client.iPropagateResolverCount=2u;
        Active=&Probe;Enabled=true;Inject=false;ResolverQueries=0;strcpy(ResolverTxt,"fixture-TXT");
        ResolverEndpointExpected="::1";ResolverEndpointPort=5353;
        testRequire(xacmeFlowTxtVisible(&Dns,&Client,"_acme-challenge.example.test",ResolverTxt) && ResolverQueries==1u,
            "invalid resolver must be skipped before querying the valid IPv6 endpoint");
        ResolverQueries=0;testRequire(xrtMemDebugFailAfter(0),"resolver parse diagnostic allocation fault arm");
        bool Visible=xacmeFlowTxtVisible(&Dns,&Client,"_acme-challenge.example.test",ResolverTxt);
        testRequire(!Visible && ResolverQueries==0u && xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError())==XERR_MEMORY,
            "resolver parse MEMORY must stop before querying a later endpoint");
        xrtMemDebugFailClear();ResolverEndpointExpected=NULL;Enabled=false;Active=NULL;xrtClearError();
        testMemoryDebugDrain("resolver propagation probes retained allocations");
        puts("[PASS] FLOW resolver propagation endpoint identity and parse OOM terminal policy");
    }
}
static void run_case(const flowcase* Case,bool Oom) {
    Active=Case; Enabled=Case->Op==FLOW_CREATE; Inject=Oom; Armed=FirstReply=Finalized=false;
    Requests=NonceRequests=AuthzRequests=OrderRequests=LookupRequests=KeyChangeRequests=Sleeps=Adds=Removes=0; LastJson=NULL; FirstError=NULL;
    ResolverQueries=0u; ResolverClock=xrtTimer();
    CertificateDecodes=AlternateRequests=0u; AlternateResponse=false;
    xrtClearError(); struct xacmeclient* Client=create_client(Enabled || strcmp(Case->Name,"host-cert-key")==0);
    bool Result=Client!=NULL;
    if(Case->Op!=FLOW_CREATE) { testRequire(Result,"flow baseline client failed"); Enabled=true; Result=operation(Client); }
    bool Triggered=xrtMemDebugFailTriggered(); xerrkind Kind=xrtErrorKind(xrtGetError()); bool Identity=!FirstError || xrtGetError()==FirstError;
    xrtMemDebugFailClear(); Enabled=false;
    printf("[diagnostic] FLOW %s/%s success=%u triggered=%u kind=%u same-cause=%u requests=%u sleeps=%u add=%u remove=%u\n",
        Case->Name,Oom?"oom":"control",(unsigned)Result,(unsigned)Triggered,(unsigned)Kind,(unsigned)Identity,Requests,Sleeps,Adds,Removes);
    bool Advisory=strcmp(Case->Name,"alternate-der")==0 && Oom;
    bool ExpectedSuccess=Advisory || (!Oom && (Case->Kind==FLOW_ALLOC || Case->Kind==FLOW_SUCCESS));
    testRequire(Result==ExpectedSuccess && Triggered==Oom,"flow ignored a fault or accepted invalid data");
    if(Oom && !Advisory) testRequire(Kind==XERR_MEMORY && Identity,"flow replaced the allocation cause");
    else if(Case->Kind==FLOW_HTTP_FAIL) testRequire(Kind==XERR_IO && Identity,"flow replaced the transport cause");
    else if(Case->Kind==FLOW_INVALID) testRequire(Kind==(strcmp(Case->Target?Case->Target:"","domain")==0?XERR_ARGUMENT:XERR_PROTOCOL),"flow invalid data has wrong error kind");
    if(strcmp(Case->Name,"resolver-advisory")==0) testRequire(Sleeps>=1u,"resolver advisory timeout was not exercised");
    else testRequire(Sleeps==(strcmp(Case->Name,"cleanup-cause")==0?2u:0u),"terminal flow failure was polled or retried");
    if(strcmp(Case->Target?Case->Target:"","resolver")==0) {
        printf("[diagnostic] resolver queries=%u\n",ResolverQueries);
        if(strcmp(Case->Name,"resolver-advisory")==0) testRequire(ResolverQueries==3u,"advisory resolver pass incomplete");
        else testRequire(ResolverQueries==(strcmp(Case->Name,"resolver-memory-first")==0?1u:2u),"resolver fallback continued after OOM");
        if(Oom) testRequire(Adds==1u && Removes==1u && AuthzRequests==1u && OrderRequests==0u,
            "resolver OOM triggered challenge or leaked TXT ownership");
    }
    if(strcmp(Case->Name,"badnonce-limit")==0 && !Oom) testRequire(Requests==6 && NonceRequests==1,"badNonce replay exceeded three POST attempts or fetched a different nonce");
    if(strcmp(Case->Name,"account-cert-key")==0) testRequire(Requests==3u && Adds==0u,"account-key CSR was sent to the CA");
    if(Case->Op!=FLOW_CREATE && !Result) {
        xrtClearError(); testRequire(operation(Client),"flow same-client recovery operation failed");
    }
    if(Client) { testRequire(xrtAcmeClientCleanup(Client),"flow cleanup failed"); xrtAcmeClientDestroy(Client); }
    xrtClearError(); xrtErrorFree(FirstError); FirstError=NULL;
    /* A new ordinary client must still complete the same public operation. */
    Client=create_client(false); testRequire(Client!=NULL,"flow recovery create failed");
    if(Case->Op!=FLOW_CREATE) testRequire(operation(Client),"flow public recovery operation failed");
    testRequire(xrtAcmeClientCleanup(Client),"flow recovery cleanup failed"); xrtAcmeClientDestroy(Client);
    xrtClearError(); testRequire(xrtMemDebugReset(),"flow retained allocations");
    printf("[PASS] FLOW %s/%s\n",Case->Name,Oom?"oom":"control");
}
static void alternate_urls(void) {
    static const struct { cstr Ref, Expected; } Cases[] = {
        {"g","http://a/b/c/g"}, {"./g","http://a/b/c/g"}, {"g/","http://a/b/c/g/"},
        {"/g","http://a/g"}, {"//g","http://g"}, {"?y","http://a/b/c/d;p?y"},
        {"g?y","http://a/b/c/g?y"}, {"#s","http://a/b/c/d;p?q"}, {"g#s","http://a/b/c/g"},
        {"g?y#s","http://a/b/c/g?y"}, {";x","http://a/b/c/;x"}, {"g;x","http://a/b/c/g;x"},
        {"g;x?y#s","http://a/b/c/g;x?y"}, {"","http://a/b/c/d;p?q"}, {".","http://a/b/c/"},
        {"./","http://a/b/c/"}, {"..","http://a/b/"}, {"../","http://a/b/"}, {"../g","http://a/b/g"},
        {"../..","http://a/"}, {"../../","http://a/"}, {"../../g","http://a/g"},
        {"../../../g","http://a/g"}, {"../../../../g","http://a/g"}, {"/./g","http://a/g"},
        {"/../g","http://a/g"}, {"g.","http://a/b/c/g."}, {".g","http://a/b/c/.g"},
        {"g..","http://a/b/c/g.."}, {"..g","http://a/b/c/..g"}, {"./../g","http://a/b/g"},
        {"./g/.","http://a/b/c/g/"}, {"g/./h","http://a/b/c/g/h"}, {"g/../h","http://a/b/c/h"},
        {"g;x=1/./y","http://a/b/c/g;x=1/y"}, {"g;x=1/../y","http://a/b/c/y"},
        {"g?y/./x","http://a/b/c/g?y/./x"}, {"g?y/../x","http://a/b/c/g?y/../x"},
        {"g#s/./x","http://a/b/c/g"}, {"g#s/../x","http://a/b/c/g"},
        {"http://b/one/../two?x/../y#z","http://b/two?x/../y"},
        {"%2e%2e/g","http://a/b/c/%2e%2e/g"}, {"g//h","http://a/b/c/g//h"},
        {"?","http://a/b/c/d;p?"}, {"g:h",NULL}, {"http:g",NULL}, {"ftp://b/g",NULL},
        {"//user@b/g",NULL}, {"g%",NULL}, {"g%2x",NULL}, {"g bad",NULL}, {"g\\bad",NULL}
    };
    char Output[512];
    for(size_t i=0;i<sizeof(Cases)/sizeof(Cases[0]);i++) {
        strcpy(Output,"unchanged");
        bool Result=xacmeFlowAlternateUrl("http://a/b/c/d;p?q",Cases[i].Ref,Output,sizeof(Output));
        if(Result!=(Cases[i].Expected!=NULL) || strcmp(Output,Cases[i].Expected?Cases[i].Expected:"unchanged")!=0) {
            fprintf(stderr,"URI vector %zu %s result=%d output=%s\n",i,Cases[i].Ref,(int)Result,Output);
            testRequire(false,"alternate URI resolution differs from RFC 3986");
        }
        xrtClearError();
    }
    testRequire(xacmeFlowAlternateUrl("https://[::1]:444/one/cert","../alt?x",Output,sizeof(Output)) &&
        strcmp(Output,"https://[::1]:444/alt?x")==0,"IPv6 alternate authority changed");
    testRequire(xacmeFlowAlternateUrl("https://a","alt",Output,sizeof(Output)) &&
        strcmp(Output,"https://a/alt")==0,"empty base path merge invalid");
    strcpy(Output,"unchanged");
    testRequire(!xacmeFlowAlternateUrl("https://a/cert","alt",Output,13u) && strcmp(Output,"unchanged")==0,"URI truncated");
    testRequire(xacmeFlowAlternateUrl("https://a/cert","alt",Output,14u) && strcmp(Output,"https://a/alt")==0,"URI exact capacity rejected");
    testRequire(!xacmeFlowAlternateUrl("https://a/cert","alt",NULL,14u),"URI accepted NULL output");
    testRequire(!xacmeFlowAlternateUrl(NULL,"alt",Output,sizeof(Output)),"URI accepted NULL base");
    testRequire(!xacmeFlowAlternateUrl("/cert","alt",Output,sizeof(Output)),"URI accepted relative base");
    xrtClearError(); testRequire(xrtMemDebugReset(),"URI resolver retained allocations");
    printf("[PASS] FLOW alternate URI resolution and output bounds (%zu vectors)\n",sizeof(Cases)/sizeof(Cases[0])+7u);
}
static void link_parser(void) {
    static const struct { cstr Header, Expected; } Cases[] = {
        {"<https://ca.example/dir>; rel=\"index\", <https://ca.example/alt>; rel=\"alternate\"", "https://ca.example/alt"},
        {"<https://ca.example/alt>; rel=alternate", "https://ca.example/alt"},
        {"<https://ca.example/alt>; ReL = \"up ALTERNATE\"", "https://ca.example/alt"},
        {"<https://ca.example/dir>; title=\"rel=\\\"alternate\\\"\"; rel=index, <https://ca.example/alt>; rel=alternate", "https://ca.example/alt"},
        {"<https://ca.example/a,b>; title=\"a,b;<fake>\"; rel=\"alternate up\"", "https://ca.example/a,b"},
        {"<https://ca.example/dir>; rel=index; rel=alternate, <https://ca.example/alt>; rel=alternate", "https://ca.example/alt"},
        {"<https://ca.example/dir>; anchor=\"https://elsewhere.example/\"; rel=alternate, <https://ca.example/alt>; rel=alternate", "https://ca.example/alt"},
        {"<https://ca.example/alt>; rel=alternate; title=\"escaped \\\" comma,\"", "https://ca.example/alt"},
        {"<https://ca.example/dir>; rel=\"notalternate alternate-suffix\"", NULL},
        {"<https://ca.example/dir>; title=\"rel=\\\"alternate\\\"\"", NULL},
        {"<https://ca.example/dir>; rel=\"index\", <https://ca.example/alt>; rel=\"alternate", NULL},
        {"<https://ca.example/alt>; rel=alternate; title=\"unterminated", NULL},
        {"<https://ca.example/alt>; rel=alternate garbage", NULL},
        {"<https://ca.example/alt>; rel=alternate; anchor=\"\"", NULL},
        {"<>; rel=alternate", NULL},
        {"<https://ca.example/dir>; rel=index", NULL},
        {"", NULL},
        {NULL, NULL}
    };
    char Output[128];
    for(size_t i=0;i<sizeof(Cases)/sizeof(Cases[0]);i++) {
        strcpy(Output,"unchanged");
        bool Result=xacmeFlowLinkAlternate(Cases[i].Header,Output,sizeof(Output));
        if(Result!=(Cases[i].Expected!=NULL) || strcmp(Output,Cases[i].Expected?Cases[i].Expected:"unchanged")!=0) {
            fprintf(stderr,"Link parser vector %zu: result=%d output=%s expected=%s\n",i,(int)Result,Output,Cases[i].Expected?Cases[i].Expected:"no link");
            testRequire(false,"Link relation attached to the wrong target or malformed parameters accepted");
        }
    }
    strcpy(Output,"unchanged");
    testRequire(!xacmeFlowLinkAlternate("<abcd>;rel=alternate",Output,4) && strcmp(Output,"unchanged")==0,"Link target truncated");
    testRequire(xacmeFlowLinkAlternate("<abcd>;rel=alternate",Output,5) && strcmp(Output,"abcd")==0,"Link exact capacity rejected");
    testRequire(!xacmeFlowLinkAlternate("<abcd>;rel=alternate",NULL,5),"Link null output accepted");
    testRequire(!xacmeFlowLinkAlternate("<abcd>;rel=alternate",Output,0),"Link zero capacity accepted");
    printf("[PASS] FLOW Link grammar, relation association and output bounds (%zu vectors)\n",sizeof(Cases)/sizeof(Cases[0])+4u);
}
static xerror* rollover_error(xerrkind Kind,bool Uncertain) {
    if(Kind==XERR_MEMORY) {
        testRequire(xrtMemDebugFailAfter(0),"rollover worker allocation arm");
        testRequire(xrtMalloc(1)==NULL && xrtMemDebugFailTriggered(),"rollover worker actual MEMORY cause");
        xrtMemDebugFailClear();
    } else xrtSetErrorInfo(Kind,"test.flow.rollover.worker",88,"rollover producer failed");
    xerror* Root=xrtErrorRef(xrtGetError());
    xerror* Error=Uncertain?xrtErrorWrap(Root,Kind,"xrt.acme.http",XACME_HTTP_ERROR_UNCERTAIN,"rollover result unknown"):
        xrtErrorRef(Root);
    testRequire(Error && xrtErrorKind(Error)==Kind && (!Uncertain || xrtErrorCause(Error)==Root),"rollover immutable cause publication");
    xrtErrorFree(Root); xrtClearError(); return Error;
}
static void rollover_fault_policy(void) {
    static const struct {
        cstr Name; rolloverfaultstage Stage; xerrkind Kind; bool Uncertain; unsigned Failures;
        bool Applied, Success; unsigned Lookups, Writes;
    } Rows[]={
        {"lookup-memory-once",ROLLOVER_LOOKUP,XERR_MEMORY,true,1,true,false,1,0},
        {"lookup-memory-persistent",ROLLOVER_LOOKUP,XERR_MEMORY,true,4,true,false,1,0},
        {"lookup-memory-plain",ROLLOVER_LOOKUP,XERR_MEMORY,false,1,true,false,1,0},
        {"lookup-io-once",ROLLOVER_LOOKUP,XERR_IO,true,1,true,true,2,0},
        {"lookup-io-exhausted",ROLLOVER_LOOKUP,XERR_IO,true,4,true,false,3,0},
        {"lookup-timeout-once",ROLLOVER_LOOKUP,XERR_TIMEOUT,true,1,true,true,2,0},
        {"lookup-timeout-exhausted",ROLLOVER_LOOKUP,XERR_TIMEOUT,true,4,true,false,3,0},
        {"lookup-io-plain",ROLLOVER_LOOKUP,XERR_IO,false,1,true,false,1,0},
        {"write-memory-applied",ROLLOVER_WRITE,XERR_MEMORY,true,1,true,false,1,1},
        {"write-memory-not-applied",ROLLOVER_WRITE,XERR_MEMORY,true,1,false,false,1,1},
        {"write-memory-plain",ROLLOVER_WRITE,XERR_MEMORY,false,1,true,false,1,1},
        {"write-io-applied",ROLLOVER_WRITE,XERR_IO,true,1,true,true,2,1},
        {"write-io-not-applied",ROLLOVER_WRITE,XERR_IO,true,1,false,false,2,1},
        {"write-timeout-applied",ROLLOVER_WRITE,XERR_TIMEOUT,true,1,true,true,2,1}
    };
    xacmees256key NewKey={{0},{0}}; char NewPem[1024], SavedPem[1024]; unsigned Failed=0;
    NewKey.Private[31]=1u; testRequire(xacmeEs256FromPrivate(&NewKey),"rollover fixed new key fixture");
    str Encoded=xacmeKeyPemWrite(&NewKey); testRequire(Encoded && strlen(Encoded)<sizeof(NewPem),"rollover new PEM fixture");
    strcpy(NewPem,Encoded); strcpy(SavedPem,Encoded); xrtSecureZero(Encoded,strlen(Encoded)); xrtFree(Encoded);
    xrtClearError(); testMemoryDebugDrain("rollover fixture allocation");
    for(size_t i=0;i<sizeof(Rows)/sizeof(Rows[0]);i++) {
        Active=&Cases[0]; Enabled=Inject=Armed=false; FirstError=NULL;
        struct xacmeclient* Client=create_client(false); testRequire(Client!=NULL,"rollover policy client fixture");
        xacmees256key Old=Client->AccountKey;
        RolloverError=rollover_error(Rows[i].Kind,Rows[i].Uncertain);
        RolloverFaultStage=Rows[i].Stage; RolloverFailures=Rows[i].Failures; RolloverUncertain=Rows[i].Uncertain;
        RolloverRemoteApplied=Rows[i].Stage==ROLLOVER_LOOKUP; RolloverApplyWrite=Rows[i].Applied;
        RolloverExportOom=false; RolloverStoreFailure=XERR_NONE; RolloverPemPending=false;
        RolloverLookups=RolloverWrites=RolloverExports=RolloverSaves=RolloverDirtyFrees=Requests=NonceRequests=Sleeps=0;
        RolloverProbe=true;
        bool Result=xrtAcmeClientRollover(Client,NewPem,NULL);
        bool Same=xrtGetError()==RolloverError;
        bool Ok=Result==Rows[i].Success && RolloverLookups==Rows[i].Lookups && RolloverWrites==Rows[i].Writes &&
            NonceRequests==Rows[i].Lookups-1u && Sleeps==0u && RolloverExports==0u && RolloverSaves==0u &&
            (Result?xrtGetError()==NULL:Same) && memcmp(&Client->AccountKey,Result?&NewKey:&Old,sizeof(NewKey))==0;
        printf("[rollover-policy] %s success=%d lookups=%u writes=%u nonce=%u same-cause=%d ok=%d\n",
            Rows[i].Name,Result,RolloverLookups,RolloverWrites,NonceRequests,Same,Ok);
        /* A caller can reconcile the same retained new key explicitly after
         * a terminal allocation failure, including a remotely applied write. */
        RolloverFailures=0; RolloverRemoteApplied=true;
        unsigned PriorLookups=RolloverLookups, PriorWrites=RolloverWrites;
        testRequire(xrtAcmeClientRollover(Client,NewPem,NULL) && xrtGetError()==NULL &&
            RolloverLookups==PriorLookups+1u && RolloverWrites==PriorWrites &&
            memcmp(&Client->AccountKey,&NewKey,sizeof(NewKey))==0,"explicit rollover reconciliation failed or repeated write");
        testRequire(strcmp(NewPem,SavedPem)==0,"rollover changed caller's new PEM");
        RolloverProbe=false; xrtAcmeClientDestroy(Client); xrtErrorFree(RolloverError); RolloverError=NULL;
        xrtSecureZero(&Old,sizeof(Old)); xrtClearError(); testMemoryDebugDrain("rollover policy retained allocations");
        if(!Ok) Failed++; else printf("[PASS] FLOW rollover/%s\n",Rows[i].Name);
    }
    static const struct { cstr Name; xerrkind StoreFailure; bool ExportOom; } StoreRows[]={
        {"store-success",XERR_NONE,false}, {"store-io",XERR_IO,false},
        {"store-memory",XERR_MEMORY,false}, {"export-memory",XERR_NONE,true}
    };
    for(size_t i=0;i<sizeof(StoreRows)/sizeof(StoreRows[0]);i++) {
        Active=&Cases[0]; Enabled=Inject=Armed=false; FirstError=NULL;
        struct xacmeclient* Client=create_client(false); testRequire(Client!=NULL,"rollover store client fixture");
        RolloverError=StoreRows[i].StoreFailure==XERR_NONE?NULL:rollover_error(StoreRows[i].StoreFailure,false);
        RolloverFaultStage=ROLLOVER_NO_FAULT; RolloverFailures=0; RolloverRemoteApplied=false; RolloverApplyWrite=true;
        RolloverExportOom=StoreRows[i].ExportOom; RolloverStoreFailure=StoreRows[i].StoreFailure;
        RolloverPemPending=false; RolloverExpectedPem=NewPem;
        RolloverLookups=RolloverWrites=RolloverExports=RolloverSaves=RolloverDirtyFrees=0;
        RolloverProbe=true; xrtClearError();
        bool Result=xrtAcmeClientRollover(Client,NewPem,"fixture-store");
        bool Expected=StoreRows[i].StoreFailure==XERR_NONE && !StoreRows[i].ExportOom;
        bool Ok=Result==Expected && (Result?xrtGetError()==NULL:xrtGetError()==RolloverError) &&
            memcmp(&Client->AccountKey,&NewKey,sizeof(NewKey))==0 && RolloverLookups==1u && RolloverWrites==1u &&
            RolloverExports==1u && RolloverSaves==(StoreRows[i].ExportOom?0u:1u) && !RolloverPemPending && RolloverDirtyFrees==0u;
        printf("[rollover-retire] %s success=%d exports=%u saves=%u uncleared=%u same-cause=%d ok=%d\n",
            StoreRows[i].Name,Result,RolloverExports,RolloverSaves,RolloverDirtyFrees,xrtGetError()==RolloverError,Ok);
        RolloverRemoteApplied=true; RolloverExportOom=false; RolloverStoreFailure=XERR_NONE;
        unsigned PriorSaves=RolloverSaves;
        bool Recovered=xrtAcmeClientRollover(Client,NewPem,"fixture-store");
        testRequire(Recovered && RolloverLookups==2u && RolloverWrites==1u && RolloverExports==2u &&
            RolloverSaves==PriorSaves+1u && !RolloverPemPending && memcmp(&Client->AccountKey,&NewKey,sizeof(NewKey))==0 &&
            strcmp(NewPem,SavedPem)==0 && xrtGetError()==NULL,"rollover persistence recovery changed key or repeated remote write");
        Ok=Ok && RolloverDirtyFrees==0u;
        RolloverProbe=false; xrtAcmeClientDestroy(Client); xrtErrorFree(RolloverError); RolloverError=NULL;
        xrtClearError(); testMemoryDebugDrain("rollover persistence retained allocations");
        if(!Ok) Failed++; else printf("[PASS] FLOW rollover/%s\n",StoreRows[i].Name);
    }
    xrtSecureZero(NewPem,sizeof(NewPem)); xrtSecureZero(SavedPem,sizeof(SavedPem)); xrtSecureZero(&NewKey,sizeof(NewKey));
    printf("[rollover-summary] rows=%zu failed=%u\n",sizeof(Rows)/sizeof(Rows[0])+sizeof(StoreRows)/sizeof(StoreRows[0]),Failed);
    testRequire(Failed==0u,"rollover replayed MEMORY, lost key state/cause or retired private text uncleared");
}
static bool eab_run(cstr Label,cstr Hmac,cstr Kid,int64 FailAfter,uint64* Allocations,bool Wire) {
    xacmeaccountconfig Account; xacmeclientconfig Config;
    char SavedText[5500]; testRequire(!Hmac || strlen(Hmac)<sizeof(SavedText),"EAB snapshot capacity"); strcpy(SavedText,Hmac?Hmac:"");
    Active=&Cases[0]; Enabled=Inject=Armed=false; FirstError=NULL; xrtClearError();
    xrtAcmeAccountConfigInit(&Account); xrtAcmeClientConfigInit(&Config);
    Account.sDirectoryUrl="http://ca.example/dir"; Account.sAccountKeyPem=KeyPem;
    Account.Eab.sKid=Kid; Account.Eab.sHmac=Hmac; Config.pAccount=&Account;
    xacmeaccountconfig Saved=Account;
    EabArmed=EabEnded=EabPending=false; EabText=Hmac; EabDecodedAddress=0; EabCapacity=EabDecodedSize=0;
    EabWipes=EabLocalWipes=EabDirtyFrees=EabPosts=0; EabAllocations=0; EabFailAfter=FailAfter; EabWire=Wire; EabProbe=true;
    struct xacmeclient* Client=xrtAcmeClientCreate(&Config);
    bool Triggered=xrtMemDebugFailTriggered(); xerrkind Kind=xrtErrorKind(xrtGetError());
    eab_end_scan();
    bool Wiped=!EabPending && EabDirtyFrees==0u;
    bool Ok=FailAfter==-4 ? Client==NULL && Kind==XERR_MEMORY && EabPosts==0u && EabDecodedSize==0u && EabLocalWipes==1u && Wiped :
        FailAfter==-3 ? Client!=NULL && EabPosts==1u && EabDecodedSize==0u && EabLocalWipes==0u && Wiped :
        FailAfter==-2 ? Client==NULL && Kind==XERR_ARGUMENT && EabPosts==0u && EabDecodedSize==0u && EabLocalWipes==1u && Wiped :
        FailAfter<0 ? Client!=NULL && EabPosts==1u && EabDecodedSize!=0u && EabWipes==1u && EabLocalWipes==1u && Wiped :
        Client==NULL && Triggered && Kind==XERR_MEMORY && EabPosts==0u && EabLocalWipes==1u && Wiped;
    if(FirstError) testRequire(xrtGetError()==FirstError,"EAB lost original allocation error");
    testRequire(Account.sDirectoryUrl==Saved.sDirectoryUrl && Account.sAccountKeyPem==Saved.sAccountKeyPem &&
        Account.Eab.sKid==Saved.Eab.sKid && Account.Eab.sHmac==Saved.Eab.sHmac &&
        Account.sContactEmail==Saved.sContactEmail && (!Hmac || strcmp(Hmac,SavedText)==0),"EAB modified borrowed account configuration");
    if(Allocations) *Allocations=EabAllocations;
    printf("[eab-key] %s fault=%lld success=%d decoded=%zu wipes=%u local-wipes=%u dirty-free=%u posts=%u allocations=%llu ok=%d\n",
        Label,(long long)FailAfter,Client!=NULL,EabDecodedSize,EabWipes,EabLocalWipes,EabDirtyFrees,EabPosts,
        (unsigned long long)EabAllocations,Ok);
    EabProbe=false; xrtMemDebugFailClear(); xrtAcmeClientDestroy(Client);
    xrtErrorFree(FirstError); FirstError=NULL; xrtClearError(); testMemoryDebugDrain("EAB retained logical allocations");
    return Ok;
}
static void eab_keys(bool Wire) {
    static const size_t Sizes[]={1u,11u,32u,63u,64u,65u,80u,128u,129u,512u,513u,4096u};
    static const xbase64config Padded={NULL,XBASE64_URL}, Raw={NULL,XBASE64_URL|XBASE64_NO_PADDING};
    uint8 Key[4096]; unsigned Failed=0, Runs=0; uint64 Faults=0;
    for(size_t i=0;i<sizeof(Key);i++) Key[i]=(uint8)(17u+131u*i);
    for(size_t i=0;i<sizeof(Sizes)/sizeof(Sizes[0]);i++) for(unsigned Padding=0;Padding<2u;Padding++) {
        char Label[64]; snprintf(Label,sizeof(Label),"%zu-%s",Sizes[i],Padding?"padded":"raw");
        str Encoded=xrtBase64EncodeNew((cstr)Key,Sizes[i],Padding?&Padded:&Raw);
        testRequire(Encoded!=NULL,"EAB key fixture encoding");
        /* Keep the fixture in local storage so per-run Core balance is exact. */
        char Text[5500]; testRequire(strlen(Encoded)<sizeof(Text),"EAB fixture capacity");
        strcpy(Text,Encoded); xrtFree(Encoded); xrtClearError(); testMemoryDebugDrain("EAB fixture retained allocation");
        uint64 Allocations=0; bool Ok=eab_run(Label,Text,"fixture-\"kid\\",-1,&Allocations,Wire); Runs++;
        if(!Ok) Failed++;
        testRequire(strlen(Text)==((Sizes[i]+2u)/3u)*4u-(Padding?0u:((3u-Sizes[i]%3u)%3u)),"EAB modified borrowed HMAC text");
        if(Ok && Padding==0u && (Sizes[i]==32u || Sizes[i]==129u)) {
            testRequire(Allocations>0u && Allocations<128u,"EAB allocation scan bound");
            for(uint64 N=0;N<Allocations;N++) {
                if(!eab_run(Label,Text,"fixture-\"kid\\",(int64)N,NULL,false)) Failed++;
                Runs++; Faults++;
            }
            if(!eab_run(Label,Text,"fixture-\"kid\\",-1,NULL,false)) Failed++;
            Runs++;
        }
    }
    static const cstr Invalid[]={NULL,"","!","A","AB","AA===","AA==suffix","AA A","+w","/w","AA\n"};
    for(size_t i=0;i<sizeof(Invalid)/sizeof(Invalid[0]);i++) {
        char Label[40]; snprintf(Label,sizeof(Label),"invalid-base64url-%zu",i);
        if(!eab_run(Label,Invalid[i],"fixture-kid",-2,NULL,false)) Failed++;
        Runs++;
    }
    if(!eab_run("zero-byte-raw","AA","fixture-kid",-1,NULL,false)) Failed++;
    if(!eab_run("zero-byte-padded","AA==","fixture-kid",-1,NULL,false)) Failed++;
    /* Core optional-padding mode also accepts this existing partial form. */
    if(!eab_run("legacy-partial-padding","AA=","fixture-kid",-1,NULL,false)) Failed++;
    if(!eab_run("no-kid","invalid ignored",NULL,-3,NULL,false)) Failed++;
    if(!eab_run("empty-kid","invalid ignored","",-3,NULL,false)) Failed++;
    if(!eab_run("decoder-diagnostic-memory","!","fixture-kid",-4,NULL,false)) Failed++;
    Runs+=6u;
    xrtSecureZero(Key,sizeof(Key));
    printf("[eab-summary] runs=%u allocation-faults=%llu failed=%u\n",Runs,(unsigned long long)Faults,Failed);
    testRequire(Failed==0u,"EAB key length, secure retirement or allocation failure regression");
    printf("[PASS] FLOW EAB key lengths, padding, secure retirement and allocation recovery\n");
}
int main(int argc,char** argv) {
    if(argc==2 && strcmp(argv[1],"rollover-faults")==0) { rollover_fault_policy(); return 0; }
    if(argc==2 && strcmp(argv[1],"eab-keys")==0) { eab_keys(true); return 0; }
    if(argc==2 && strcmp(argv[1],"link-parser")==0) { link_parser(); return 0; }
    if((argc==2 || argc==3) && strcmp(argv[1],"read-retry")==0) { read_retry_policy(argc==3?argv[2]:NULL); return 0; }
    if(argc==2 && strcmp(argv[1],"resolver-endpoints")==0) { resolver_endpoints(); return 0; }
    testRequire(argc==1 || argc==3,"flow fixture usage: [name control|oom]");
    if(argc==2 && strcmp(argv[1],"alternate-urls")==0) { alternate_urls(); return 0; }
    if(argc==1) { link_parser(); alternate_urls(); read_retry_policy(NULL); resolver_endpoints(); eab_keys(false); rollover_fault_policy(); }
    for(size_t i=0;i<sizeof(Cases)/sizeof(Cases[0]);i++) {
        if(argc==3 && strcmp(argv[1],Cases[i].Name)!=0) continue;
        if(argc==3) { bool Oom=strcmp(argv[2],"oom")==0; testRequire(Oom || strcmp(argv[2],"control")==0,"invalid flow mode"); run_case(&Cases[i],Oom); return 0; }
        run_case(&Cases[i],false); if(Cases[i].Oom) run_case(&Cases[i],true);
    }
    testRequire(argc==1,"unknown flow case"); return 0;
}
