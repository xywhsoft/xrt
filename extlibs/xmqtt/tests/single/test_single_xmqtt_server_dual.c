#define XMQTT_MODULE_XMQTT_SERVER
#define XMQTT_MODULE_XMQTT_CODEC311
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "adding compatibility must leave the client unselected"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static xatomic32 serverResumeFence;
enum { RESUME_ARMED = 1, RESUME_AT_CONNACK, RESUME_RELEASED };

static xnetresult serverControlledSend(xnetstream* Stream, const void* Data, size_t Size)
{
    const unsigned char* Wire = Data;
    uint32 Armed = RESUME_ARMED;
    if (Size >= 4 && Wire[0] == 0x20 && Wire[2] == 1 && Wire[3] == 0 &&
        xrtAtomic32CompareExchange(&serverResumeFence, &Armed, RESUME_AT_CONNACK,
            XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
        double Until = xrtTimer() + 5.0;
        while (xrtAtomic32Load(&serverResumeFence, XMEMORY_ACQUIRE) != RESUME_RELEASED) {
            if (xrtTimer() >= Until) return XNET_RESULT_ERROR;
            xrtSleep(1);
        }
    }
    return xrtNetStreamSend(Stream, Data, Size);
}

#define xrtNetStreamSend serverControlledSend
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtNetStreamSend
#define main serverDualSuite
#include "../test_server.c"
#undef main

static void serverResumeWriteCredit(size_t PayloadBytes)
{
    const xmqttversion Version = XMQTT_VERSION_5;
    serverfixture Fixture;
    serverStartConfigured(&Fixture, true, 0, true, 65536);
    xnetsocket Old = serverDial(&Fixture);
    serverPersistentConnect(Old, Version, "cred", true, 60, 1, false, 0);
    const unsigned char Subscribe[] = {0x82,10,0,1,0,0,4,'f','i','l','l',0};
    unsigned char Reply[6];
    serverSend(Old, Subscribe, sizeof(Subscribe));
    serverRecv(Old, Reply, sizeof(Reply));
    testRequire(!memcmp(Reply, (const unsigned char[]){0x90,4,0,1,0,0}, sizeof(Reply)),
        "subscribe the saved session to the write-credit test");
    xnetsocket Publisher = serverDial(&Fixture);
    serverPersistentConnect(Publisher, Version, "send", true, 0, 1, false, 0);
    const unsigned char Disconnect[] = {0xe0,0};
    serverSend(Old, Disconnect, sizeof(Disconnect));
    serverEof(Old);
    serverConnections(&Fixture, 1);
    serverWaitSessions(&Fixture, 1, 1);

    xnetsocket Successor = serverDial(&Fixture);
    unsigned char Connect[40];
    xrtAtomic32Store(&serverResumeFence, RESUME_ARMED, XMEMORY_RELEASE);
    serverSend(Successor, Connect, serverPersistentWire(Connect, Version, "cred", false, 60, 1));
    double Until = xrtTimer() + 5.0;
    while (xrtAtomic32Load(&serverResumeFence, XMEMORY_ACQUIRE) != RESUME_AT_CONNACK) {
        testRequire(xrtTimer() < Until, "resumed worker reached the controlled CONNACK send");
        xrtSleep(1);
    }
    unsigned char Payload[119], Publish[128];
    memset(Payload, 'a', PayloadBytes);
    size_t Bytes = serverPublishWire(Publish, Version, "fill", Payload, PayloadBytes, NULL, 0);
    testRequire(Bytes == PayloadBytes + 9, "publication uses the planned write credit");
    serverSend(Publisher, Publish, Bytes);
    serverPingPeer(Publisher); /* The other worker processed the publication before CONNACK. */
    xrtAtomic32Store(&serverResumeFence, RESUME_RELEASED, XMEMORY_RELEASE);
    serverConnackState(Successor, Version, 0, true, true, true, 64);
    if (Bytes < sizeof(Publish)) {
        unsigned char Actual[128];
        serverRecv(Successor, Actual, Bytes);
        testRequire(!memcmp(Actual, Publish, Bytes), "queued PUBLISH follows CONNACK");
        serverPingPeer(Successor);
    }
    xrtNetSocketClose(Successor);
    xrtNetSocketClose(Publisher);
    serverStop(&Fixture);
}

int main(void)
{
    xrtAtomic32Init(&serverResumeFence, 0);
    serverResumeWriteCredit(119); /* Occupies the entire 128-byte TCP write allowance. */
    serverResumeWriteCredit(23);
    puts("[PASS] resumed CONNACK precedes best-effort sends under TCP write pressure");
    return serverDualSuite();
}
