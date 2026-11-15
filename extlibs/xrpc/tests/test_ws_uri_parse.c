#include "test.h"
#include <xrpc/ws_uri.h>
#include <xrt/memory_stats.h>
static bool uriEqual(xstrview V,cstr S){return V.Size==strlen(S)&&(!V.Size||!memcmp(V.Data,S,V.Size));}
static void uriFixture(cstr Text,bool Secure,uint16 Port,xrpcwsurihostkind Kind,cstr Host,cstr Path,cstr Query,size_t Resource)
{
    xrpcwsuri U={.Size=sizeof(U)};testRequire(xrpcWsUriParse((xstrview){Text,strlen(Text)},&U),"valid URI fixture");testRequire(U.Secure==Secure&&U.Port==Port&&U.HostKind==Kind&&uriEqual(U.Host,Host)&&uriEqual(U.Path,Path)&&uriEqual(U.Query,Query)&&U.TargetBytes==Resource,"exact URI components/defaults/resource");
}
int main(void)
{
    uriFixture("ws://host",false,80,XRPC_WS_URI_NAME,"host","","",1);
    uriFixture("WsS://host:",true,443,XRPC_WS_URI_NAME,"host","","",1);
    uriFixture("ws://host:00080?x=%23",false,80,XRPC_WS_URI_NAME,"host","","x=%23",7);
    uriFixture("wss://[::1]:65535//a/%2f?q=a?b",true,65535,XRPC_WS_URI_IPV6,"::1","//a/%2f","q=a?b",13);
    uriFixture("ws://127.0.0.1/rpc?",false,80,XRPC_WS_URI_IPV4,"127.0.0.1","/rpc","",4);
    uriFixture("ws://[v1.a:b]/",false,80,XRPC_WS_URI_IPVFUTURE,"v1.a:b","/","",1);
    uriFixture("ws://r%6fute.test/",false,80,XRPC_WS_URI_NAME,"r%6fute.test","/","",1);
    const char* Bad[]={"", "/rpc", "http://host/rpc", "wss:host/rpc", "ws:///rpc", "ws://u:p@host/", "ws://host/#", "ws://host/a#x", "ws://host/%", "ws://host/%x0", "ws://host/a b", "ws://host:0/", "ws://host:65536/", "ws://host:999999999999999999999999999/", "ws://[::1", "ws://[fe80::1%25eth0]/", "ws://host/\xc3\xa9", "ws://host/\r\nX: bad"};
    for(size_t I=0;I<sizeof(Bad)/sizeof(*Bad);++I){xrpcwsuri U;memset(&U,0xa5,sizeof(U));U.Size=sizeof(U);xrpcwsuri Before=U;testRequire(!xrpcWsUriParse((xstrview){Bad[I],strlen(Bad[I])},&U)&&!memcmp(&U,&Before,sizeof(U)),"bad URI preserves every output byte");xrtClearError();}
    char Buffer[9000];memcpy(Buffer,"ws://",5);memset(Buffer+5,'a',1024);Buffer[1029]='/';memset(Buffer+1030,'b',4095);xrpcwsuri U={.Size=sizeof(U)};testRequire(xrpcWsUriParse((xstrview){Buffer,5125},&U)&&U.TargetBytes==4096&&U.Authority.Size==1024,"exact authority/resource caps");Buffer[5125]='b';testRequire(!xrpcWsUriParse((xstrview){Buffer,5126},&U)&&xrtErrorKind(xrtGetError())==XERR_RANGE,"resource over cap");xrtClearError();
    memcpy(Buffer,"ws://",5);memset(Buffer+5,'a',1025);testRequire(!xrpcWsUriParse((xstrview){Buffer,1030},&U)&&xrtErrorKind(xrtGetError())==XERR_RANGE,"authority over cap");xrtClearError();testRequire(!xrpcWsUriParse((xstrview){Buffer,8193},&U)&&xrtErrorKind(xrtGetError())==XERR_RANGE,"total URI over cap");xrtClearError();
    union {xrpcwsuri U;char Text[256];} Alias;memset(&Alias,0,sizeof(Alias));Alias.U.Size=sizeof(Alias.U);unsigned char Saved[sizeof(Alias)];memcpy(Saved,&Alias,sizeof(Alias));testRequire(!xrpcWsUriParse((xstrview){Alias.Text,sizeof(Alias)},&Alias.U)&&!memcmp(Saved,&Alias,sizeof(Alias)),"URI/output alias rejected before mutation");xrtClearError();
    uint32 State=712367;for(unsigned Trial=0;Trial<10000;++Trial){char Text[128];int N=snprintf(Text,sizeof(Text),"w%s://h%u.test:%u/a/%%2F?x=%u?y",Trial&1?"ss":"s",Trial,1+Trial%65535,Trial);xrpcwsuri P={.Size=sizeof(P)};testRequire(N>0&&xrpcWsUriParse((xstrview){Text,(size_t)N},&P)&&P.Port==1+Trial%65535&&P.Secure==((Trial&1)!=0)&&P.HasQuery,"generated URI property");
        State=State*1664525u+1013904223u;Text[(State>>16)%(unsigned)N]=(char)(State&127);xrpcwsuri Before=P;bool OK=xrpcWsUriParse((xstrview){Text,(size_t)N},&P);if(!OK)testRequire(!memcmp(&Before,&P,sizeof(P)),"mutated failure remains atomic");else testRequire(P.Authority.Size<=1024&&P.TargetBytes<=4096&&P.Port,"mutated success remains bounded");xrtClearError();}
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"actual budget1 for valid parser path");xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
    for(unsigned I=0;I<20000;++I){xrpcwsuri P={.Size=sizeof(P)};testRequire(xrpcWsUriParse(XRT_STR_LITERAL("wss://r%6fute.test:443/a%2Fb?x=%23"),&P)&&P.RouteBytes==10&&P.TargetBytes==12,"repeat actual zero-heap parsing");}
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(xrtAllocationBudgetLeave(&Scope),"leave parser budget");xallocationbudgetinfo B={.Size=sizeof(B)};testRequire(xrtAllocationBudgetInfo(Budget,&B)&&!B.Rejections&&!B.LiveCount&&After.BlockAllocCalls==Before.BlockAllocCalls,"valid parser has no heap allocations or hidden budget refusal");xrtAllocationBudgetFree(Budget);puts("[PASS] URI standards fixtures, bounds/aliases,10000 properties/mutations and20000 actual zero-heap budget1 parses");return 0;
}
