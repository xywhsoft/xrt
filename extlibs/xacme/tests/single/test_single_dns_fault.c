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
static void run_fault(dnsfault fault, bool inject)
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
    Active = fault; Enabled = true; Inject = inject; Armed = false; FirstError = NULL;
    SendCalls = ReceiveCalls = 0u;
    xrtClearError();
    bool result = xacmeDnsTxtQuery(&dns, "127.0.0.1", address.Port, name, records, 4u, &count);
    bool triggered = xrtMemDebugFailTriggered(), identity = FirstError != NULL && xrtGetError() == FirstError;
    xerrkind kind = xrtErrorKind(xrtGetError());
    printf("[diagnostic] DNS_FAULT %s/%s result=%u count=%zu triggered=%u kind=%u identity=%u sends=%u receives=%u\n",
        Names[fault], inject ? "oom" : "control", (unsigned)result, count, (unsigned)triggered,
        (unsigned)kind, (unsigned)identity, SendCalls, ReceiveCalls);
    testRequire(!result && count == 0u && triggered == inject, "DNS ignored fault or returned partial output");
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
    testRequire(!xacmeDnsTxtQuery(&dns, "127.0.0.1", address.Port, name, records, 4u, &count) &&
        count == 0u && xrtErrorKind(xrtGetError()) == XERR_TIMEOUT && SendCalls == 2u && ReceiveCalls == 2u,
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
    printf("[PASS] DNS_FAULT %s/%s\n", Names[fault], inject ? "oom" : "control");
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
int main(int argc, char** argv)
{
    testRequire(argc == 1 || argc == 2 || argc == 3, "DNS fixture usage: [null-count|partial-count|fault control|oom]");
    if(argc == 2) {
        testRequire(strcmp(argv[1], "null-count") == 0 || strcmp(argv[1], "partial-count") == 0, "unknown DNS parser case");
        parse_case(strcmp(argv[1], "null-count") == 0); return 0;
    }
    for(unsigned fault = 0u; fault < 6u; fault++) {
        if(argc == 3 && strcmp(argv[1], Names[fault]) != 0) continue;
        if(argc == 3) {
            testRequire(strcmp(argv[2], "control") == 0 || strcmp(argv[2], "oom") == 0, "unknown DNS fault mode");
            run_fault((dnsfault)fault, strcmp(argv[2], "oom") == 0); return 0;
        }
        run_fault((dnsfault)fault, false); run_fault((dnsfault)fault, true);
    }
    testRequire(argc == 1, "unknown DNS fault case");
    parse_case(true); parse_case(false); return 0;
}
