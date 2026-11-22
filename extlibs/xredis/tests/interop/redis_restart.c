/* One native client process and two successive private Redis server processes. */
#include <xredis.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MEMBERS = 2, BATCH = 16, IO_MS = 3000, PHASE_MS = 15000 };
static const xbytesview Channel = XRT_BYTES_LITERAL("xrt:restart:\0channel");
static const xbytesview Intermediate = XRT_BYTES_LITERAL("xrt:restart:\0intermediate");
static const xbytesview Replacement = XRT_BYTES_LITERAL("xrt:restart:\0replacement");
static const xbytesview Before = XRT_BYTES_LITERAL("before\0restart");
static const xbytesview Key = XRT_BYTES_LITERAL("xrt:restart:\0tracking");
static const xbytesview Prefix = XRT_BYTES_LITERAL("pipeline\0prefix");

static void require(bool Ok, const char* Message)
{
	if (!Ok) {
		fprintf(stderr, "Redis restart: %s\n", Message);
		exit(EXIT_FAILURE);
	}
}

#include "tls_probe.h"

typedef struct restartendpoint {
	xnetaddr Address;
	redisprobetls Tls;
} restartendpoint;

static const void* result(xfuture* Future, int64 Timeout)
{
	require(Future && xrtFutureWaitFor(Future, Timeout) == XWAIT_OK, "Future deadline");
	const void* Value = xrtFutureValue(Future);
	require(Value != NULL, "missing Future value");
	return Value;
}

static xredisreplyvalue root(const xredisrequestresult* Result)
{
	require(Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED, "reply status");
	xredisreplyinfo Info = {.Size = sizeof(Info)};
	xredisreplyvalue Value;
	require(xredisReplyInfo(Result->Reply, &Info) && xredisReplyGet(Result->Reply, Info.Root, &Value), "reply root");
	return Value;
}

static bool same(xbytesview Left, xbytesview Right)
{
	return Left.Size == Right.Size && memcmp(Left.Data, Right.Data, Right.Size) == 0;
}

static void checkSet(xredisreplyvalue Root)
{
	require(Root.Type == XREDIS_REPLY_SIMPLE_STRING && same(Root.Data, (xbytesview)XRT_BYTES_LITERAL("OK")), "SET or TRACKING acknowledgement");
}

static xfuture* call(xredispool* Pool, const xbytesview* Args, size_t Count, int64 Timeout)
{
	xfuture* Future = NULL;
	require(xredisSharedPoolCallAsync(Pool, Args, Count, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "shared command admission");
	return Future;
}

static bool connectMember(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
	(void)Attempt;
	const restartendpoint* Endpoint = Data;
	xredisclientinitconfig Init;
	if (!xredisClientInitConfigInit(&Init)) return false;
	Init.Timeout = Timeout;
	return redisProbeConnect(&Endpoint->Tls, Engine, &Endpoint->Address, Slot, &Init, Out);
}

static bool connectSubscriber(xnetengine* Engine, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
	return connectMember(Engine, 1, Attempt, Timeout, Data, Out);
}

static xredisclient* startTracking(xnetengine* Engine, restartendpoint* Endpoint)
{
	xredisclient* Client = NULL;
	require(connectMember(Engine, 0, 0, IO_MS, Endpoint, &Client), "start tracking connection");
	xfuture* Ready = xredisClientReady(Client);
	const xredisreadyresult* Negotiated = result(Ready, IO_MS);
	require(Negotiated->Status == XREDIS_READY_OK && Negotiated->Protocol == XREDIS_RESP3, "tracking HELLO 3");
	xrtFutureDestroy(Ready);

	xredistrackingconfig Config;
	require(xredisTrackingConfigInit(&Config) && xredisClientTrackingObserve(Client, 0) == XREDIS_ADMISSION_ACCEPTED, "tracking observer");
	Config.Enabled = true;
	xfuture* Future = NULL;
	require(xredisClientTrackingConfigureAsync(Client, &Config, IO_MS, &Future) == XREDIS_ADMISSION_ACCEPTED, "enable tracking");
	const xredistrackingresult* Tracked = result(Future, IO_MS);
	checkSet(root(&Tracked->Request));
	xrtFutureDestroy(Future);
	return Client;
}

static void closedClient(xredisclient* Client, xredisrequeststatus Status)
{
	xfuture* Closed = xredisClientClosed(Client);
	if (Status == XREDIS_REQUEST_CLOSED) require(xredisClientClose(Client), "request client close");
	require(Closed && xrtFutureWaitFor(Closed, IO_MS) == XWAIT_OK, "physical client close");
	xredisclientinfo Info = {.Size = sizeof(Info)};
	require(xredisClientInfo(Client, &Info) && Info.State == XREDIS_CLIENT_CLOSED &&
		Info.CloseStatus == Status && !Info.Requests && !Info.InitBytes &&
		!Info.CommandBytes && !Info.Pushes && !Info.PushBytes && Info.Accepted == Info.Completed, "closed client accounting");
	xrtFutureDestroy(Closed);
	xredisClientRelease(Client);
}

static xredisclient* waitOnline(xredispool* Pool, xredissubscriptionsession* Session, uint64 Generation, uint64 Revision)
{
	double Due = xrtTimer() + PHASE_MS / 1000.0;
	for (;;) {
		xredispoolinfo Slots = {.Size = sizeof(Slots)};
		xredispoolconnectinfo Connect = {.Size = sizeof(Connect)};
		require(xredisPoolInfo(Pool, &Slots) && xredisPoolConnectInfo(Pool, &Connect), "read pool state");
		if (Slots.Idle == MEMBERS) {
			xredissubscriptionsessioncurrent Current = {0};
			require(Connect.Connected <= MEMBERS * Generation, "unexpected pool generation");
			if (Connect.Connected == MEMBERS * Generation &&
				xredisSubscriptionSessionCurrent(Session, &Current) == XREDIS_ADMISSION_ACCEPTED) {
				require(Current.Generation == Generation && Current.Revision == Revision, "subscription generation and revision");
				return Current.Client;
			}
		}
		require(xrtTimer() < Due, "active pool and subscriber deadline");
		xrtSleep(2);
	}
}

typedef struct restartobservations {
	xfuture* Read;
	xredistrackingpush Push;
	xredisreply* Message;
} restartobservations;

static uint64 checkObservedBulk(xfuture* Future, xbytesview Expected)
{
	const xredistrackingresult* Tracked = result(Future, IO_MS);
	xredisreplyvalue Read = root(&Tracked->Request);
	require(Tracked->Sequence && Read.Type == XREDIS_REPLY_BULK_STRING && same(Read.Data, Expected),
		"observed bulk reply content");
	return Tracked->Sequence;
}

static xfuture* pipelineCall(xredisclient* Client, const xbytesview* Args, size_t Count)
{
	xfuture* Future = NULL;
	require(xredisClientTrackingCallAsync(Client, Args, Count, PHASE_MS, &Future) == XREDIS_ADMISSION_ACCEPTED &&
			Future, "admit observed pipeline command");
	return Future;
}

static void checkUnconfirmed(const xredisrequestresult* Request, xredisrequeststatus Status)
{
	require(Request->Status == Status && Request->Delivery == XREDIS_MAY_HAVE_EXECUTED && !Request->Reply,
		"lost reply leaves the request outcome uncertain");
}

static void checkPipelineLoss(xfuture* Future)
{
	const xredistrackingresult* Observed = result(Future, PHASE_MS);
	checkUnconfirmed(&Observed->Request, XREDIS_REQUEST_DISCONNECTED);
	require(!Observed->Sequence, "unconfirmed pipeline command has no reply sequence");
}

static void checkInvalidation(xredistrackingpush Push, uint64 Sequence)
{
	xredistrackingevent Invalidation;
	xredisreplyvalue Item;
	require(Push.Sequence > Sequence && xredisTrackingEventRead(Push.Reply, &Invalidation) == XREDIS_TRACKING_EVENT &&
		Invalidation.Kind == XREDIS_TRACKING_KEYS && Invalidation.Keys == 1 &&
		xredisReplyGet(Push.Reply, Invalidation.FirstKey, &Item) && same(Item.Data, Key), "invalidation identity and ordering");
}

static void checkPublication(xredisreply* Message, xbytesview Topic, xbytesview Value)
{
	xredispubsubevent Event;
	require(xredisPubSubEventRead(Message, &Event) == XREDIS_PUBSUB_EVENT &&
		Event.Kind == XREDIS_PUBSUB_MESSAGE && same(Event.Channel, Topic) && same(Event.Payload, Value),
		"publication identity");
}

static void traffic(xredispool* Pool, xredisclient* Tracking, xredisclient* Subscriber,
	xbytesview Topic, xbytesview Value, restartobservations* Retained)
{
	xbytesview Set[] = {XRT_BYTES_LITERAL("SET"), Key, Value};
	xfuture* Future = call(Pool, Set, 3, IO_MS);
	checkSet(root(result(Future, IO_MS)));
	xrtFutureDestroy(Future);

	xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Key};
	Future = NULL;
	require(xredisClientTrackingCall(Tracking, Get, 2, IO_MS, &Future) == XREDIS_ADMISSION_ACCEPTED, "tracked GET admission");
	uint64 Sequence = checkObservedBulk(Future, Value);
	if (Retained) Retained->Read = Future;
	else xrtFutureDestroy(Future);

	Set[2] = (xbytesview)XRT_BYTES_LITERAL("updated\0value");
	Future = call(Pool, Set, 3, IO_MS);
	checkSet(root(result(Future, IO_MS)));
	xrtFutureDestroy(Future);
	double Due = xrtTimer() + IO_MS / 1000.0;
	xredistrackingpush Push;
	while (!xredisClientTrackingTake(Tracking, &Push)) {
		require(xrtTimer() < Due, "tracking invalidation deadline");
		xrtSleep(1);
	}
	checkInvalidation(Push, Sequence);
	if (Retained) Retained->Push = Push;
	else xredisReplyRelease(Push.Reply);

	xfuture* Notification = NULL;
	require(xredisSubscriberReadable(Subscriber, &Notification) == XREDIS_ADMISSION_ACCEPTED &&
		Notification && !xrtFutureDone(Notification), "empty subscriber queue before PUBLISH");
	xbytesview Publish[] = {XRT_BYTES_LITERAL("PUBLISH"), Topic, Value};
	Future = call(Pool, Publish, 3, IO_MS);
	xredisreplyvalue Count = root(result(Future, IO_MS));
	require(Count.Type == XREDIS_REPLY_INTEGER && Count.Integer == 1, "one live PUBLISH recipient");
	xrtFutureDestroy(Future);
	require(xrtFutureWaitFor(Notification, IO_MS) == XWAIT_OK, "subscription readability deadline");
	xrtFutureDestroy(Notification);
	xredisreply* Message = NULL;
	require(xredisClientTakePush(Subscriber, &Message), "take publication");
	checkPublication(Message, Topic, Value);
	if (Retained) Retained->Message = Message;
	else xredisReplyRelease(Message);
}

static void editSubscriptions(xredissubscriptionsession* Session, uint64* Revision)
{
	uint64 BeforeEdit = *Revision;
	require(xredisSubscriptionSessionApply(Session, XREDIS_PUBSUB_CMD_UNSUBSCRIBE, &Channel, 1, Revision) ==
		XREDIS_ADMISSION_ACCEPTED &&
		xredisSubscriptionSessionApply(Session, XREDIS_PUBSUB_CMD_SUBSCRIBE, &Intermediate, 1, Revision) ==
		XREDIS_ADMISSION_ACCEPTED &&
		xredisSubscriptionSessionApply(Session, XREDIS_PUBSUB_CMD_UNSUBSCRIBE, &Intermediate, 1, Revision) ==
		XREDIS_ADMISSION_ACCEPTED &&
		xredisSubscriptionSessionApply(Session, XREDIS_PUBSUB_CMD_SUBSCRIBE, &Replacement, 1, Revision) ==
		XREDIS_ADMISSION_ACCEPTED && *Revision == BeforeEdit + 4, "edit desired subscriptions during the outage");
}

static void publishAbsent(xredispool* Pool, xbytesview Topic)
{
	xbytesview Args[] = {XRT_BYTES_LITERAL("PUBLISH"), Topic, XRT_BYTES_LITERAL("removed")};
	xfuture* Future = call(Pool, Args, 3, IO_MS);
	xredisreplyvalue Reply = root(result(Future, IO_MS));
	require(Reply.Type == XREDIS_REPLY_INTEGER && Reply.Integer == 0, "retired subscription remains absent");
	xrtFutureDestroy(Future);
}

static void verifyOutage(xredispool* Pool, xredissubscriptionsession* Session, xfuture* Lost,
	const char* Phase, uint64 PoolFailures, uint64 SubscriptionFailures)
{
	xredispoolinfo Slots = {.Size = sizeof(Slots)};
	xredispoolconnectinfo Connect = {.Size = sizeof(Connect)};
	xredissubscriptionsessioninfo Subscription = {.Size = sizeof(Subscription)};
	double Due = xrtTimer() + PHASE_MS / 1000.0;
	for (;;) {
		require(xredisPoolInfo(Pool, &Slots) && xredisPoolConnectInfo(Pool, &Connect) &&
			xredisSubscriptionSessionInfo(Session, &Subscription), "outage counters");
		/* Live falls before physical retirement. Failure counts include the
		 * old connections, so wait for a failed new attempt as well. */
		if (!Slots.Live && Slots.Retired >= MEMBERS && !Slots.Requests && !Slots.CommandBytes &&
			Connect.Failures > PoolFailures && Subscription.Failures > SubscriptionFailures) break;
		require(xrtTimer() < Due, "physical retirements and failed reconnects deadline");
		xrtSleep(2);
	}
	require(Connect.Connected == MEMBERS && Subscription.Generations == 1, "no generation published during outage");
	xbytesview Ping = XRT_BYTES_LITERAL("PING");
	xfuture* Future = Lost;
	require(xredisSharedPoolCallAsync(Pool, &Ping, 1, IO_MS, &Future) == XREDIS_ADMISSION_OVERLOADED &&
		Future == Lost, "outage call rejected without overwriting output");
	xredissubscriptionsessioncurrent Current = {.Generation = 99, .Revision = 99};
	require(xredisSubscriptionSessionCurrent(Session, &Current) == XREDIS_ADMISSION_OVERLOADED &&
		!Current.Client && Current.Generation == 99 && Current.Revision == 99, "retired subscriber unavailable");
	printf("{\"phase\":\"%s\",\"pool_failures\":%" PRIu64 ",\"subscription_failures\":%" PRIu64 "}\n",
		Phase, Connect.Failures, Subscription.Failures);
	fflush(stdout);
}

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void verifyUntrusted(xnetengine* Engine, restartendpoint* Endpoint, xredispool* Pool,
	xredissubscriptionsession* Session, xfuture* Lost)
{
	require(getchar() == '\n', "untrusted replacement barrier");
	xredispoolconnectinfo PoolBefore = {.Size = sizeof(PoolBefore)};
	xredissubscriptionsessioninfo SessionBefore = {.Size = sizeof(SessionBefore)};
	require(xredisPoolConnectInfo(Pool, &PoolBefore) && xredisSubscriptionSessionInfo(Session, &SessionBefore),
		"untrusted reconnect baseline");
	xredisclient* Client = NULL;
	require(connectMember(Engine, 0, 0, IO_MS, Endpoint, &Client), "probe untrusted TLS listener");
	xfuture* Ready = xredisClientReady(Client);
	const xredisreadyresult* State = result(Ready, IO_MS);
	require(State->Status == XREDIS_READY_DISCONNECTED && !State->Hello && State->Cause &&
		xrtErrorFind(State->Cause, "xrt.tls", XTLS_ERROR_VERIFY), "untrusted listener certificate rejection");
	xrtFutureDestroy(Ready);
	closedClient(Client, XREDIS_REQUEST_DISCONNECTED);
	verifyOutage(Pool, Session, Lost, "untrusted_rejected", PoolBefore.Failures, SessionBefore.Failures);
}
#endif

static void verifyBatch(xredispool* Pool)
{
	unsigned char Payload[BATCH][3];
	xfuture* Futures[BATCH];
	for (unsigned Index = 0; Index < BATCH; ++Index) {
		Payload[Index][0] = (unsigned char)Index;
		Payload[Index][1] = 0;
		Payload[Index][2] = (unsigned char)(BATCH - Index);
		xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), {Payload[Index], sizeof(Payload[Index])}};
		Futures[Index] = call(Pool, Args, 2, IO_MS);
	}
	for (unsigned Index = 0; Index < BATCH; ++Index) {
		xredisreplyvalue Reply = root(result(Futures[Index], IO_MS));
		require(Reply.Type == XREDIS_REPLY_BULK_STRING &&
			same(Reply.Data, (xbytesview){Payload[Index], sizeof(Payload[Index])}), "recovered pipeline reply content");
		xrtFutureDestroy(Futures[Index]);
	}
}

static xnetengine* startEngine(void)
{
	xnetengineconfig Config;
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	xnetengine* Engine = xrtNetEngineCreate(&Config);
	require(Engine && xrtNetEngineStart(Engine), "start engine");
	return Engine;
}

static uint16 argumentPort(const char* Text)
{
	char* End;
	unsigned long Port = strtoul(Text, &End, 10);
	return *End || Port == 0 || Port > UINT16_MAX ? 0 : (uint16)Port;
}

static xredisclient* rawClient(xnetengine* Engine, uint16 Port)
{
	xnetaddr Address;
	xredisclient* Client = NULL;
	require(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, Port) &&
		xredisClientConnect(Engine, &Address, 0, NULL, &Client), "open raw fixture connection");
	xfuture* Ready = xredisClientReady(Client);
	const xredisreadyresult* State = result(Ready, IO_MS);
	require(State->Status == XREDIS_READY_OK, "raw fixture connection ready");
	xrtFutureDestroy(Ready);
	return Client;
}

static xfuture* rawCall(xredisclient* Client, const xbytesview* Args, size_t Count)
{
	xfuture* Future = NULL;
	require(xredisClientCallAsync(Client, Args, Count, PHASE_MS, &Future) == XREDIS_ADMISSION_ACCEPTED,
		"admit raw pipeline command");
	return Future;
}

static void checkBulk(xfuture* Future, xbytesview Expected)
{
	xredisreplyvalue Value = root(result(Future, IO_MS));
	require(Value.Type == XREDIS_REPLY_BULK_STRING && same(Value.Data, Expected), "retained bulk reply value");
}

enum { INCREMENT_REPLY_BYTES = sizeof(":1\r\n") - 1 };

static void checkIncrement(xfuture* Future)
{
	xredisreplyvalue Value = root(result(Future, IO_MS));
	require(Value.Type == XREDIS_REPLY_INTEGER && Value.Integer == 1, "retained increment reply value");
}

static int lostReply(uint16 RelayPort, uint16 RedisPort, size_t ReplyBytes)
{
	bool MutationReplied = ReplyBytes == INCREMENT_REPLY_BYTES;
	bool Truncated = ReplyBytes && !MutationReplied;
	xredisrequeststatus Interrupted = Truncated ? XREDIS_REQUEST_PROTOCOL : XREDIS_REQUEST_DISCONNECTED;
	xnetengine* Engine = startEngine();
	xredisclient* Client = rawClient(Engine, RelayPort);
	const xbytesview Applied = XRT_BYTES_LITERAL("xrt:restart:\0applied");
	xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), Prefix};
	xfuture* Replied = rawCall(Client, Echo, 2);
	xbytesview Increment[] = {XRT_BYTES_LITERAL("INCR"), Applied};
	xfuture* Mutation = rawCall(Client, Increment, 2);
	Echo[1] = (xbytesview)XRT_BYTES_LITERAL("pipeline\0tail");
	xfuture* Tail = rawCall(Client, Echo, 2);
	checkBulk(Replied, Prefix);
	if (MutationReplied) checkIncrement(Mutation);
	else require(!xrtFutureDone(Mutation), "unconfirmed increment still pending before cutoff");
	require(!xrtFutureDone(Tail), "suffix still pending before reply cutoff");
	puts("{\"phase\":\"prefix_verified\"}");
	fflush(stdout);
	if (!MutationReplied) checkUnconfirmed(result(Mutation, PHASE_MS), Interrupted);
	checkUnconfirmed(result(Tail, PHASE_MS), Interrupted);
	closedClient(Client, Interrupted);

	Client = rawClient(Engine, RedisPort);
	xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Applied};
	xfuture* Read = rawCall(Client, Get, 2);
	checkBulk(Read, (xbytesview)XRT_BYTES_LITERAL("1"));
	xrtFutureDestroy(Read);
	closedClient(Client, XREDIS_REQUEST_CLOSED);
	require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "retire raw fixture engine");
	checkBulk(Replied, Prefix);
	if (MutationReplied) checkIncrement(Mutation);
	else checkUnconfirmed(result(Mutation, IO_MS), Interrupted);
	checkUnconfirmed(result(Tail, IO_MS), Interrupted);
	xrtFutureDestroy(Replied);
	xrtFutureDestroy(Mutation);
	xrtFutureDestroy(Tail);
	printf("{\"phase\":\"complete\",\"retained_replied\":%d,\"retained_uncertain\":%d,\"status\":\"%s\","
		"\"delivery\":\"MAY_HAVE_EXECUTED\",\"read_after_reconnect\":1}\n",
		1 + MutationReplied, 2 - MutationReplied, Truncated ? "PROTOCOL" : "DISCONNECTED");
	return 0;
}

int main(int Argc, char** Argv)
{
	if (Argc > 1 && strcmp(Argv[1], "--lost-reply") == 0) {
		if (Argc != 4 && Argc != 5) return 2;
		const char* Cut = Argc == 5 ? Argv[4] : "3";
		if (Cut[0] < '0' || Cut[0] > '0' + INCREMENT_REPLY_BYTES || Cut[1]) return 2;
		uint16 Relay = argumentPort(Argv[2]), Redis = argumentPort(Argv[3]);
		return Relay && Redis && Relay != Redis ? lostReply(Relay, Redis, (size_t)(Cut[0] - '0')) : 2;
	}
	bool Edit = Argc > 2 && strcmp(Argv[Argc - 1], "--edit-subscriptions") == 0;
	if (Edit) --Argc;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Argc != 2 && Argc != 3) return 2;
#else
	if (Argc != 2) return 2;
#endif
	uint16 Port = argumentPort(Argv[1]);
	if (!Port) return 2;
	restartendpoint Endpoint = {0};
	require(xrtNetAddrLoopback(&Endpoint.Address, XNET_FAMILY_IPV4, Port), "loopback address");
	redisProbeTlsInit(&Endpoint.Tls, Argc == 3 ? Argv[2] : NULL);
	xnetengine* Engine = startEngine();

	xredispoolconnectconfig PoolConfig;
	require(xredisPoolConnectConfigInit(&PoolConfig), "automatic pool defaults");
	PoolConfig.Connect = connectMember;
	PoolConfig.Data = &Endpoint;
	PoolConfig.AttemptTimeout = 500;
	PoolConfig.RetryMin = 30;
	PoolConfig.RetryMax = 100;
	xredispool* Pool = NULL;
	require(xredisSharedPoolConnect(Engine, 0, MEMBERS, NULL, &PoolConfig, &Pool) == XREDIS_ADMISSION_ACCEPTED, "start pool");

	xredissubscriptions* Store = NULL;
	uint64 Revision = 0;
	require(xredisSubscriptionsCreate(NULL, &Store) &&
		xredisSubscriptionsApply(Store, XREDIS_PUBSUB_CMD_SUBSCRIBE, &Channel, 1, &Revision) == XREDIS_ADMISSION_ACCEPTED &&
		Revision != 0, "record subscription intent");
	xredissubscriptionsessionconfig SessionConfig;
	require(xredisSubscriptionSessionConfigInit(&SessionConfig), "subscription defaults");
	SessionConfig.Connect = connectSubscriber;
	SessionConfig.Data = &Endpoint;
	SessionConfig.AttemptTimeout = 500;
	SessionConfig.RetryMin = 30;
	SessionConfig.RetryMax = 100;
	xredissubscriptionsession* Session = NULL;
	require(xredisSubscriptionSessionCreate(Engine, 1, Store, &SessionConfig, &Session) == XREDIS_ADMISSION_ACCEPTED, "start subscription session");

	xredisclient* Subscriber = waitOnline(Pool, Session, 1, Revision);
	xredisclient* Tracking = startTracking(Engine, &Endpoint);
	restartobservations Observed = {0};
	traffic(Pool, Tracking, Subscriber, Channel, Before, Edit ? &Observed : NULL);
	xbytesview Block[] = {XRT_BYTES_LITERAL("BLPOP"), XRT_BYTES_LITERAL("xrt:restart:\0blocked"), XRT_BYTES_LITERAL("0")};
	xfuture* Pending = call(Pool, Block, 3, PHASE_MS);
	xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), Prefix};
	xbytesview Increment[] = {XRT_BYTES_LITERAL("INCR"), XRT_BYTES_LITERAL("xrt:restart:\0pipeline-write")};
	xfuture* Replied = pipelineCall(Tracking, Echo, 2);
	xfuture* Blocked = pipelineCall(Tracking, Block, 3);
	xfuture* Tail = pipelineCall(Tracking, Increment, 2);
	uint64 PrefixSequence = checkObservedBulk(Replied, Prefix);
	require(!xrtFutureDone(Blocked) && !xrtFutureDone(Tail), "pipeline suffix still awaiting replies");
	printf("{\"phase\":\"blocked_submitted\",\"subscription_revision\":%" PRIu64
		   ",\"pipeline_prefix_sequence\":%" PRIu64 "}\n", Revision, PrefixSequence);
	fflush(stdout);
	const xredisrequestresult* Lost = result(Pending, PHASE_MS);
	require(Lost->Status == XREDIS_REQUEST_DISCONNECTED && Lost->Delivery == XREDIS_MAY_HAVE_EXECUTED && !Lost->Reply, "blocked request loss");
	checkPipelineLoss(Blocked);
	checkPipelineLoss(Tail);
	closedClient(Tracking, XREDIS_REQUEST_DISCONNECTED);
	closedClient(Subscriber, XREDIS_REQUEST_DISCONNECTED);
	double Outage = xrtTimer();
	if (Edit) editSubscriptions(Session, &Revision);
	verifyOutage(Pool, Session, Pending, "outage_ready", MEMBERS, 1);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Argc == 3) verifyUntrusted(Engine, &Endpoint, Pool, Session, Pending);
#endif

	Subscriber = waitOnline(Pool, Session, 2, Revision);
	Tracking = startTracking(Engine, &Endpoint);
	xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), Key};
	xfuture* Future = call(Pool, Get, 2, IO_MS);
	require(root(result(Future, IO_MS)).Type == XREDIS_REPLY_NULL, "fresh server has no prior key");
	xrtFutureDestroy(Future);
	if (Edit) {
		publishAbsent(Pool, Channel);
		publishAbsent(Pool, Intermediate);
	}
	traffic(Pool, Tracking, Subscriber, Edit ? Replacement : Channel,
		(xbytesview)XRT_BYTES_LITERAL("after\0restart"), NULL);
	verifyBatch(Pool);
	double Recovered = xrtTimer();
	if (Edit) {
		printf("{\"phase\":\"edited_recovered\",\"subscription_revision\":%" PRIu64 "}\n", Revision);
		fflush(stdout);
		require(getchar() == '\n', "independent subscription ledger barrier");
	}
	require(Lost->Status == XREDIS_REQUEST_DISCONNECTED && Lost->Delivery == XREDIS_MAY_HAVE_EXECUTED && !Lost->Reply,
		"retained failure unchanged after recovery");
	xrtFutureDestroy(Pending);

	xredissubscriberinfo Deliveries = {.Size = sizeof(Deliveries)};
	require(xredisSubscriberInfo(Subscriber, &Deliveries) && Deliveries.Messages == 1, "one restored subscription delivery");
	xredisClientRelease(Subscriber);
	closedClient(Tracking, XREDIS_REQUEST_CLOSED);
	xfuture* Closed = xredisSubscriptionSessionClosed(Session);
	require(xredisSubscriptionSessionClose(Session), "request session close");
	const xredissubscriptionsessionclosed* SessionEnd = result(Closed, IO_MS);
	uint64 SubscriptionGenerations = SessionEnd->Generations;
	require(SessionEnd->Status == XREDIS_REQUEST_OK && SubscriptionGenerations == 2, "closed subscription accounting");
	xrtFutureDestroy(Closed);
	xredisSubscriptionSessionRelease(Session);
	xredisSubscriptionsRelease(Store);

	Closed = xredisPoolClosed(Pool);
	xredispoolinfo Slots = {.Size = sizeof(Slots)};
	require(Closed && xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, IO_MS) == XWAIT_OK &&
		xredisPoolInfo(Pool, &Slots) && Slots.State == XREDIS_POOL_CLOSED && !Slots.Live &&
		!Slots.Requests && !Slots.CommandBytes, "closed pool accounting");
	xrtFutureDestroy(Closed);
	xredispoolconnectinfo Connect = {.Size = sizeof(Connect)};
	require(xredisPoolConnectInfo(Pool, &Connect) && Connect.Connected == 2 * MEMBERS, "final pool generations");
	xredisPoolRelease(Pool);
	require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine), "retire engine workers");
	if (Edit) {
		checkInvalidation(Observed.Push, checkObservedBulk(Observed.Read, Before));
		checkPublication(Observed.Message, Channel, Before);
		xrtFutureDestroy(Observed.Read);
		xredisReplyRelease(Observed.Push.Reply);
		xredisReplyRelease(Observed.Message);
	}
	require(checkObservedBulk(Replied, Prefix) == PrefixSequence, "retained pipeline prefix after retirement");
	checkPipelineLoss(Blocked);
	checkPipelineLoss(Tail);
	xrtFutureDestroy(Replied);
	xrtFutureDestroy(Blocked);
	xrtFutureDestroy(Tail);
	redisProbeTlsClose(&Endpoint.Tls);
	printf("{\"phase\":\"complete\",\"pool_generations\":%" PRIu64 ",\"subscription_generation\":%" PRIu64
		",\"subscription_revision\":%" PRIu64 ",\"retained_observations\":%s,\"pipeline_prefix_sequence\":%" PRIu64
		",\"unconfirmed_pipeline_commands\":2,\"recovery_ms\":%.3f}\n",
		Connect.Connected, SubscriptionGenerations, Revision, Edit ? "true" : "false", PrefixSequence,
		(Recovered - Outage) * 1000.0);
	return 0;
}
