#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "support/runtime.h"
static const char* failResponse;
static bool triggered;
static str exampleDup(cstr text)
{
    if(failResponse!=NULL && strstr(text,failResponse)!=NULL) {
        xrtMemDebugFailAfter(0); str result=xrtStrDup(text);
        triggered=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); return result;
    }
    return xrtStrDup(text);
}
#define xrtStrDup exampleDup
#define main oidcExampleMain
#include "../examples/oidc_login.c"
#undef main
#undef xrtStrDup
static void require(bool condition,const char* label)
{
    if(!condition) { fprintf(stderr,"[FAIL] OIDC example: %s\n",label); exit(1); }
}
static void clean(void)
{
    failResponse=NULL; triggered=false; xrtMemDebugFailClear(); xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    require(memory.LiveCount==0 && memory.LiveBytes==0 && memory.InvalidFreeCount==0 && memory.DoubleFreeCount==0,"all owners released");
    require(g_Nonce[0]==0,"mock nonce cleared");
    require(xrtMemDebugReset(),"memory diagnostics reset");
}
static void policy_allocation_faults(void)
{
    xvalue* issued=xrtValueObject();
    require(issued!=NULL && oauthExampleSetString(issued,"iss","https://idp.example") &&
        oauthExampleSetString(issued,"aud","demo-client-id") &&
        oauthExampleSetString(issued,"sub","user-42") &&
        oauthExampleSetString(issued,"nonce","controlled-nonce"),"policy fixture claims");
    xjwtconfig signing; xjwtConfigInit(&signing);
    signing.Alg=XJWT_ALG_ES256; signing.KeyPem=OIDC_EC_PRIV;
    signing.KeyId="oidc-ec-1"; signing.ExpireSeconds=600;
    char* id=xjwtSign(&signing,issued);
    xjwtjwks* keys=xjwtJwksParse(OIDC_JWKS);
    require(id!=NULL && keys!=NULL,"policy signing/JWKS control");
    xoauth2token token={0}; token.IdToken=id; token.AccessToken="acc-demo"; token.TokenType="bearer";
    oidcExamplePolicy policy; oidcExamplePolicyInit(&policy); policy.SigningAlg=XJWT_ALG_ES256;
    xmemdebugsnapshot baseline; xrtClearError(); xrtMemDebugSnapshot(&baseline);
    unsigned failures=0;
    for(size_t limit=0;limit<512;limit++) {
        xoauth2client oauth={0};
        oauth.Config.Issuer="https://idp.example"; oauth.Config.ClientId="demo-client-id";
        strcpy(oauth.sNonce,"controlled-nonce");
        require(xrtMemDebugFailAfter(limit),"policy allocation fault enabled");
        xvalue* identity=oidcExampleVerifyIdToken(&oauth,&token,keys,&policy,NULL);
        bool fault=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(fault) {
            require(identity==NULL && xrtErrorKind(xrtGetError())==XERR_MEMORY,"policy OOM remains a failed authentication with memory cause");
            require(oauth.sNonce[0]!=0,"failed validation does not consume nonce"); failures++;
        } else require(identity!=NULL && oauth.sNonce[0]==0,"validated identity consumes nonce");
        xrtValueRelease(identity); xoauth2ClientUnit(&oauth); xrtClearError();
        xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
        require(after.LiveCount==baseline.LiveCount && after.LiveBytes==baseline.LiveBytes &&
            after.InvalidFreeCount==baseline.InvalidFreeCount && after.DoubleFreeCount==baseline.DoubleFreeCount,"policy allocation owners released");
        if(!fault) { require(failures>=5,"header/signature/claims allocation stages reached"); break; }
        require(limit+1<512,"policy allocation sweep completed");
    }
    xjwtJwksFree(keys); xrtFree(id); xrtValueRelease(issued); clean();
    printf("[PASS] OIDC application policy allocation failures=%u, original memory causes and nonce ownership verified\n",failures);
}
int main(void)
{
    require(xrtMemDebugEnable(true),"enable allocation diagnostics");
    require(xrtMemDebugFailAfter(0),"duplication allocation fault");
    require(dupstr("controlled")==NULL && xrtMemDebugFailTriggered(),"dupstr handles actual null allocation"); clean();
    const char* faults[]={"acc-demo","\"keys\"","\"sub\":\"user-42\""};
    for(size_t i=0;i<3;i++) {
        failResponse=faults[i];
        require(oidcExampleMain()==1 && triggered,"failed token/JWKS/userinfo exits with failure");
        require(xrtErrorKind(xrtGetError())==XERR_MEMORY,"callback memory cause retained"); clean();
    }
    require(xrtMemDebugFailAfter(0),"authorization URL failure");
    require(oidcExampleMain()==1 && xrtMemDebugFailTriggered(),"startup failure exit"); clean();
    require(oidcExampleMain()==0,"successful recovery control"); clean();
    policy_allocation_faults();
    unsigned signingFailures=0;
    for(size_t limit=0;limit<1024;limit++) {
        strcpy(g_Nonce,"controlled-nonce");
        char* response=NULL; int status=0;
        require(xrtMemDebugFailAfter(limit),"mock signing allocation fault");
        bool ok=mock_http("POST","https://idp.example/token",NULL,NULL,&response,&status,NULL);
        bool fault=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        require(ok==(response!=NULL),"callback never reports a missing response as success");
        if(ok) {
            xrtClearError(); xvalue* body=xrtJsonParse(xrtStrView(response)); char idToken[2048];
            require(body!=NULL && oauthExampleClaimString(body,"id_token",idToken,sizeof(idToken)),"successful callback contains a complete id_token");
            xrtValueRelease(body);
        } else { require(fault,"unexpected mock signing failure"); signingFailures++; }
        xrtFree(response); memset(g_Nonce,0,sizeof(g_Nonce)); clean();
        if(!fault) { require(signingFailures>=5,"claims, signing and response faults covered"); break; }
        require(limit+1<1024,"mock signing sweep completed");
    }
    for(unsigned form=0;form<5;form++) {
        xvalue* claims=xrtValueObject(); char nonce[8]="stale";
        if(form!=0) {
            xvalue* value=form==1?xrtValueInt(42):xrtValueString(form==2?xrtStrView(""):
                (form==3?xrtStrViewN("a\0b",3):xrtStrView("oversized")));
            require(xrtValueObjectSetNew(claims,xrtStrView("nonce"),value),"nonce fixture");
        }
        require(!oauthExampleClaimString(claims,"nonce",nonce,sizeof(nonce)) && nonce[0]==0,"invalid required nonce not truncated or reused");
        xrtValueRelease(claims); clean();
    }
    printf("[PASS] OIDC example: null duplication, 3 response faults, startup failure, recovery, signing failures=%u and 5 invalid nonce cases\n",signingFailures);
    return 0;
}
