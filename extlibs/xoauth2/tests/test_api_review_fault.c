/* The actual example callback fails its real allocator at selected responses. */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include <xrt.h>
static const char* failResponse;
static bool triggered;
static str exampleDup(cstr text)
{
    if(failResponse!=NULL && strstr(text,failResponse)!=NULL) {
        xrtMemDebugFailAfter(0);
        str result=xrtStrDup(text);
        triggered=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        return result;
    }
    return xrtStrDup(text);
}
#define xrtStrDup exampleDup
#define main oauthExampleMain
#include "../examples/api_review.c"
#undef main
#undef xrtStrDup
static void require(bool condition,const char* label)
{
    if(!condition) { fprintf(stderr,"[FAIL] OAuth example: %s\n",label); exit(1); }
}
static void clean(void)
{
    failResponse=NULL; triggered=false; xrtMemDebugFailClear(); xrtClearError();
    xmemdebugsnapshot memory; xrtMemDebugSnapshot(&memory);
    require(memory.LiveCount==0 && memory.LiveBytes==0 && memory.InvalidFreeCount==0 && memory.DoubleFreeCount==0,"ownership clean");
    require(g_RefreshBody==NULL,"mock refresh state cleared");
    require(xrtMemDebugReset(),"memory diagnostics reset");
}
int main(void)
{
    require(xrtMemDebugEnable(true),"enable allocation diagnostics");
    bool (*scenarios[])(void)={scenario_github,scenario_oidc,scenario_lifecycle,scenario_errors,scenario_reset};
    for(size_t i=0;i<5;i++) { require(scenarios[i](),"successful scenario"); clean(); }
    const char* faults[]={"gho_abc123","\"login\"","ya29.x","\"keys\"","\"access_token\":\"new\""};
    const unsigned selected[]={0,0,1,1,2};
    for(size_t i=0;i<5;i++) {
        failResponse=faults[i]; triggered=false;
        require(!scenarios[selected[i]]() && triggered,"response allocation failure propagated");
        require(xrtErrorKind(xrtGetError())==XERR_MEMORY,"allocation error retained"); clean();
    }
    for(size_t i=0;i<5;i++) {
        require(xrtMemDebugFailAfter(0),"scenario startup failure enabled");
        require(!scenarios[i]() && xrtMemDebugFailTriggered(),"startup allocation failure propagated"); clean();
    }
    failResponse="\"access_token\":\"new\"";
    require(oauthExampleMain()==1 && triggered,"refresh failure makes main fail"); clean();
    require(oauthExampleMain()==0,"main recovery control"); clean();
    puts("[PASS] OAuth example: 5 controls, 5 response faults, 5 startup faults, refresh failure exit and recovery");
    return 0;
}
