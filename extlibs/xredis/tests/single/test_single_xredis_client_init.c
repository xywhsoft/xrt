#define XREDIS_MODULE_XREDIS_CLIENT
#define XRT_MODULE_MEMORY_STATS
#include <xredis/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XREDIS_IMPLEMENTATION
#include "../../../../single/extlibs/xredis.h"
#define main testClientInitSuite
#include "../test_client_init.c"
#undef main

typedef struct testdeadlines {
	xredisclient* Client;
	xatomic32 Checked;
} testdeadlines;

static void testClientCheckDeadlines(xnetworker* Worker, ptr Data)
{
	testdeadlines* Test = Data;
	xredisclient* Client = Test->Client;
	const xredisrequest* Request = Client->PendingHead;
	testRequire(Client->ReadyPromise != NULL && Client->SentCount == 0 && Request != NULL,
		"inspect both pending timers before the peer completes HELLO");
	const __xrt_net_engine_timer* InitTimer = __xrtNetEngineTimerFind(Worker, Client->InitTimer);
	const __xrt_net_engine_timer* RequestTimer = __xrtNetEngineTimerFind(Worker, Request->Timer);
	testRequire(InitTimer != NULL && RequestTimer != NULL, "both finite deadlines must be armed");
	if (InitTimer->Deadline != Client->InitDeadline || RequestTimer->Deadline != Request->Deadline) {
		fprintf(stderr, "Timer offsets from logical deadlines (us): initialization %.3f, request %.3f\n",
			(InitTimer->Deadline - Client->InitDeadline) * 1e6, (RequestTimer->Deadline - Request->Deadline) * 1e6);
		testRequire(false, "timer deadlines differ from the original monotonic deadlines");
	}
	xrtAtomic32Store(&Test->Checked, 1, XMEMORY_RELEASE);
}

static void testClientAbsoluteDeadlines(void)
{
	const redisinitpeerstep Step = {{INIT_B("HELLO"), INIT_B("3")}, 2, INIT_B(REDIS_INIT_HELLO_REPLY), true, false};
	redisinitpeer Peer;
	xredisclientinitconfig Init;
	xredisClientInitConfigInit(&Init);
	Init.Timeout = 30000;
	redisInitPeerStart(&Peer, &Step, 1);
	xredisclient* Client = initConnect(&Peer, NULL, &Init);
	xfuture* Ready = xredisClientReady(Client);
	redisInitPeerStepWait(&Peer, 1);
	xfuture* Ping = initPing(Client, 30000);

	testdeadlines Test = {.Client = Client};
	xrtAtomic32Init(&Test.Checked, 0);
	xnetpost Barrier;
	testRequire(xrtNetPostInit(&Barrier) && xrtNetPost(xrtNetEngineWorker(Peer.Peer.Engine, 0), &Barrier,
		testClientCheckDeadlines, &Test), "post deadline inspection after request ingestion");
	redisPeerWait(&Test.Checked, 1);

	redisPeerReleaseBlocked(&Peer.Peer);
	initExpectReady(Ready, XREDIS_RESP3, false);
	const xredisrequestresult* Result = initWait(Ping);
	testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED,
		"request completed after the held HELLO");
	xrtFutureDestroy(Ready);
	xrtFutureDestroy(Ping);
	initClose(&Peer, Client);
}

int main(void)
{
	testClientAbsoluteDeadlines();
	return testClientInitSuite();
}
