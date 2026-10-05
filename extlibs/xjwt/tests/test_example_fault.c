/* Exercise the actual offline middleware, including its application ownership. */
#define XRT_MODULE_MEMORY_DEBUG
#define main jwtExampleMain
#include "../examples/auth_middleware.c"
#undef main
static void require(bool condition, const char* label)
{
    if(!condition) { fprintf(stderr,"[FAIL] JWT example: %s\n",label); exit(1); }
}
static void clean(void)
{
    xrtMemDebugFailClear(); xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    require(memory.LiveCount==0 && memory.LiveBytes==0 &&
        memory.InvalidFreeCount==0 && memory.DoubleFreeCount==0,"all owned memory released once");
    require(xrtMemDebugReset(),"reset after cleanup");
}
static void claims_case(unsigned variant)
{
    xvalue* claims=xrtValueObject(); require(claims!=NULL,"claims allocated");
    require(example_set_string(claims,"sub","alice") && example_set_string(claims,"role","admin"),"default identity");
    const char* key=variant<5?"role":"sub";
    unsigned form=variant<5?variant:variant-5;
    if(variant<10) {
        if(form==0) require(xrtValueObjectRemove(claims,xrtStrView(key)),"remove required claim");
        else {
            xvalue* value=form==1?xrtValueInt(42):
                xrtValueString(form==2?xrtStrView(""):(form==3?xrtStrViewN("a\0b",3):xrtStrView("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnop")));
            require(xrtValueObjectSetNew(claims,xrtStrView(key),value),"malformed claim fixture");
        }
    }
    xjwtconfig config; xjwtConfigInit(&config);
    config.Alg=XJWT_ALG_HS256; config.KeyPem=G_HS_SECRET;
    config.Issuer=G_ISSUER; config.Audience=G_AUDIENCE;
    char* token=xjwtSign(&config,claims); xrtValueRelease(claims);
    require(token!=NULL,"independently signed claim fixture");
    char header[2048],user[64]="previous-user",role[16]="admin";
    snprintf(header,sizeof(header),"Bearer %s",token); xrtFree(token);
    authresult result=auth_middleware(header,user,sizeof(user),role,sizeof(role));
    require(variant==10?result==AUTH_OK:result==AUTH_BAD_TOKEN,"mandatory identity and privilege claims");
    require(variant==10?(strcmp(user,"alice")==0 && strcmp(role,"admin")==0):(user[0]==0 && role[0]==0),"no stale or partial authorization output");
    clean();
}
int main(void)
{
    require(xrtMemDebugEnable(true),"memory diagnostics enabled");
    for(unsigned i=0;i<=10;i++) claims_case(i);
    unsigned failures=0,fallbacks=0;
    for(size_t limit=0;limit<1024;limit++) {
        require(xrtMemDebugFailAfter(limit),"allocation fault enabled");
        char* token=handle_login("alice","admin");
        bool triggered=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(token==NULL) { require(triggered,"unexpected login failure"); failures++; }
        else {
            xrtClearError();
            char header[2048],user[64],role[16]; snprintf(header,sizeof(header),"Bearer %s",token);
            require(auth_middleware(header,user,sizeof(user),role,sizeof(role))==AUTH_OK &&
                strcmp(user,"alice")==0 && strcmp(role,"admin")==0,"successful allocation fallback is a valid complete token");
            if(triggered) fallbacks++;
        }
        xrtFree(token); clean();
        if(!triggered) { require(failures>=3,"claims construction faults covered"); break; }
        require(limit+1<1024,"allocation sweep completed");
    }
    require(refresh_idp_jwks(),"initial cache");
    xjwtjwks* previous=G_IDP_JWKS;
    require(xrtMemDebugFailAfter(0),"JWKS refresh allocation fault");
    require(!refresh_idp_jwks() && G_IDP_JWKS==previous,"failed refresh retains previous valid keys");
    xrtMemDebugFailClear(); xrtClearError();
    require(verify_idp_token(FIX_IDP_TOKEN),"retained cache remains usable");
    xjwtconfig foreign; xjwtConfigInit(&foreign);
    foreign.Alg=XJWT_ALG_RS256; foreign.KeyPem=FIX_RSA_PRIVATE;
    foreign.Issuer="https://idp.example.com"; foreign.Audience="different-service";
    xvalue* foreignClaims=xrtValueObject();
    char* foreignToken=xjwtSign(&foreign,foreignClaims); xrtValueRelease(foreignClaims);
    require(foreignToken!=NULL,"foreign audience token signed with trusted IdP key");
    xjwtcheck issuerOnly; xjwtCheckInit(&issuerOnly); issuerOnly.Issuer=foreign.Issuer;
    foreignClaims=xjwtVerifyJwks(foreignToken,G_IDP_JWKS,&issuerOnly);
    require(foreignClaims!=NULL,"issuer-only control proves valid signature");
    xrtValueRelease(foreignClaims);
    require(!verify_idp_token(foreignToken),"trusted IdP token for another audience denied");
    xrtFree(foreignToken);
    xjwtJwksFree(G_IDP_JWKS); G_IDP_JWKS=NULL; clean();
    require(xrtMemDebugFailAfter(0),"main startup allocation fault");
    require(jwtExampleMain()==1,"main failure exit status"); clean();
    require(jwtExampleMain()==0,"main successful control"); clean();
    char user[16]="previous",role[16]="admin";
    require(auth_middleware(NULL,user,sizeof(user),role,sizeof(role))==AUTH_NO_TOKEN && user[0]==0 && role[0]==0,"missing header clears outputs");
    require(auth_middleware("Bearer bad",NULL,0,role,sizeof(role))==AUTH_BAD_TOKEN && role[0]==0,"invalid output rejected"); clean();
    printf("[PASS] JWT example: 11 identity cases, login allocation failures=%u valid fallbacks=%u; cache retention, foreign audience denial and main status\n",failures,fallbacks);
    return 0;
}
