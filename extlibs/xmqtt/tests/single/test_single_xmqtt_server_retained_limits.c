#define XMQTT_MODULE_XMQTT_SERVER_RETAINED_COMMIT
#include <xmqtt/features.h>
#if defined(XRT_FEATURE_FILE) || defined(XRT_FEATURE_TASK_POOL) || defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "the retained commit module must only select the broker protocol closure"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test.h"

static xmqttserverretainedticket proposal(xmqttserverpeer* Peer, xbytesview Wire, bool Remove)
{
	xmqttserver* Server = Peer->Server;
	xbytesview Key = {Wire.Data + 4, 1};
	xstrview Publisher = XRT_STR_LITERAL("source");
	xmqttserverpublication* Copy = xmqttServerCopyPublication(Wire, XMQTT_PUBLISH_RETAIN, Publisher);
	testRequire(Copy != NULL, "allocate immutable retained proposal");
	return (xmqttserverretainedticket){
		.Server = Server, .Peer = Peer, .Shard = xmqttServerWildcardShard(Server, Key),
		.Publication = Copy, .Key = Key, .Hash = xmqttServerIdHash(Key, &Server->HashKey),
		.Record = {XMQTT_VERSION_5, Publisher, Wire, 0},
		.Received = xrtTimer(), .Remove = Remove
	};
}

static bool captureRestoredWill(xmqttserverretainedticket* Ticket, xstrview Topic,
	const xmqttserverretainedrecord* Record, bool Remove, double Received, ptr User)
{
	testRequire(!Remove && isfinite(Received) && Topic.Size == 1 && Topic.Data[0] == 'a' &&
		Record->PublisherId.Size == 6 && !memcmp(Record->PublisherId.Data, "source", 6),
		"restored Will enters the backend with its original publisher");
	xrtAtomicPtrStore(User, Ticket, XMEMORY_RELEASE);
	return true;
}

static bool checkRestoredWill(const xmqttserverretainedrecord* Record, ptr User)
{
	static const unsigned char Expected[] = {0x31, 5, 0, 1, 'a', 0, 'w'};
	testRequire(Record->Version == XMQTT_VERSION_5 && Record->Publish.Size == sizeof(Expected) &&
		!memcmp(Record->Publish.Data, Expected, sizeof(Expected)), "committed restored Will is retained");
	*(bool*)User = true;
	return true;
}

static void detachedWill(xmqttserver* Server)
{
	xatomicptr Pending;
	xrtAtomicPtrInit(&Pending, NULL);
	testRequire(xmqttServerSetRetainedCommit(Server, 4096, captureRestoredWill, &Pending),
		"install detached Will backend on fresh broker");
	static const unsigned char Will[] = {0x31, 5, 0, 1, 'a', 0, 'w'};
	const xmqttserversessionrecord Records[] = {
		{.Kind = XMQTT_SERVER_RECORD_SESSION, .Data = XRT_BYTES_INIT("source"),
			.Milliseconds = 20000, .Code = XMQTT_VERSION_5},
		{.Kind = XMQTT_SERVER_RECORD_WILL, .Data = {Will, sizeof(Will)}}
	};
	testRequire(xmqttServerRestoreSessions(Server, Records, 2, 0), "restore a due retained Will");
	double Deadline = xrtTimer() + 5.0;
	xmqttserverretainedticket* Ticket;
	while (!(Ticket = xrtAtomicPtrLoad(&Pending, XMEMORY_ACQUIRE))) {
		testRequire(xrtTimer() < Deadline, "restored Will reaches backend");
		xrtSleep(1);
	}
	testRequire(xmqttServerClose(Server), "close with a detached Will in progress");
	for (;;) {
		xrtMutexLock(&Server->Gate);
		bool TimerRetired = !Server->ExpiryTimer;
		xrtMutexUnlock(&Server->Gate);
		if (TimerRetired) break;
		testRequire(xrtTimer() < Deadline, "expiry worker retires");
		xrtSleep(1);
	}
	testRequire(xmqttServerWaitFor(Server, 0) == XWAIT_TIMEOUT,
		"detached Will pins the broker after the timer retires");
	testRequire(xmqttServerCompleteRetained(Ticket, XMQTT_SERVER_RETAINED_COMMITTED) &&
		xmqttServerWaitFor(Server, 5000) == XWAIT_OK, "close waits for detached Will completion");
	bool Visited = false;
	testRequire(xmqttServerVisitRetained(Server, checkRestoredWill, &Visited) && Visited,
		"committed detached Will is visible in the closed checkpoint");
}

int main(void)
{
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xnetengine* Engine = xrtNetEngineCreate(&EngineConfig);
	xmqttserverconfig Config;
	xmqttserver* Server = NULL;
	testRequire(Engine && xrtNetEngineStart(Engine) && xmqttServerConfigInit(&Config) &&
		xmqttServerCreate(Engine, &Config, &Server), "initialize standalone retained commit fixture");
	xmqttserverpeer Peer = {.Server = Server};
	static const unsigned char First[] = {0x31, 5, 0, 1, 'a', 0, 'x'};
	static const unsigned char Larger[] = {0x31, 7, 0, 1, 'a', 0, 'x', 'y', 'z'};
	xmqttserverretainedticket Initial = proposal(&Peer, (xbytesview){First, sizeof(First)}, false);
	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"install retained admission allocation failpoint");
	testRequire(!xmqttServerPrepareRetainedTicket(&Initial) && xrtAllocationBudgetLeave(&Scope),
		"entry OOM before backend admission");
	testRequire(!Initial.Shard->RetainedPending && !xrtAtomic32Load(&Server->RetainedTopics, XMEMORY_RELAXED) &&
		!xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED), "rollback provisional retained quotas and lease");
	xrtClearError();
	xrtAllocationBudgetFree(Budget);
	testRequire(xmqttServerPrepareRetainedTicket(&Initial), "reserve new retained entry");

	xmqttserverretainedticket Update = proposal(&Peer, (xbytesview){Larger, sizeof(Larger)}, false);
	testRequire(!Initial.Shard->Retained.Count && !xmqttServerPrepareRetainedTicket(&Update),
		"prepared value stays hidden and owns the shard lease");
	xmqttServerResolveRetainedTicket(&Initial, true);
	xmqttServerReleaseRetainedShard(Initial.Shard);
	size_t Previous = sizeof(First) + Initial.Record.PublisherId.Size;
	testRequire(Initial.Shard->Retained.Count == 1 && xmqttServerPrepareRetainedTicket(&Update),
		"admit a replacement after successful apply");
	xmqttServerResolveRetainedTicket(&Update, false);
	xmqttServerReleaseRetainedShard(Update.Shard);
	xmqttserverretained* Visible = xmqttServerFindKey(&Initial.Shard->Retained, Initial.Hash, Initial.Key);
	testRequire(Visible && Visible->Publication == Initial.Publication &&
		xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED) == Previous,
		"failed replacement leaves the old record and exact bytes");
	xmqttserverretainedticket Delete = proposal(&Peer, (xbytesview){First, sizeof(First)}, true);
	testRequire(xmqttServerPrepareRetainedTicket(&Delete), "reserve retained tombstone");
	xmqttServerResolveRetainedTicket(&Delete, true);
	xmqttServerReleaseRetainedShard(Delete.Shard);
	testRequire(!Initial.Shard->Retained.Count && !xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED),
		"committed tombstone releases index and bytes");
	xmqttServerFreePublication(Initial.Publication, NULL, 0);
	xmqttServerFreePublication(Update.Publication, NULL, 0);
	xmqttServerFreePublication(Delete.Publication, NULL, 0);
	detachedWill(Server);
	testRequire(xmqttServerDestroy(Server) && xrtNetEngineStop(Engine) &&
		xrtNetEngineDestroy(Engine), "retire retained commit fixture");
	puts("[PASS] retained proposal visibility, OOM rollback and detached Will retirement");
	return 0;
}
