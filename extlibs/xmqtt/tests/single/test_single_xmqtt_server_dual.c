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

static void serverWaitResumeTakeover(xmqttserver* Server)
{
    const xbytesview Id = XRT_BYTES_LITERAL("cred");
    double Until = xrtTimer() + 5.0;
    for (;;) {
        xrtMutexLock(&Server->Gate);
        xmqttserversession** Session = xrtMapGet(&Server->Identities, Id);
        bool Reserved = Session && (*Session)->Successor;
        xrtMutexUnlock(&Server->Gate);
        if (Reserved) return;
        testRequire(xrtTimer() < Until, "takeover reserved the paused resume");
        xrtSleep(1);
    }
}

static void serverResumeWriteCredit(size_t PayloadBytes, bool Takeover)
{
    const xmqttversion Version = XMQTT_VERSION_5;
    serverfixture Fixture;
    serverStartLimits(&Fixture, true, 0, true, 65536, 4096, 16u * 1024u * 1024u, 64, 1, 0, 0);
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
    /* Admit both streams before holding the worker that wrote the CONNACK. */
    xnetsocket Replacement = NULL;
    if (Takeover) {
        serverConnections(&Fixture, 2);
        Replacement = serverDial(&Fixture);
        serverConnections(&Fixture, 3);
    }
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
    if (Bytes < sizeof(Publish)) {
        Publish[Bytes - 1] = 'b';
        serverSend(Publisher, Publish, Bytes);
        Publish[Bytes - 1] = 'a';
    }
    serverPingPeer(Publisher); /* The other worker processed the publication before CONNACK. */
    xmqttserverinfo Info;
    if (Bytes < sizeof(Publish)) {
        testRequire(xmqttServerInfo(Fixture.Server, &Info) && Info.OutgoingPublishes == 1 &&
            Info.OutgoingPublishBytes == Bytes && Info.DroppedDeliveries == 1 && !Info.Forwarded &&
            !Info.OutgoingPublishOverloads, "a full resume queue drops only the extra QoS 0 publication");
    }
    if (Replacement) {
        serverSend(Replacement, Connect, serverPersistentWire(Connect, Version, "cred", false, 60, 1));
        serverWaitResumeTakeover(Fixture.Server);
        size_t Read;
        testRequire(xrtNetSocketRecv(Replacement, Connect, sizeof(Connect), &Read) == XNET_RESULT_AGAIN,
            "replacement CONNACK waits for the paused owner");
    }
    xrtAtomic32Store(&serverResumeFence, RESUME_RELEASED, XMEMORY_RELEASE);
    serverConnackState(Successor, Version, 0, true, true, true, 64);
    if (Replacement) {
        serverTakenOver(Successor);
        Successor = Replacement;
        serverConnackState(Successor, Version, 0, true, true, true, 64);
    }
    if (Bytes < sizeof(Publish)) {
        unsigned char Actual[128];
        if (!Replacement) {
            serverRecv(Successor, Actual, Bytes);
            testRequire(!memcmp(Actual, Publish, Bytes), "queued PUBLISH follows CONNACK");
        }
        serverPingPeer(Successor);
        Publish[Bytes - 1] = 'c';
        serverSend(Publisher, Publish, Bytes);
        serverPingPeer(Publisher);
        serverRecv(Successor, Actual, Bytes);
        testRequire(!memcmp(Actual, Publish, Bytes), "resumed session accepts later QoS 0 after a drop");
        serverPingPeer(Successor);
        unsigned Dropped = Replacement ? 2 : 1, Forwarded = Replacement ? 1 : 2;
        testRequire(xmqttServerInfo(Fixture.Server, &Info) && !Info.OutgoingPublishes &&
            !Info.OutgoingPublishBytes && Info.DroppedDeliveries == Dropped && Info.Forwarded == Forwarded &&
            !Info.OutgoingPublishOverloads, "resume queue retires credit and counts each delivery once");
    }
    xrtNetSocketClose(Successor);
    xrtNetSocketClose(Publisher);
    serverStop(&Fixture);
}

int main(void)
{
    xrtAtomic32Init(&serverResumeFence, 0);
    serverResumeWriteCredit(119, false); /* Occupies the entire 128-byte TCP write allowance. */
    serverResumeWriteCredit(23, false);
    serverResumeWriteCredit(23, true);
    puts("[PASS] resumed CONNACK precedes bounded best-effort sends under TCP write pressure");
    return serverDualSuite();
}
