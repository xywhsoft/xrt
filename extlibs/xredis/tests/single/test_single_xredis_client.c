#define XREDIS_MODULE_XREDIS_CLIENT
#define XRT_MODULE_MEMORY_STATS
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#define main testClientSuite
#include "../test_client.c"
#undef main

enum { TEST_DRAIN_BYTES = 256u * 1024u };

typedef struct testdrain {
	redispeer* Peer;
	xredisclient* Client;
	xatomic32 Step;
} testdrain;

static void testDrainLimitSend(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	testdrain* Test = Data;
	testRequire(xrtNetSocketSet(xrtNetStreamSocket(Test->Client->Stream), XNET_OPTION_SEND_BUFFER, 8192),
		"set small client socket send buffer");
	xrtAtomic32Store(&Test->Step, 1, XMEMORY_RELEASE);
}

static void testDrainPausePeer(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	testdrain* Test = Data;
	xnetstream* Stream = Test->Peer->Stream;
	testRequire(xrtNetSocketSet(xrtNetStreamSocket(Stream), XNET_OPTION_RECEIVE_BUFFER, 65536),
		"set bounded peer socket receive buffer");
	xrtNetStreamPause(Stream);
	xrtAtomic32Store(&Test->Step, 2, XMEMORY_RELEASE);
}

static void testDrainCheckPending(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	testdrain* Test = Data;
	xredisclient* Client = Test->Client;
	testRequire(Client->SentCount == 1 && Client->PendingHead != NULL &&
		xrtNetStreamWritable(Client->Stream) < Client->PendingHead->WireBytes,
		"second request must wait for transport credit");
	xrtAtomic32Store(&Test->Step, 3, XMEMORY_RELEASE);
}

static void testDrainResumePeer(xnetworker* Worker, ptr Data)
{ (void)Worker; testRequire(xrtNetStreamResume(((redispeer*)Data)->Stream), "resume pressured peer"); }

static bool testDrainCommand(redispeer* Peer, xredisreply* Reply, ptr Data)
{
	(void)Data;
	xredisreplyvalue Root, Command, Value;
	testRequire(xredisReplyGet(Reply, 0, &Root) && Root.ChildCount == 2 &&
		xredisReplyGet(Reply, Root.FirstChild, &Command) && xredisReplyGet(Reply, Command.Next, &Value) &&
		Value.Data.Size == TEST_DRAIN_BYTES, "pressured request envelope");
	if (xrtAtomic32Load(&Peer->Commands, XMEMORY_RELAXED) == 0) {
		testRequire(redisPeerName(Command, "FIRST"), "first pressured request");
	} else {
		testRequire(redisPeerName(Command, "SECOND"), "second pressured request");
		redisPeerSend(Peer, ":1\r\n:2\r\n", 8);
	}
	return true;
}

static void testClientDrainReturnsCredit(void)
{
	redispeer Peer; redisPeerStart(&Peer);
	redispeerhook Hook = {testDrainCommand, NULL};
	xrtAtomicPtrStore(&Peer.Hook, &Hook, XMEMORY_RELEASE);
	xredisclientconfig Config; xredisClientConfigInit(&Config);
	Config.Stream.WriteLimit = Config.Stream.WriteHighWater = TEST_DRAIN_BYTES + 64;
	Config.Stream.WriteLowWater = 0;
	Config.MaxCommandBytes = Config.Stream.WriteLimit;
	Config.MaxInFlight = 2;
	xredisclient* Client = testClientConnect(&Peer, &Config);
	xfuture* Ready = xredisClientReady(Client);
	testRequire(Ready && xrtFutureWaitFor(Ready, 5000) == XWAIT_OK, "pressured client readiness");
	const xredisreadyresult* Outcome = xrtFutureValue(Ready);
	testRequire(Outcome && Outcome->Status == XREDIS_READY_OK, "pressured client open");
	xrtFutureDestroy(Ready); redisPeerWait(&Peer.Accepted, 1);

	testdrain Test = {.Peer = &Peer, .Client = Client};
	xrtAtomic32Init(&Test.Step, 0);
	testRequire(xrtNetEnginePost(Peer.Engine, 0, testDrainLimitSend, &Test), "post send limit");
	redisPeerWait(&Test.Step, 1);
	uint32 PeerWorker = xrtNetWorkerIndex(xrtNetStreamWorker(Peer.Stream));
	testRequire(xrtNetEnginePost(Peer.Engine, PeerWorker, testDrainPausePeer, &Test), "post peer pause");
	redisPeerWait(&Test.Step, 2);

	static const unsigned char Payload[TEST_DRAIN_BYTES] = {0};
	xbytesview Args[] = {XRT_BYTES_LITERAL("FIRST"), {Payload, sizeof(Payload)}};
	xfuture* First = NULL; xfuture* Second = NULL;
	testRequire(xredisClientCallAsync(Client, Args, 2, XRT_WAIT_FOREVER, &First) == XREDIS_ADMISSION_ACCEPTED,
		"first pressured admission");
	Args[0] = (xbytesview)XRT_BYTES_LITERAL("SECOND");
	testRequire(xredisClientCallAsync(Client, Args, 2, XRT_WAIT_FOREVER, &Second) == XREDIS_ADMISSION_ACCEPTED,
		"second pressured admission");
	xnetpost Barrier;
	testRequire(xrtNetPostInit(&Barrier) && xrtNetPost(xrtNetEngineWorker(Peer.Engine, 0), &Barrier,
		testDrainCheckPending, &Test), "post transport credit barrier");
	redisPeerWait(&Test.Step, 3);
	testRequire(xrtNetEnginePost(Peer.Engine, PeerWorker, testDrainResumePeer, &Peer), "post peer resume");

	/* The peer withholds both replies until the second command arrives, so
	 * reply credit cannot be the event that releases the pending write. */
	const xredisrequestresult* A = testClientResult(First);
	const xredisrequestresult* B = testClientResult(Second);
	xredisreplyvalue Value;
	testRequire(A->Status == XREDIS_REQUEST_OK && A->Delivery == XREDIS_REPLIED &&
		xredisReplyGet(A->Reply, 0, &Value) && Value.Integer == 1 &&
		B->Status == XREDIS_REQUEST_OK && B->Delivery == XREDIS_REPLIED &&
		xredisReplyGet(B->Reply, 0, &Value) && Value.Integer == 2, "transport drain did not return write credit in order");
	xrtFutureDestroy(First); xrtFutureDestroy(Second); testClientClose(&Peer, Client);
}

int main(void)
{
	testClientDrainReturnsCredit();
	return testClientSuite();
}
