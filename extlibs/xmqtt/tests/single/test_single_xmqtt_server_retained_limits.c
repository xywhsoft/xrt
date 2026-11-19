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
		.Peer = Peer, .Shard = xmqttServerWildcardShard(Server, Key),
		.Publication = Copy, .Key = Key, .Hash = xmqttServerIdHash(Key, &Server->HashKey),
		.Record = {XMQTT_VERSION_5, Publisher, Wire, 0},
		.Received = xrtTimer(), .Remove = Remove
	};
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
	size_t Previous = sizeof(First) + Initial.Record.PublisherId.Size;
	testRequire(Initial.Shard->Retained.Count == 1 && xmqttServerPrepareRetainedTicket(&Update),
		"admit a replacement after successful apply");
	xmqttServerResolveRetainedTicket(&Update, false);
	xmqttserverretained* Visible = xmqttServerFindKey(&Initial.Shard->Retained, Initial.Hash, Initial.Key);
	testRequire(Visible && Visible->Publication == Initial.Publication &&
		xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED) == Previous,
		"failed replacement leaves the old record and exact bytes");
	xmqttserverretainedticket Delete = proposal(&Peer, (xbytesview){First, sizeof(First)}, true);
	testRequire(xmqttServerPrepareRetainedTicket(&Delete), "reserve retained tombstone");
	xmqttServerResolveRetainedTicket(&Delete, true);
	testRequire(!Initial.Shard->Retained.Count && !xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED),
		"committed tombstone releases index and bytes");
	xmqttServerFreePublication(Initial.Publication, NULL, 0);
	xmqttServerFreePublication(Update.Publication, NULL, 0);
	xmqttServerFreePublication(Delete.Publication, NULL, 0);
	testRequire(xmqttServerClose(Server) && xmqttServerDestroy(Server) && xrtNetEngineStop(Engine) &&
		xrtNetEngineDestroy(Engine), "retire retained commit fixture");
	puts("[PASS] retained proposal visibility, bounded contention, OOM and rollback");
	return 0;
}
