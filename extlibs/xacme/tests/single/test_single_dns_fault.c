/* Real buffer/UDP/future allocation faults. Receive controls are deterministic
 * timeouts; real successful DNS wire responses are tested by the interop suite. */
#define XACME_MODULE_ACME_DNS
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../src/internal/xacme_dnstxt.h"
#include "../test.h"

typedef enum dnsfault { DNS_HEAD, DNS_NAME, DNS_OPEN, DNS_SEND, DNS_RECEIVE, DNS_RESEND } dnsfault;
static const char* Names[] = { "head", "name", "open", "send", "receive", "resend" };
static dnsfault Active;
static bool Enabled, Inject, Armed;
static unsigned SendCalls, ReceiveCalls;
static xerror* FirstError;

static void arm(void)
{
    if(Enabled && Inject && !Armed) {
        testRequire(xrtMemDebugFailAfter(0u), "DNS fault arm failed");
        Armed = true;
    }
}
static void remember(void)
{
    if(Enabled && Armed && xrtErrorKind(xrtGetError()) == XERR_MEMORY && FirstError == NULL)
        FirstError = xrtErrorRef(xrtGetError());
}
static bool dns_append(xbuffer* buffer, xbytesview data)
{
    if((Active == DNS_HEAD && buffer->Size == 0u) ||
       (Active == DNS_NAME && buffer->Size > 12u && buffer->Size + data.Size > buffer->Capacity)) arm();
    bool result = xrtBufferAppend(buffer, data);
    if(!result) remember();
    return result;
}
static xnetudp* dns_open(xnetengine* engine, const xnetaddr* peer, uint64 affinity,
    const xnetudpconfig* config, const xnetudpevents* events, ptr data)
{
    if(Active == DNS_OPEN) arm();
    xnetudp* result = xrtNetUdpConnect(engine, peer, affinity, config, events, data);
    if(result == NULL) remember();
    return result;
}
static xnetresult dns_send(xnetudp* udp, const void* data, size_t size)
{
    SendCalls++;
    if(Active == DNS_SEND || (Active == DNS_RESEND && SendCalls == 2u)) arm();
    xnetresult result = xrtNetUdpSend(udp, data, size);
    if(result != XNET_RESULT_OK) remember();
    return result;
}
static xnetudppacket* dns_receive(xnetudp* udp, double deadline, xcancel* cancel)
{
    ReceiveCalls++;
    if(Enabled && Inject && Active == DNS_RECEIVE && !Armed) {
        arm();
        xnetudppacket* result = __xrtNetUdpReceiveWait(udp, deadline, cancel);
        if(result == NULL) remember();
        return result;
    }
    xrtSetErrorInfo(XERR_TIMEOUT, "test.dns.receive", 91, "controlled receive timeout");
    return NULL;
}
#define xrtBufferAppend dns_append
#define xrtNetUdpConnect dns_open
#define xrtNetUdpSend dns_send
#define __xrtNetUdpReceiveWait dns_receive
#include "../../src/dns/xacme_dnstxt.c"
#undef xrtBufferAppend
#undef xrtNetUdpConnect
#undef xrtNetUdpSend
#undef __xrtNetUdpReceiveWait

static void wait_objects(xnetengine* engine, size_t expected)
{
    xnetenginestats stats;
    double deadline = __xrtWaitAfter(INT64_C(5000));
    do {
        testRequire(xrtNetEngineStats(engine, &stats), "DNS engine stats failed");
        if(stats.LiveObjects == expected) return;
        xrtSleep(1u);
    } while(!__xrtWaitExpired(deadline));
    fprintf(stderr, "DNS objects live=%zu expected=%zu\n", stats.LiveObjects, expected);
    testRequire(false, "DNS query retained UDP objects");
}
static void run_fault(dnsfault fault, bool inject, bool contains)
{
    xnetengineconfig config;
    xrtNetEngineConfigInit(&config);
    config.Workers = 1u;
    xnetengine* engine = xrtNetEngineCreate(&config);
    testRequire(engine != NULL && xrtNetEngineStart(engine), "DNS engine setup failed");
    xacmedns dns;
    testRequire(xacmeDnsInit(&dns, engine), "DNS borrower setup failed");
    xnetaddr address;
    testRequire(xrtNetAddrParse(&address, "127.0.0.1", 0u), "DNS peer setup failed");
    xnetudp* listener = xrtNetUdpBind(engine, &address, 0u, NULL, NULL, NULL);
    testRequire(listener != NULL && xrtNetUdpLocal(listener, &address), "DNS listener setup failed");
    char name[256], records[4][XACME_TXT_RECORD_MAX];
    memset(name, 'a', sizeof(name));
    name[63] = name[127] = name[191] = '.';
    name[222] = '\0';
    size_t count = SIZE_MAX;
    bool found = true;
    Active = fault; Enabled = true; Inject = inject; Armed = false; FirstError = NULL;
    SendCalls = ReceiveCalls = 0u;
    xrtClearError();
    bool result = contains?xacmeDnsTxtContains(&dns,"127.0.0.1",address.Port,name,"probe",&found):
        xacmeDnsTxtQuery(&dns, "127.0.0.1", address.Port, name, records, 4u, &count);
    bool triggered = xrtMemDebugFailTriggered(), identity = FirstError != NULL && xrtGetError() == FirstError;
    xerrkind kind = xrtErrorKind(xrtGetError());
    printf("[diagnostic] DNS_FAULT %s/%s/%s result=%u count=%zu triggered=%u kind=%u identity=%u sends=%u receives=%u\n",
        contains?"contains":"collect",Names[fault], inject ? "oom" : "control", (unsigned)result, count, (unsigned)triggered,
        (unsigned)kind, (unsigned)identity, SendCalls, ReceiveCalls);
    testRequire(!result && (contains?!found:count==0u) && triggered == inject, "DNS ignored fault or returned partial output");
    if(inject) {
        testRequire(kind == XERR_MEMORY && identity, "DNS replaced allocator cause");
        testRequire(SendCalls == (fault < DNS_SEND ? 0u : fault == DNS_RESEND ? 2u : 1u),
            "DNS sent after terminal allocation failure");
        testRequire(ReceiveCalls == (fault < DNS_RECEIVE ? 0u : 1u), "DNS received again after allocation failure");
    } else testRequire(kind == XERR_TIMEOUT && SendCalls == 2u && ReceiveCalls == 2u,
        "DNS ordinary timeout retry changed");
    xrtMemDebugFailClear(); Enabled = false;
    xerror* previous = xrtErrorRef(xrtGetError());
    wait_objects(engine, 1u);
    testRequire(xrtGetError() == previous, "DNS object cleanup replaced error");
    xrtErrorFree(previous);
    xrtClearError(); count = SIZE_MAX; SendCalls = ReceiveCalls = 0u;
    found=true;
    result=contains?xacmeDnsTxtContains(&dns,"127.0.0.1",address.Port,name,"probe",&found):
        xacmeDnsTxtQuery(&dns,"127.0.0.1",address.Port,name,records,4u,&count);
    testRequire(!result && (contains?!found:count==0u) && xrtErrorKind(xrtGetError()) == XERR_TIMEOUT && SendCalls == 2u && ReceiveCalls == 2u,
        "DNS same borrower recovery query failed");
    wait_objects(engine, 1u);
    testRequire(xrtNetUdpClose(listener), "DNS listener close failed");
    xrtNetUdpDestroy(listener);
    wait_objects(engine, 0u);
    testRequire(xacmeDnsUnit(&dns) && xrtNetEngineState(engine) == XNET_ENGINE_RUNNING,
        "DNS borrower cleanup stopped engine");
    testRequire(xrtNetEngineDestroy(engine), "DNS engine cleanup failed");
    xrtErrorFree(FirstError); FirstError = NULL; xrtClearError();
    testRequire(xrtMemDebugReset(), "DNS retained allocations or invalid frees");
    printf("[PASS] DNS_FAULT %s/%s/%s\n",contains?"contains":"collect",Names[fault], inject ? "oom" : "control");
}
static void parse_case(bool null_count)
{
    static const uint8 packet[] = {
        0x12,0x34,0x81,0x80,0,0,0,2,0,0,0,0,
        0,0,16,0,1,0,0,0,0,0,2,1,'a',
        0,0,16,0,1,0,0,0,0,0,2,2,'b'
    };
    char records[4][XACME_TXT_RECORD_MAX]; size_t count = SIZE_MAX;
    bool result = xacmeTxtParseResponse(packet, sizeof(packet), 0x1234u, records, 4u, null_count ? NULL : &count);
    printf("[diagnostic] DNS_PARSE %s result=%u count=%zu\n", null_count ? "null-count" : "partial-count", (unsigned)result, count);
    testRequire(!result && (null_count || count == 0u), "DNS parser accepted invalid output or published partial count");
    testRequire(xrtMemDebugReset(), "DNS parser retained memory");
    printf("[PASS] DNS_PARSE %s\n", null_count ? "null-count" : "partial-count");
}
typedef struct dnspacket { uint8 Data[4096];size_t Size; } dnspacket;
static void packet_init(dnspacket* packet,uint16 answers) {
    memset(packet,0,sizeof(*packet));packet->Size=12u;
    packet->Data[0]=0x12;packet->Data[1]=0x34;packet->Data[2]=0x81;packet->Data[3]=0x80;
    packet->Data[6]=(uint8)(answers>>8u);packet->Data[7]=(uint8)answers;
}
static void packet_txt(dnspacket* packet,const uint8* data,size_t size) {
    static const uint8 Head[]={0,0,16,0,1,0,0,0,0};
    testRequire(packet->Size+sizeof(Head)+2u+size<=sizeof(packet->Data) && size<=65535u,"DNS matrix packet capacity");
    memcpy(packet->Data+packet->Size,Head,sizeof(Head));packet->Size+=sizeof(Head);
    packet->Data[packet->Size++]=(uint8)(size>>8u);packet->Data[packet->Size++]=(uint8)size;
    if(size) memcpy(packet->Data+packet->Size,data,size);
    packet->Size+=size;
}
static void parser_matrix(void) {
    static const cstr Names[]={"ordinary","empty-string","empty-segments","zero-rdata","embedded-nul",
        "malformed-after-capacity","self-pointer","outside-pointer","forward-pointer","missing-authority",
        "missing-additional","truncated-flag","trailing-wire","malformed-nxdomain","maximum-text"};
    static const uint8 Probe[]={5,'p','r','o','b','e'}, Empty[]={0}, Split[]={0,2,'p','r',0,3,'o','b','e',0};
    static const uint8 Nul[]={7,'p','r','o','b','e',0,'x'}, Bad[]={2,'x'};
    unsigned Failed=0;
    for(size_t row=0;row<sizeof(Names)/sizeof(Names[0]);row++) {
        dnspacket Packet;char Records[2][XACME_TXT_RECORD_MAX];size_t Count=SIZE_MAX;
        bool Expected=row==0u || row==1u || row==2u || row==14u;
        packet_init(&Packet,row==5u?2u:1u);packet_txt(&Packet,Probe,sizeof(Probe));
        if(row==1u || row==2u || row==3u || row==4u || row==14u) {
            packet_init(&Packet,1u);
            if(row==1u) packet_txt(&Packet,Empty,sizeof(Empty));
            if(row==2u) packet_txt(&Packet,Split,sizeof(Split));
            if(row==3u) packet_txt(&Packet,NULL,0u);
            if(row==4u) packet_txt(&Packet,Nul,sizeof(Nul));
            if(row==14u) {uint8 Data[256];Data[0]=255u;memset(Data+1,'z',255u);packet_txt(&Packet,Data,sizeof(Data));}
        }
        if(row==5u) packet_txt(&Packet,Bad,sizeof(Bad));
        if(row>=6u && row<=8u) {
            memmove(Packet.Data+14u,Packet.Data+13u,Packet.Size-13u);Packet.Size++;
            Packet.Data[12]=0xc0u;Packet.Data[13]=(uint8)(row==6u?12u:row==7u?255u:14u);
            if(row==7u) Packet.Data[12]=0xffu;
        }
        if(row==9u) Packet.Data[9]=1u;
        if(row==10u) Packet.Data[11]=1u;
        if(row==11u) Packet.Data[2]|=2u;
        if(row==12u) Packet.Data[Packet.Size++]=0;
        if(row==13u) {packet_init(&Packet,0u);Packet.Data[3]=0x83u;Packet.Data[5]=1u;}
        memset(Records,0xa5,sizeof(Records));xrtClearError();
        testRequire(xrtMemDebugFailAfter(0),"DNS parser zero-allocation probe arm");
        bool Result=xacmeTxtParseResponse(Packet.Data,Packet.Size,0x1234u,Records,1u,&Count);
        bool Triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        bool Ok=Result==Expected && Count==(Expected?1u:0u) && !Triggered;
        for(size_t i=0;i<sizeof(Records[1]);i++) Ok=Ok && (uint8)Records[1][i]==0xa5u;
        if(Expected) {
            size_t Length=row==1u?0u:row==14u?255u:5u;
            Ok=Ok && strlen(Records[0])==Length;
            if(Length==5u) Ok=Ok && strcmp(Records[0],"probe")==0;
        }
        printf("[dns-parser] %s success=%d count=%zu ok=%d\n",Names[row],Result,Count,Ok);
        if(!Ok) Failed++;
        xrtClearError();testMemoryDebugDrain("DNS parser matrix allocation balance");
    }
    printf("[dns-parser-summary] rows=%zu failed=%u\n",sizeof(Names)/sizeof(Names[0]),Failed);
    testRequire(Failed==0u,"DNS text parser accepted malformed wire or rejected valid character strings");
}
static void match_matrix(void) {
    static const cstr Names[]={"fifth","sixty-fifth","absent","segmented-empty","empty-before-match",
        "nul-prefix","binary-before-match","oversized-before-match","malformed-after-match","truncated",
        "authority-only","additional-only","id-mismatch","null-view","null-output","binary-exact","empty-exact","negative-answer"};
    static const uint8 Probe[]={5,'p','r','o','b','e'}, Other[]={1,'x'}, Empty[]={0};
    static const uint8 Split[]={0,2,'p','r',0,3,'o','b','e',0}, Nul[]={7,'p','r','o','b','e',0,'x'}, Bad[]={2,'x'};
    unsigned Failed=0;
    for(size_t row=0;row<sizeof(Names)/sizeof(Names[0]);row++) {
        dnspacket Packet;bool Found=true;uint16 Id=0x1234u;
        xbytesview Expected=XRT_BYTES_LITERAL("probe");
        bool Valid=row!=8u && row!=9u && row!=12u && row!=13u && row!=14u;
        bool Want=row==0u || row==1u || row==3u || row==4u || row==6u || row==7u || row==15u || row==16u;
        packet_init(&Packet,row==0u?5u:row==1u?65u:row==4u || row==6u || row==7u || row==8u?2u:1u);
        if(row<=1u) for(size_t i=0;i<(row==0u?4u:64u);i++) packet_txt(&Packet,Other,sizeof(Other));
        if(row==4u) packet_txt(&Packet,Empty,sizeof(Empty));
        if(row==6u) packet_txt(&Packet,Nul,sizeof(Nul));
        if(row==7u) {uint8 Long[258];Long[0]=255u;memset(Long+1,'x',255u);Long[256]=1u;Long[257]='y';packet_txt(&Packet,Long,sizeof(Long));}
        packet_txt(&Packet,row==2u?Other:row==3u?Split:row==5u || row==15u?Nul:row==16u?Empty:Probe,
            row==2u?sizeof(Other):row==3u?sizeof(Split):row==5u || row==15u?sizeof(Nul):row==16u?sizeof(Empty):sizeof(Probe));
        if(row==8u) packet_txt(&Packet,Bad,sizeof(Bad));
        if(row==9u) Packet.Data[2]|=2u;
        if(row==10u || row==11u) {Packet.Data[7]=0;Packet.Data[row==10u?9u:11u]=1u;}
        if(row==12u) Id^=1u;
        if(row==13u) Expected=(xbytesview){NULL,5u};
        if(row==15u) Expected=(xbytesview){Nul+1u,sizeof(Nul)-1u};
        if(row==16u) Expected=(xbytesview){NULL,0u};
        if(row==17u) Packet.Data[3]=0x83u;
        testRequire(xrtMemDebugFailAfter(0),"DNS exact-match zero-allocation probe arm");
        bool Result=xacmeTxtResponseContains(Packet.Data,Packet.Size,Id,Expected,row==14u?NULL:&Found);
        bool Triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        bool Ok=Result==Valid && !Triggered && (row==14u || Found==(Valid && Want));
        printf("[dns-match] %s success=%d found=%d ok=%d\n",Names[row],Result,Found,Ok);
        if(!Ok) Failed++;
        xrtClearError();testMemoryDebugDrain("DNS match parser allocated or retained memory");
    }
    printf("[dns-match-summary] rows=%zu failed=%u\n",sizeof(Names)/sizeof(Names[0]),Failed);
    testRequire(Failed==0u,"DNS exact comparison lost late/binary values or published malformed/truncated answers");
}
int main(int argc, char** argv)
{
    if(argc==2 && strcmp(argv[1],"match-matrix")==0) {match_matrix();return 0;}
    if(argc==2 && strcmp(argv[1],"parser-matrix")==0) {parser_matrix();return 0;}
    testRequire(argc == 1 || argc == 2 || argc == 3, "DNS fixture usage: [null-count|partial-count|fault control|oom]");
    if(argc == 2) {
        testRequire(strcmp(argv[1], "null-count") == 0 || strcmp(argv[1], "partial-count") == 0, "unknown DNS parser case");
        parse_case(strcmp(argv[1], "null-count") == 0); return 0;
    }
    for(unsigned fault = 0u; fault < 6u; fault++) {
        if(argc == 3 && strcmp(argv[1], Names[fault]) != 0) continue;
        if(argc == 3) {
            testRequire(strcmp(argv[2], "control") == 0 || strcmp(argv[2], "oom") == 0, "unknown DNS fault mode");
            run_fault((dnsfault)fault, strcmp(argv[2], "oom") == 0,false); return 0;
        }
        run_fault((dnsfault)fault, false,false); run_fault((dnsfault)fault, true,false);
        run_fault((dnsfault)fault, false,true); run_fault((dnsfault)fault, true,true);
    }
    testRequire(argc == 1, "unknown DNS fault case");
    parse_case(true); parse_case(false);parser_matrix();match_matrix();return 0;
}
