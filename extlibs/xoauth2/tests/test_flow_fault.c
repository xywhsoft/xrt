/* Actual form, Basic, JSON, token and allocator code; HTTP is controlled here.
 * The independent TLS probe covers transmission and asynchronous cleanup. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "../xoauth2-xrt.h"

static bool Faulting;
static xerror* FirstError;
static void remember(void)
{
    if(Faulting && FirstError == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY)
        FirstError = xrtErrorRef(xrtGetError());
}
static ptr flow_alloc(size_t Size)
{
    ptr Result = xrtMalloc(Size);
    if(Result == NULL) remember();
    return Result;
}
static xvalue* flow_parse(xstrview Text)
{
    xvalue* Result = xrtJsonParse(Text);
    if(Result == NULL) remember();
    return Result;
}
static str flow_base64(const void* Data,size_t Size,const xbase64config* Config)
{
    str Result=xrtBase64EncodeNew(Data,Size,Config);
    if(Result==NULL) remember();
    return Result;
}
static void flow_error_info(xerrkind Kind,cstr Domain,int32 Code,cstr Message)
{
    xrtSetErrorInfo(Kind,Domain,Code,Message);
    remember();
}
#undef xrtMalloc
#define xrtMalloc flow_alloc
#define xrtJsonParse flow_parse
#define xrtBase64EncodeNew flow_base64
#define xrtSetErrorInfo flow_error_info
#include "../xoauth2.c"
#undef xrtMalloc
#undef xrtJsonParse
#undef xrtBase64EncodeNew
#undef xrtSetErrorInfo

static const char* Mode;
static unsigned Calls;
static const char TokenJson[] = "{\"access_token\":\"access\",\"refresh_token\":\"refresh\","
    "\"id_token\":\"id\",\"openid\":\"open\",\"token_type\":\"Bearer\",\"scope\":\"read write\",\"expires_in\":3600}";
static void require(bool Result, const char* Message)
{
    if(!Result) { fprintf(stderr,"[FAIL] OAUTH_FLOW %s: %s\n",Mode,Message); exit(1); }
}
static bool named(const char* Name) { return strcmp(Mode,Name)==0; }
static bool login(void) { return strncmp(Mode,"login-",6)==0 || named("wechat-login") || named("callback-cause") || named("callback-empty"); }
static bool http(const char* Method, const char* Url, const char* Body, const char* Auth,
    char** Response, int* Status, void* Context)
{
    (void)Context; Calls++;
    require(Calls==1,"one transport attempt per operation");
    if(named("callback-cause")) {
        xrtSetErrorInfo(XERR_IO,"test.oauth.callback",2701,"unknown token outcome");
        FirstError=xrtErrorRef(xrtGetError());
        return false;
    }
    if(named("callback-empty")) return false;
    if(named("userinfo")) {
        require(strcmp(Method,"GET")==0 && Body==NULL && Auth!=NULL && strcmp(Auth,"Bearer access")==0,"userinfo request");
    } else if(named("wechat-userinfo")) {
        require(strcmp(Method,"GET")==0 && Body==NULL && Auth==NULL && strstr(Url,"access_token=access&openid=open")!=NULL,"WeChat userinfo request");
    } else if(named("wechat-login")) {
        require(strcmp(Method,"GET")==0 && Body==NULL && Auth==NULL && strstr(Url,"appid=client%26one&secret=sec%2B%3A%20")!=NULL,"WeChat request");
    } else {
        require(strcmp(Method,"POST")==0 && Body!=NULL,"token request method/body");
        if(strstr(Mode,"basic")!=NULL) {
            require(Auth!=NULL && strcmp(Auth,"Basic Y2xpZW50JTI2b25lOnNlYyUyQiUzQSUyMA==")==0,"independently encoded Basic credentials");
            require(strstr(Body,"client_id")==NULL && strstr(Body,"client_secret")==NULL,"credentials occur only in Basic");
        } else require(Auth==NULL && strstr(Body,"client_id=client%26one")!=NULL && strstr(Body,"client_secret=sec%2B%3A%20")!=NULL,"encoded body credentials");
        if(login()) require(strstr(Body,"code=code%2F%3D%26")!=NULL && strstr(Body,"code_verifier=")!=NULL,"encoded code and PKCE verifier");
        else require(strstr(Body,"refresh_token=refresh%2F%3D%26")!=NULL && strstr(Body,"code_verifier=")==NULL,"encoded refresh token");
    }
    const char* Text = named("token-denied")?"{\"error\":\"invalid_grant\"}":
        ((named("userinfo") || named("wechat-userinfo"))?"{\"sub\":\"user\"}":TokenJson);
    *Response=(char*)flow_alloc(strlen(Text)+1u);
    if(*Response==NULL) return false;
    strcpy(*Response,Text);
    *Status=named("token-denied")?400:(named("endpoint")?502:200);
    return true;
}
static void configure(xoauth2client* Client)
{
    memset(Client,0,sizeof(*Client));
    xoauth2config Config; xoauth2ConfigInit(&Config);
    Config.AuthorizeUrl="https://idp.example/auth"; Config.TokenUrl="https://idp.example/token";
    Config.UserInfoUrl="https://idp.example/user"; Config.ClientId="client&one";
    Config.ClientSecret="sec+: "; Config.RedirectUri="https://app.example/cb?x=1";
    Config.UsePkce=true; Config.AuthStyle=strstr(Mode,"basic")?XOAUTH2_AUTH_BASIC:XOAUTH2_AUTH_BODY;
    Config.Http=http; xoauth2UseCustom(Client,&Config);
    Client->Wechat=strncmp(Mode,"wechat-",7)==0;
    strcpy(Client->sState,"state"); memset(Client->sVerifier,'x',43u); Client->sVerifier[43]=0;
    strcpy(Client->sChallenge,"challenge");
}
static bool operation(xoauth2client* Client)
{
    if(named("userinfo") || named("wechat-userinfo")) {
        xoauth2token Token={0}; Token.AccessToken="access"; Token.OpenId="open";
        xvalue* Claims=named("userinfo")?xoauth2GetUserInfo(Client,"access"):xoauth2GetWechatUserInfo(Client,&Token);
        bool Result=Claims!=NULL; xrtValueRelease(Claims); return Result;
    }
    xoauth2token* Token=named("parse")?xoauth2__parse_token_response(TokenJson,strlen(TokenJson)):
        (login()?xoauth2CompleteLogin(Client,"code/=&","state"):xoauth2Refresh(Client,"refresh/=&"));
    bool Result=Token!=NULL; xoauth2TokenFree(Token); return Result;
}
static void clean(xoauth2client* Client)
{
    xoauth2ClientUnit(Client); xrtErrorFree(FirstError); FirstError=NULL; xrtClearError();
    xmemdebugsnapshot Memory; xrtMemDebugSnapshot(&Memory);
    require(Memory.LiveCount==0u && Memory.LiveBytes==0u && Memory.InvalidFreeCount==0u && Memory.DoubleFreeCount==0u,"no retained allocation or invalid free");
}
static void run(const char* Name)
{
    Mode=Name;
    if(named("wrong-state")) {
        xoauth2client Client; configure(&Client); Calls=0;
        require(xoauth2CompleteLogin(&Client,"code","wrong")==NULL && xoauth2LastError()==XOAUTH2_ERROR_STATE_MISMATCH,"wrong state rejected");
        require(Calls==0 && Client.sState[0]!=0 && Client.sVerifier[0]!=0,"wrong state leaves valid session intact");
        clean(&Client); puts("[PASS] OAUTH_FLOW wrong-state"); return;
    }
    if(named("callback-cause") || named("callback-empty")) {
        xoauth2client Client; configure(&Client); Calls=0;
        if(named("callback-empty")) xrtSetErrorInfo(XERR_MEMORY,"test.previous",2702,"stale caller error");
        require(!operation(&Client),"callback failure returns no token");
        require(Calls==1 && Client.sState[0]==0 && Client.sVerifier[0]==0,"callback failure consumed session without replay");
        require(named("callback-cause")?xrtGetError()==FirstError:xoauth2LastError()==XOAUTH2_ERROR_NETWORK,"callback error retained or fresh NETWORK fallback");
        clean(&Client); printf("[PASS] OAUTH_FLOW %s\n",Name); return;
    }
    unsigned Faults=0;
    for(size_t Limit=0;Limit<256u;Limit++) {
        xoauth2client Client; configure(&Client); Calls=0; FirstError=NULL; xrtClearError();
        require(xrtMemDebugFailAfter(Limit),"fault injection available"); Faulting=true;
        bool Result=operation(&Client); bool Triggered=xrtMemDebugFailTriggered();
        Faulting=false; xrtMemDebugFailClear();
        printf("[diagnostic] OAUTH_FLOW %s limit=%zu triggered=%u success=%u kind=%u same-cause=%u calls=%u verifier=%u\n",
            Name,Limit,Triggered,Result,(unsigned)xrtErrorKind(xrtGetError()),FirstError!=NULL && xrtGetError()==FirstError,Calls,Client.sVerifier[0]!=0);
        if(Triggered) {
            require(!Result,"allocation failure returns no partial result");
            require(xrtErrorKind(xrtGetError())==XERR_MEMORY && FirstError!=NULL && xrtGetError()==FirstError,"exact allocation cause retained");
            if(login()) require(Client.sState[0]==0 && Client.sVerifier[0]==0 && Client.sChallenge[0]==0,"all consumed PKCE material burned on request failure");
            Faults++;
            xrtErrorFree(FirstError); FirstError=NULL; xrtClearError(); Calls=0;
            if(login()) { char* Url=xoauth2BeginLogin(&Client); require(Url!=NULL,"same client login recovery"); xrtFree(Url); strcpy(Client.sState,"state"); }
            bool Recovered=operation(&Client);
            require((named("token-denied") || named("endpoint"))?!Recovered:Recovered,"same client recovery");
        } else {
            require((named("token-denied") || named("endpoint"))?!Result:Result,"normal control result");
            if(named("token-denied")) require(xoauth2LastError()==XOAUTH2_ERROR_TOKEN_DENIED,"provider denial control");
            if(named("endpoint")) require(xoauth2LastError()==XOAUTH2_ERROR_TOKEN_ENDPOINT,"HTTP endpoint error control");
        }
        clean(&Client);
        if(!Triggered) { require(Faults!=0u,"actual faults executed"); printf("[PASS] OAUTH_FLOW %s faults=%u controls=1\n",Name,Faults); return; }
    }
    require(false,"allocation sweep reached successful control");
}
int main(int Argc,char** Argv)
{
    static const char* Cases[]={"login-body","login-basic","refresh-body","refresh-basic","parse","token-denied","endpoint","userinfo","wechat-userinfo","wechat-login","wrong-state","callback-cause","callback-empty"};
    if(Argc==2) { run(Argv[1]); return 0; }
    require(Argc==1,"usage: test_flow_fault [case]");
    for(size_t I=0;I<sizeof(Cases)/sizeof(Cases[0]);I++) run(Cases[I]);
    return 0;
}
