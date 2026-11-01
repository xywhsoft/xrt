/* Provider budget primitives and callback identity on actual Core allocation,
 * errors and mutexes. Only budget expiry/sleep scheduling is controlled;
 * signed real TCP/TLS coverage lives in test_acme_provider_wire.py. */
#define XACME_MODULE_DNS_ALI
#define XACME_MODULE_DNS_CF
#define XACME_MODULE_DNS_TENCENT
#define XACME_MODULE_DNS_AWS
#define XACME_MODULE_DNS_HUAWEI
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include <xrt/detail/wait.h>
#include "../test.h"
#include "../../src/acme/xacme_http.c"

static bool RetryProbe, RetryTencent, RetryMemory;
static unsigned RetryCalls;
static xerror* RetryFirst;
static xerror* RetryLatestMemory;
static bool retry_failure(xacmehttp* http,xacmehttpresponse* response)
{
    memset(response,0,sizeof(*response));RetryCalls++;
    http->bWriteUncertain=RetryTencent;
    if(RetryCalls==1u) {
        xrtSetErrorInfo(XERR_IO,"test.retry.first",401,"actual first read error");
        RetryFirst=xrtErrorRef(xrtGetError());
    } else {
        http->OperationDeadline=__xrtWaitAfter(0);
        xrtClearError();
        if(RetryMemory) {
            testRequire(xrtMemDebugFailAfter(0),"retry producer memory arm");
            testRequire(xrtMalloc(8u)==NULL && xrtMemDebugFailTriggered(),"retry actual producer memory");
            RetryLatestMemory=xrtErrorRef(xrtGetError());xrtMemDebugFailClear();
        } else xrtSetErrorInfo(XERR_TIMEOUT,"test.retry.later",402,"later request expired");
    }
    return false;
}
static bool retry_once(xacmehttp* http,cstr method,cstr url,cstr type,xstrview body,
    const xacmehttpheader* extra,size_t count,xacmehttpresponse* response)
{
    return RetryProbe?retry_failure(http,response):xacmeHttpExchangeOnceV(http,method,url,type,body,extra,count,response);
}
static bool retry_many(xacmehttp* http,cstr method,cstr url,cstr type,xstrview body,
    const xacmehttpheader* extra,size_t count,xacmehttpresponse* response)
{
    return RetryProbe?retry_failure(http,response):xacmeHttpExchangeV(http,method,url,type,body,extra,count,response);
}


static bool ForceExpired, ObserveSleep, ExpireAfterSleep;
static unsigned SleepCalls, CustomCalls;
static int64 LastSleep;
static bool probe_expired(double Deadline)
{
    return ForceExpired && isfinite(Deadline) ? true : __xrtWaitExpired(Deadline);
}
static void probe_sleep(int64 Ms)
{
    if(!ObserveSleep) { xrtSleep(Ms);return; }
    SleepCalls++;LastSleep=Ms;
    if(ExpireAfterSleep) ForceExpired=true;
}
#define xacmeHttpExchangeOnceV retry_once
#define xacmeHttpExchangeV retry_many
#define __xrtWaitExpired probe_expired
#define xrtSleep probe_sleep
#include "../../src/dns/xacme_dns_aws_txt.c"
#include "../../src/dns/xacme_dns_aws.c"
#include "../../src/dns/xacme_dns_ali.c"
#include "../../src/dns/xacme_dns_cf.c"
#include "../../src/dns/xacme_dns_tencent.c"
#include "../../src/dns/xacme_dns_huawei.c"
#include "../../src/acme/xacme_dns.c"
#undef xrtSleep
#undef __xrtWaitExpired
#undef xacmeHttpExchangeOnceV
#undef xacmeHttpExchangeV

static bool custom_add(xacmednsprovider* Provider, xstrview Fqdn, xstrview Txt)
{
    (void)Provider;(void)Fqdn;(void)Txt;CustomCalls++;return true;
}
static bool custom_remove(xacmednsprovider* Provider, xstrview Fqdn, xstrview Txt)
{
    (void)Provider;(void)Fqdn;(void)Txt;return true;
}

static xerror* actual_memory(void)
{
    testRequire(xrtMemDebugFailAfter(0u),"provider budget allocator arm");
    testRequire(xrtMalloc(8u)==NULL && xrtMemDebugFailTriggered(),"provider budget real allocation failure");
    xerror* Error=xrtErrorRef(xrtGetError());
    xrtMemDebugFailClear();
    testRequire(xrtErrorKind(Error)==XERR_MEMORY,"provider budget real MEMORY cause");
    return Error;
}
static void require_empty(void)
{
    xmemdebugsnapshot Memory;
    ForceExpired=false;ObserveSleep=false;ExpireAfterSleep=false;
    xrtClearError();xrtMemDebugSnapshot(&Memory);
    testRequire(Memory.LiveCount==0u && Memory.LiveBytes==0u &&
        Memory.InvalidFreeCount==0u && Memory.DoubleFreeCount==0u,
        "provider budget primitive retained ownership");
}


static bool retry_case(bool tencent,bool memory)
{
    xacmednsalicontext ali={0};xacmednstencentcontext tc={0};
    bool result,contains=false,ok;str body=NULL;uint16 status=0;size_t size=0;
    RetryTencent=tencent;RetryMemory=memory;RetryCalls=0;RetryProbe=true;
    ObserveSleep=true;ForceExpired=false;ExpireAfterSleep=false;
    xacmehttp* http=tencent?&tc.Http:&ali.Http;
    http->bOperationDeadline=true;http->OperationDeadline=__xrtWaitAfter(1000);
    strcpy(ali.sKeyId,"test-id");strcpy(ali.sSecret,"test-key");strcpy(ali.sEndpoint,"api.example.test");
    strcpy(tc.sId,"test-id");strcpy(tc.sKey,"test-key");strcpy(tc.sEndpoint,"api.example.test");
    result=tencent?xacmeTencentReadCall(&tc,"DescribeDomain","{\"Domain\":\"example.com\"}",&status,&body,&size):
        xacmeAliCall(&ali,"DescribeDomainRecords","DomainName=example.com&PageSize=1",&status,&body);
    for(const xerror* error=xrtGetError();error!=NULL;error=xrtErrorCause(error))if(error==RetryFirst)contains=true;
    ok=!result && RetryCalls==2u && (memory?xrtGetError()==RetryLatestMemory:contains);
    printf("RETRY_CAUSE provider=%s memory=%d calls=%u first_retained=%d kind=%d result=%d\n",tencent?"tencent":"ali",memory,RetryCalls,contains,(int)xrtErrorKind(xrtGetError()),ok);
    xrtFree(body);xrtErrorFree(RetryFirst);xrtErrorFree(RetryLatestMemory);
    RetryFirst=NULL;RetryLatestMemory=NULL;RetryProbe=false;require_empty();
    return ok;
}

int main(void)
{
    const xstrview Owner=XRT_STR_LITERAL("_acme-challenge.example.com");
    const xstrview Txt=XRT_STR_LITERAL("digest_01");
    const cstr Ids[]={"ali","cf","tencent","aws","huawei"};
    const xacmednsaddproc Adds[]={xacmeAliAdd,xacmeCfAdd,xacmeTencentAdd,xacmeAwsAdd,xacmeHuaweiAdd};
    const xacmednsremoveproc Removes[]={xacmeAliRemove,xacmeCfRemove,xacmeTencentRemove,xacmeAwsRemove,xacmeHuaweiRemove};
    xacmednsprovider Provider={0};
    xacmehttp Http={0};
    xacmednsbudgetscope Scope;
    xerror* Cause;
    double Deadline,Previous;
    testRequire(xrtMemDebugEnable(true),"provider budget memory debug enable");
    for(size_t i=0u;i<5u;i++) {
        /* A colliding id and poison context may never trigger a private cast. */
        Provider.sId=Ids[i];Provider.pContext=(ptr)(uintptr_t)1u;
        Provider.Add=custom_add;Provider.Remove=custom_remove;CustomCalls=0u;
        testRequire(xacmeDnsProviderAddUntil(&Provider,Owner,Txt,INFINITY) &&
            xacmeDnsProviderAddUntil(&Provider,Owner,Txt,__xrtWaitAfter(1000)) &&
            CustomCalls==2u,"provider id collision entered a built-in context");
        Provider.Remove=Removes[i];
        testRequire(xacmeDnsProviderAddUntil(&Provider,Owner,Txt,INFINITY) &&
            CustomCalls==3u,"provider mixed callback pair entered a built-in context");
        Provider.Add=Adds[i];Provider.pContext=NULL;
        testRequire(!xacmeDnsProviderAddUntil(&Provider,Owner,Txt,INFINITY) &&
            xrtErrorKind(xrtGetError())==XERR_STATE,"provider released context state");
        require_empty();
    }
    Provider.Add=custom_add;Provider.Remove=custom_remove;Provider.pContext=NULL;CustomCalls=0u;
    testRequire(!xacmeDnsProviderAddUntil(&Provider,Owner,Txt,NAN) &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT && CustomCalls==0u,"provider invalid deadline invoked host");
    testRequire(!xacmeDnsProviderAddUntil(&Provider,Owner,Txt,-INFINITY) &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT && CustomCalls==0u,"provider negative infinity invoked host");
    Cause=actual_memory();ForceExpired=true;
    testRequire(!xacmeDnsProviderAddUntil(&Provider,Owner,Txt,__xrtWaitAfter(1000)) &&
        xrtErrorKind(xrtGetError())==XERR_TIMEOUT && CustomCalls==0u,
        "provider stale caller MEMORY masked a new expired operation");
    xrtErrorFree(Cause);require_empty();

    xrtSetErrorInfo(XERR_IO,"test.provider.budget",301,"actual error object from failed read");
    Cause=xrtErrorRef(xrtGetError());ForceExpired=true;
    testRequire(!xacmeDnsDeadlineBudget(__xrtWaitAfter(1000)) &&
        xrtErrorKind(xrtGetError())==XERR_TIMEOUT && xrtErrorCause(xrtGetError())==Cause,
        "provider deadline lost actual read cause identity");
    xrtErrorFree(Cause);require_empty();

    Cause=actual_memory();ForceExpired=true;
    testRequire(!xacmeDnsDeadlineBudget(__xrtWaitAfter(1000)) && xrtGetError()==Cause,
        "provider elapsed deadline replaced producer MEMORY");
    xrtErrorFree(Cause);require_empty();

    xrtSetErrorInfo(XERR_IO,"test.provider.budget",302,"actual error before metadata OOM");
    Cause=xrtErrorRef(xrtGetError());ForceExpired=true;
    testRequire(xrtMemDebugFailAfter(0u),"provider metadata OOM arm");
    testRequire(!xacmeDnsDeadlineBudget(__xrtWaitAfter(1000)) && xrtMemDebugFailTriggered() &&
        xrtErrorKind(xrtGetError())==XERR_MEMORY,"provider timeout context OOM lost MEMORY");
    xrtMemDebugFailClear();xrtErrorFree(Cause);require_empty();

    Previous=__xrtWaitAfter(10000);Http.bOperationDeadline=true;Http.OperationDeadline=Previous;
    testRequire(xacmeDnsProviderBegin(&Http,Previous+5.0,&Scope) &&
        Http.OperationDeadline==Previous,"provider widened an existing enclosing budget");
    xacmeDnsProviderEnd(&Http,&Scope);
    Deadline=Previous-5.0;
    testRequire(xacmeDnsProviderBegin(&Http,Deadline,&Scope) && Http.OperationDeadline==Deadline,
        "provider failed to choose earlier deadline");
    xacmeDnsProviderEnd(&Http,&Scope);
    testRequire(Http.bOperationDeadline && Http.OperationDeadline==Previous,"provider lost previous scope");
    Http.bOperationDeadline=false;Http.OperationDeadline=0.0;ForceExpired=true;
    testRequire(!xacmeDnsProviderBegin(&Http,Deadline,&Scope) &&
        !Http.bOperationDeadline && Http.OperationDeadline==0.0,"failed provider entry retained deadline state");
    require_empty();

    Http.bOperationDeadline=true;Http.OperationDeadline=__xrtWaitAfter(20);
    SleepCalls=0u;ObserveSleep=true;ExpireAfterSleep=true;
    xrtSetErrorInfo(XERR_IO,"test.provider.budget",303,"actual retry failure");
    Cause=xrtErrorRef(xrtGetError());
    testRequire(!xacmeDnsOperationSleep(&Http,500u) && SleepCalls==1u && LastSleep>0 && LastSleep<=21 &&
        xrtErrorKind(xrtGetError())==XERR_TIMEOUT && xrtErrorCause(xrtGetError())==Cause,
        "provider retry sleep restarted budget or lost cause");
    xrtErrorFree(Cause);require_empty();
    SleepCalls=0u;ObserveSleep=true;ForceExpired=true;
    testRequire(!xacmeDnsOperationSleep(&Http,500u) && SleepCalls==0u,"provider expired retry slept");
    require_empty();
    Http.bOperationDeadline=false;ObserveSleep=true;SleepCalls=0u;
    testRequire(xacmeDnsOperationSleep(&Http,7u) && SleepCalls==1u && LastSleep==7,
        "provider independent cleanup lost ordinary retry delay");
    require_empty();
    {
        xmutex Invalid={0};ObserveSleep=true;SleepCalls=0u;
        testRequire(!xacmeDnsProviderLock(&Invalid,__xrtWaitAfter(1000)) &&
            xrtErrorKind(xrtGetError())==XERR_STATE && SleepCalls==0u,
            "provider invalid lock was retried as contention");
    }
    require_empty();
    for(unsigned provider=0u;provider<2u;provider++)
        for(unsigned memory=0u;memory<2u;memory++)
            testRequire(retry_case(provider!=0u,memory!=0u),"provider signed retry lost first cause or producer MEMORY");
    printf("[PASS] provider budget identity, causes, MEMORY, scopes, sleep and lock guards\n");
    return 0;
}
