#if !defined(_WIN32) && !defined(_WIN64) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "../test.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
static unsigned Notices;
static void ntNotice(const xerror* Error,ptr Context){(void)Error;(void)Context;++Notices;}
static void ntInvalid(xstrview Text)
{xnetaddr A,Before;memset(&A,0xa5,sizeof(A));Before=A;testRequire(!xrtNetAddrTryParse(&A,Text,65535)&&!memcmp(&A,&Before,sizeof(A)),"quiet refusal preserves complete output");}
static void ntValid(cstr Text,xnetfamily Family,uint32 Scope,const unsigned char* Bytes)
{
    xnetaddr A;memset(&A,0xa5,sizeof(A));testRequire(xrtNetAddrTryParse(&A,(xstrview){Text,strlen(Text)},65535),"counted numeric address");
    testRequire(A.Family==Family&&A.Port==65535&&A.Scope==Scope&&!memcmp(A.Address,Bytes,Family==XNET_FAMILY_IPV4?4:16),"independent family, port, scope and address bytes");
}
static void ntGuarded(void)
{
    size_t Page;unsigned char* Mapping;
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO Info;GetSystemInfo(&Info);Page=Info.dwPageSize;Mapping=VirtualAlloc(NULL,Page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);testRequire(Mapping!=NULL,"guard mapping");DWORD Old;testRequire(VirtualProtect(Mapping+Page,Page,PAGE_NOACCESS,&Old)!=0,"guard page protection");
#else
    Page=(size_t)sysconf(_SC_PAGESIZE);Mapping=mmap(NULL,Page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);testRequire(Mapping!=MAP_FAILED,"guard mapping");testRequire(mprotect(Mapping+Page,Page,PROT_NONE)==0,"guard page protection");
#endif
    static const cstr Texts[]={"127.0.0.1","2001:db8::1","fe80::1%42","not-an-ip"};
    for(size_t I=0;I<sizeof(Texts)/sizeof(*Texts);++I){size_t N=strlen(Texts[I]);unsigned char* At=Mapping+Page-N;memcpy(At,Texts[I],N);xnetaddr A;
        testRequire(xrtNetAddrTryParse(&A,(xstrview){(const char*)At,N},7)==(I!=3),"non-NUL view stops exactly before inaccessible page");}
#if defined(_WIN32) || defined(_WIN64)
    testRequire(VirtualFree(Mapping,0,MEM_RELEASE)!=0,"guard mapping release");
#else
    testRequire(munmap(Mapping,Page*2)==0,"guard mapping release");
#endif
}
static uint32 ntNext(uint32* S){*S^=*S<<13;*S^=*S>>17;*S^=*S<<5;return *S;}
int main(void)
{
    static const unsigned char A4[]={127,0,0,1},A6[16]={[15]=1},B6[16]={0x20,1,0x0d,0xb8,[15]=1},M6[16]={[10]=255,[11]=255,192,0,2,1},S6[16]={0xfe,0x80,[15]=1};
    xrtSetErrorInfo(XERR_STATE,"net.try.ambient",3,"must stay untouched");const xerror* Ambient=xrtGetError();testRequire(Ambient!=NULL,"quiet baseline ambient error");xrtSetErrorHandler(ntNotice,NULL);
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"pure parsing under one-byte budget");
    ntValid("127.0.0.1",XNET_FAMILY_IPV4,0,A4);ntValid("::1",XNET_FAMILY_IPV6,0,A6);ntValid("2001:db8::1",XNET_FAMILY_IPV6,0,B6);ntValid("::ffff:192.0.2.1",XNET_FAMILY_IPV6,0,M6);ntValid("fe80::1%4294967295",XNET_FAMILY_IPV6,UINT32_MAX,S6);ntValid("fe80::1%0",XNET_FAMILY_IPV6,0,S6);
    static const cstr Invalid[]={"","broker.fixture","01.2.3.4","256.0.0.1","127.0.0.1.","127.0.0.1:443","[::1]","2001:db8:::1","fe80::1%","fe80::1%-1","fe80::1%4294967296","fe80::1%not-an-interface","fe80::1%3%4"," ::1","::1 "};
    for(size_t I=0;I<sizeof(Invalid)/sizeof(*Invalid);++I)ntInvalid((xstrview){Invalid[I],strlen(Invalid[I])});
    ntInvalid((xstrview){"127.0.0.1\0x",11});ntInvalid((xstrview){NULL,0});ntInvalid((xstrview){NULL,1});ntInvalid((xstrview){(const char*)(UINTPTR_MAX-3),8});
    testRequire(!xrtNetAddrTryParse(NULL,XRT_STR_LITERAL("::1"),1),"NULL output stays quiet");
    xnetaddr Alias;memset(&Alias,0,sizeof(Alias));memcpy(&Alias,"::1",3);xnetaddr Before=Alias;testRequire(!xrtNetAddrTryParse(&Alias,(xstrview){(const char*)&Alias,3},1)&&!memcmp(&Alias,&Before,sizeof(Alias)),"overlapping output rejected without modification");
    ntGuarded();testRequire(xrtGetError()==Ambient&&!Notices,"all pure paths preserve error identity and handler silence");testRequire(xrtAllocationBudgetLeave(&Scope),"pure budget leave");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&!BI.Rejections&&!BI.PeakBytes&&!BI.LiveCount,"no pure-path XRT allocation or hidden diagnostic allocation");xrtAllocationBudgetFree(Budget);xrtSetErrorHandler(NULL,NULL);xrtClearError();
    /* Legacy diagnostic parser supplies the unchanged existing grammar as a
     * differential regression; fixed bytes and guard pages are independent. */
    uint32 Seed=0x1d4a8e37;static const cstr Seeds[]={"127.0.0.1","2001:db8::1","::ffff:192.0.2.1","fe80::1%42","broker.fixture"};
    for(unsigned I=0;I<10000;++I){char Text[96];size_t N=strlen(Seeds[I%5]);memcpy(Text,Seeds[I%5],N+1);
        if(I%3){size_t At=ntNext(&Seed)%N;Text[At]="0123456789abcdef:.%xyz "[ntNext(&Seed)%22];}if(I%7==0)Text[N=ntNext(&Seed)%(N+1)]=0;
        xnetaddr Legacy,Quiet,Guard;memset(&Quiet,0x6d,sizeof(Quiet));Guard=Quiet;bool Expected=xrtNetAddrParse(&Legacy,Text,49152);const xerror* Prior=xrtGetError();unsigned Was=Notices;xrtSetErrorHandler(ntNotice,NULL);
        bool Got=xrtNetAddrTryParse(&Quiet,(xstrview){Text,N},49152);testRequire(Got==Expected&&(Got?!memcmp(&Quiet,&Legacy,sizeof(Quiet)):!memcmp(&Quiet,&Guard,sizeof(Quiet)))&&xrtGetError()==Prior&&Notices==Was,"10000 seeded grammar/output/ambient differential cases");xrtSetErrorHandler(NULL,NULL);xrtClearError();}
    puts("[PASS] quiet counted IP parser: exact bytes, numeric scopes, invalid spans/alias, guarded non-NUL input, one-byte budget and10000 seeded differential cases");return 0;
}
