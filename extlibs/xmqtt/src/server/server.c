#include <xmqtt/server.h>
#include <xmqtt/codec311.h>
#include <xrt/atomic.h>
#include <xrt/avl.h>
#include <xrt/hash.h>
#include <xrt/map.h>
#include <xrt/memory.h>
#include <xrt/random.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER)

#define XMQTT_SERVER_TOPIC_SHARDS 16u
#define XMQTT_SERVER_ROOT_WILDCARDS XMQTT_SERVER_TOPIC_SHARDS

typedef struct xmqttserverpeer xmqttserverpeer;
typedef struct xmqttserversession xmqttserversession;
typedef struct xmqttserverlink xmqttserverlink;
typedef struct xmqttserverwildnode xmqttserverwildnode;
typedef struct xmqttserverpublication xmqttserverpublication;
typedef struct xmqttserverdelivery xmqttserverdelivery;
typedef struct xmqttserverkey {
	xavlnode Node;
	uint64 Hash;
	xbytesview Text;
} xmqttserverkey;
typedef struct xmqttserverretained {
	xmqttserverkey Key;
	xavlnode ExpiryNode;
	xmqttserverpublication* Publication;
	size_t PublisherIdBytes;
	double Received, Expires;
	xmqttversion Version;
} xmqttserverretained;
typedef struct xmqttservershare {
	xbytesview Name;
	struct xmqttservershare *Previous, *Next;
	xmqttserverlink* Head;
	xatomicptr Cursor; /* Structural changes take the topic shard's write lock. */
} xmqttservershare;
typedef struct xmqttservertopic {
	xmqttserverkey Key;
	xmqttserverwildnode* Wildcard;
	xmqttservershare* Shares;
	xmqttserverlink* Head;
	size_t Count;
} xmqttservertopic;
struct xmqttserverwildnode {
	xavlnode Sibling;
	xavl Children;
	xmqttserverwildnode* Parent;
	xmqttservertopic* Topic;
	xbytesview Level;
};
typedef struct xmqttservershard {
	xrwlock Gate, RetainedGate;
	xavl Topics, Retained, RetainedExpiry;
	xmqttserverwildnode WildcardRoot;
	xatomic32 WildcardFilters;
} xmqttservershard;
struct xmqttserverlink {
	xmqttserversession* Session;
	xmqttservertopic* Topic;
	xmqttservershard* Shard;
	xmqttservershare* Share;
	xmqttserverlink *Previous, *Next, *SessionNext;
	uint8 Options;
};

typedef enum xmqttserverackphase {
	XMQTT_SERVER_ACK_TERMINAL = 0,
	XMQTT_SERVER_ACK_AWAIT_PUBREL,
	XMQTT_SERVER_ACK_PUBCOMP
} xmqttserverackphase;

typedef struct xmqttserverack {
	xavlnode Node;
	xmqttserversession* Session;
	uint16 Identifier;
	uint8 Phase, Reason;
	unsigned char Wire[5];
} xmqttserverack;

struct xmqttserver {
	xnetengine* Engine;
	xmqttserverconfig Config;
	xmutex Gate;
	xcond Drained;
	xsipkey HashKey;
	xmap Identities;
	xmqttservershard Shards[XMQTT_SERVER_TOPIC_SHARDS + 1u];
	size_t ShardCount;
	xatomic32 Subscriptions, WildcardNodes, WildcardFilters, RetainedTopics, RetainedExpiring, IncomingPublishes;
	xatomic32 OutgoingPublishes;
	xatomic64 Forwarded, DroppedDeliveries, RejectedPublications, RetainedBytes, DroppedRetained;
	xatomic64 OutgoingPublishBytes, OutgoingPublishOverloads;
	xmqttserverpeer* Peers;
	xmqttserverinfo Info;
};

/* The protocol state has an identity and lifetime distinct from its stream.
 * A topic shard pins the session through every published subscription link. */
struct xmqttserversession {
	xmqttserver* Server;
	xmqttserverpeer* Peer;
	xmqttversion Version;
	xbytesview RegisteredKey; /* Borrows the registry key under Server->Gate. */
	xstrview ClientName;
	uint64 Identity;
	xatomic32 Unavailable;
	xmqttserverlink* Links;
	size_t LinkCount;
	xavl Incoming, Outgoing;
	xmqttserverack InlineAck;
	xmqttserverdelivery *Waiting, **WaitingTail;
	xatomic32 OutgoingCount;
	uint16 LastIdentifier;
};

struct xmqttserverpeer {
	xatomic32 References;
	xmqttserver* Server;
	xnetstream* Stream;
	xnetpost Retirement, OutgoingOverload;
	xmqttserverpeer *Previous, *Next;
	xmqttserversession* Session;
	xatomic32 OutgoingStopped;
	uint16 SendMaximum;
	bool Pumping;
	size_t PacketLimit;
	uint32 Worker;
	xmqttversion Version;
	bool Online, Closing;
	unsigned KeepAliveMs;
	uint64 Timer;
	double Deadline, Scheduled;
};

static bool xmqttServerError(xerrkind Kind, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.mqtt.server", 0, Message);
	return false;
}

static uint64 xmqttServerIdHash(xbytesview Key, ptr Data)
{
	return xrtSipHash(Key.Data, Key.Size, *(const xsipkey*)Data);
}

static bool xmqttServerIdEqual(xbytesview Left, xbytesview Right, ptr Data)
{
	(void)Data;
	return Left.Size == Right.Size && memcmp(Left.Data, Right.Data, Left.Size) == 0;
}

static bool xmqttServerReserve(xatomic32* Counter, size_t Limit)
{
	uint32 Count = xrtAtomic32Load(Counter, XMEMORY_RELAXED);
	while (Count < Limit) {
		if (xrtAtomic32CompareExchange(Counter, &Count, Count + 1, XMEMORY_RELAXED, XMEMORY_RELAXED))
			return true;
	}
	return false;
}

/* A nonnull empty tail represents a final empty level. */
static bool xmqttServerReserveBytes(xatomic64* Counter, size_t Limit, size_t Bytes)
{
	uint64 Used = xrtAtomic64Load(Counter, XMEMORY_RELAXED);
	while (Bytes <= Limit - Used) {
		if (xrtAtomic64CompareExchange(Counter, &Used, Used + Bytes,
			XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
	return false;
}

static xbytesview xmqttServerLevel(xbytesview* Tail)
{
	xbytesview Level = *Tail;
	const unsigned char* Slash = memchr(Tail->Data, '/', Tail->Size);
	if (Slash) {
		Level.Size = (size_t)(Slash - Tail->Data);
		*Tail = (xbytesview){Slash + 1, Tail->Size - Level.Size - 1};
	} else {
		*Tail = (xbytesview){0};
	}
	return Level;
}

static int xmqttServerCompareBytes(xbytesview Left, xbytesview Right)
{
	size_t Common = Left.Size < Right.Size ? Left.Size : Right.Size;
	int Order = Common ? memcmp(Left.Data, Right.Data, Common) : 0;
	return Order ? Order : (Left.Size > Right.Size) - (Left.Size < Right.Size);
}

static int xmqttServerCompareLevel(const void* Data, const xavlnode* Sibling, ptr User)
{
	(void)User;
	const xmqttserverwildnode* Node = (const xmqttserverwildnode*)Sibling;
	return xmqttServerCompareBytes(*(const xbytesview*)Data, Node->Level);
}

/* Probe the table under shared locks. Both this comparator and AVL lookup
 * leave the index untouched, including during concurrent comparisons. */
static int xmqttServerCompareKey(const void* Data, const xavlnode* Sibling, ptr User)
{
	(void)User;
	const xmqttserverkey* Left = Data;
	const xmqttserverkey* Right = (const xmqttserverkey*)Sibling;
	if (Left->Hash != Right->Hash) return Left->Hash < Right->Hash ? -1 : 1;
	return xmqttServerCompareBytes(Left->Text, Right->Text);
}

static void xmqttServerInsertKey(xavl* Index, xmqttserverkey* Entry, uint64 Hash,
	xbytesview Key, unsigned char* Text)
{
	memcpy(Text, Key.Data, Key.Size);
	Entry->Hash = Hash;
	Entry->Text = (xbytesview){Text, Key.Size};
	xrtAVLNodeInit(&Entry->Node);
	if (xrtAVLInsert(Index, &Entry->Node, Entry, xmqttServerCompareKey, NULL, NULL) != &Entry->Node) abort();
}

static ptr xmqttServerFindKey(const xavl* Index, uint64 Hash, xbytesview Key)
{
	xmqttserverkey Probe = {.Hash = Hash, .Text = Key};
	return xrtAVLFind(Index, &Probe, xmqttServerCompareKey, NULL);
}

static xmqttserverwildnode* xmqttServerFindLevel(const xmqttserverwildnode* Node, xbytesview Level)
{
	if (!Node->Children.Root) return NULL;
	return (xmqttserverwildnode*)xrtAVLFind(&Node->Children, &Level, xmqttServerCompareLevel, NULL);
}

/* Unhook only unused, nonroot suffix nodes. An existing filter or sibling
 * branch retains its prefix, including after a partially failed insertion. */
static void xmqttServerPruneWildcard(xmqttserver* Server, xmqttserverwildnode* Node)
{
	while (Node->Parent && !Node->Topic && !Node->Children.Root) {
		xmqttserverwildnode* Parent = Node->Parent;
		if (xrtAVLRemove(&Parent->Children, &Node->Level, xmqttServerCompareLevel, NULL) != &Node->Sibling)
			abort();
		xrtFree(Node);
		xrtAtomic32FetchSub(&Server->WildcardNodes, 1, XMEMORY_RELAXED);
		Node = Parent;
	}
}

static uint8 xmqttServerAddWildcard(xmqttserver* Server, xmqttservershard* Shard, xmqttservertopic* Topic)
{
	xmqttserverwildnode* Node = &Shard->WildcardRoot;
	xbytesview Tail = Topic->Key.Text;
	unsigned Depth = 0;
	uint8 Reason = 0x97;
	while (Tail.Data) {
		if (++Depth > XMQTT_SERVER_MAX_WILDCARD_LEVELS) goto Failed;
		xbytesview Level = xmqttServerLevel(&Tail);
		xmqttserverwildnode* Child = xmqttServerFindLevel(Node, Level);
		if (!Child) {
			if (!xmqttServerReserve(&Server->WildcardNodes, Server->Config.MaxWildcardNodes))
				goto Failed;
			Child = xrtMalloc(sizeof(*Child) + Level.Size);
			if (!Child) {
				xrtAtomic32FetchSub(&Server->WildcardNodes, 1, XMEMORY_RELAXED);
				Reason = 0x83;
				goto Failed;
			}
			*Child = (xmqttserverwildnode){.Parent = Node};
			unsigned char* Text = (unsigned char*)(Child + 1);
			if (Level.Size) memcpy(Text, Level.Data, Level.Size);
			Child->Level = (xbytesview){Text, Level.Size};
			xrtAVLNodeInit(&Child->Sibling);
			xrtAVLInit(&Child->Children);
			if (xrtAVLInsert(&Node->Children, &Child->Sibling, &Level, xmqttServerCompareLevel, NULL, NULL) !=
				&Child->Sibling) abort();
		}
		Node = Child;
	}
	Node->Topic = Topic;
	Topic->Wildcard = Node;
	xrtAtomic32FetchAdd(&Shard->WildcardFilters, 1, XMEMORY_RELEASE);
	xrtAtomic32FetchAdd(&Server->WildcardFilters, 1, XMEMORY_RELEASE);
	return 0;

Failed:
	xmqttServerPruneWildcard(Server, Node);
	return Reason;
}

static void xmqttServerRemoveTopic(xmqttserver* Server, xmqttservershard* Shard, xmqttservertopic* Topic)
{
	if (Topic->Wildcard) {
		xmqttserverwildnode* Node = Topic->Wildcard;
		Node->Topic = NULL;
		xmqttServerPruneWildcard(Server, Node);
		xrtAtomic32FetchSub(&Shard->WildcardFilters, 1, XMEMORY_RELEASE);
		xrtAtomic32FetchSub(&Server->WildcardFilters, 1, XMEMORY_RELEASE);
	}
	if (xrtAVLRemove(&Shard->Topics, &Topic->Key, xmqttServerCompareKey, NULL) != &Topic->Key.Node) abort();
	xrtFree(Topic);
}

static void xmqttServerUnlinkTopic(xmqttserverlink* Link)
{
	xmqttserversession* Session = Link->Session;
	xmqttserver* Server = Session->Server;
	xmqttservershard* Shard = Link->Shard;
	xmqttservertopic* Topic = Link->Topic;
	xmqttservershare* Share = Link->Share;
	xmqttserverlink** Head = Share ? &Share->Head : &Topic->Head;
	xrtRWLockWrite(&Shard->Gate);
	if (Link->Previous) Link->Previous->Next = Link->Next;
	else *Head = Link->Next;
	if (Link->Next) Link->Next->Previous = Link->Previous;
	if (Share) {
		if (!Share->Head) {
			if (Share->Previous) Share->Previous->Next = Share->Next;
			else Topic->Shares = Share->Next;
			if (Share->Next) Share->Next->Previous = Share->Previous;
			xrtFree(Share);
		} else if (xrtAtomicPtrLoad(&Share->Cursor, XMEMORY_RELAXED) == Link) {
			xrtAtomicPtrStore(&Share->Cursor, Link->Next ? Link->Next : Share->Head, XMEMORY_RELAXED);
		}
	}
	if (!--Topic->Count) xmqttServerRemoveTopic(Server, Shard, Topic);
	xrtRWLockWriteUnlock(&Shard->Gate);
	--Session->LinkCount;
	xrtFree(Link);
	xrtAtomic32FetchSub(&Server->Subscriptions, 1, XMEMORY_RELAXED);
}

static void xmqttServerPeerRetire(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	xmqttserver* Server = Peer->Server;
	xmqttserversession* Session = Peer->Session;
	if (Session) {
		while (Session->Links) {
			xmqttserverlink* Link = Session->Links;
			Session->Links = Link->SessionNext;
			xmqttServerUnlinkTopic(Link);
		}
		if (Session->Incoming.Count || Session->Outgoing.Count || Session->Waiting ||
			xrtAtomic32Load(&Session->OutgoingCount, XMEMORY_RELAXED)) abort();
	}
	xrtMutexLock(&Server->Gate);
	if (Session && Session->RegisteredKey.Size) {
		xrtMapRemove(&Server->Identities, Session->RegisteredKey);
		--Server->Info.Established;
	}
	xrtFree(Session);
	if (Peer->Previous) Peer->Previous->Next = Peer->Next;
	else Server->Peers = Peer->Next;
	if (Peer->Next) Peer->Next->Previous = Peer->Previous;
	xrtNetStreamDestroy(Peer->Stream);
	xrtFree(Peer);
	if (--Server->Info.Connections == 0 && Server->Info.Closing) xrtCondBroadcast(&Server->Drained);
	xrtMutexUnlock(&Server->Gate);
}

static void xmqttServerPeerDrop(const void* Data)
{
	xmqttserverpeer* Peer = (xmqttserverpeer*)Data;
	if (xrtAtomic32FetchSub(&Peer->References, 1, XMEMORY_ACQ_REL) != 1) return;
	/* Native TCP releases its runtime pin after returning from the owned
	 * event drop. Publish the free connection slot on the next worker turn. */
	if (!xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Peer->Retirement, xmqttServerPeerRetire, Peer))
		abort();
}

/* A route shard keeps the link and memory alive during this try-acquire, even
 * if native TCP has released its last active reference and posted retirement. */
static bool xmqttServerPeerTryRef(xmqttserverpeer* Peer)
{
	uint32 Refs = xrtAtomic32Load(&Peer->References, XMEMORY_RELAXED);
	while (Refs && Refs != UINT32_MAX) {
		if (xrtAtomic32CompareExchange(&Peer->References, &Refs, Refs + 1,
			XMEMORY_ACQUIRE, XMEMORY_RELAXED)) return true;
	}
	return false;
}

static bool xmqttServerPeerCount(const void* Data, size_t* Count)
{
	const xmqttserverpeer* Peer = Data;
	*Count = xrtAtomic32Load(&Peer->References, XMEMORY_ACQUIRE);
	return true;
}

static bool xmqttServerPeerTrace(const void* Data, xrtownershipvisitor Visit, ptr Context)
{
	(void)Data;
	(void)Visit;
	(void)Context;
	return false;
}

static const xrtownershipops xmqttServerPeerOps = {xmqttServerPeerCount, xmqttServerPeerTrace};

static void xmqttServerTimeout(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);

static bool xmqttServerArm(xmqttserverpeer* Peer, double Now)
{
	double Remaining = Peer->Deadline - Now;
	int64 Delay = Remaining > 0 ? (int64)ceil(Remaining * 1000.0) : 0;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	Peer->Timer = xrtNetEngineAfter(Peer->Server->Engine, Peer->Worker, Delay, xmqttServerTimeout, Peer);
	if (!Peer->Timer) {
		xrtAtomic32FetchSub(&Peer->References, 1, XMEMORY_RELAXED);
		return false;
	}
	Peer->Scheduled = Now + (double)Delay * 0.001;
	return true;
}

static void xmqttServerCancelTimer(xmqttserverpeer* Peer)
{
	if (!Peer->Timer) return;
	uint64 Id = Peer->Timer;
	Peer->Timer = 0;
	xrtNetEngineTimerCancelCurrent(Peer->Server->Engine, Id);
}

static void xmqttServerAbortPeer(xmqttserverpeer* Peer)
{
	Peer->Closing = true;
	xrtNetStreamAbort(Peer->Stream);
}

static void xmqttServerTimeout(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	if (Peer->Timer == Id) {
		Peer->Timer = 0;
		double Now = xrtTimer();
		if (Result != XNET_RESULT_OK || Now >= Peer->Deadline || !isfinite(Now)) {
			if (Result == XNET_RESULT_OK) {
				xmqttserver* Server = Peer->Server;
				xrtMutexLock(&Server->Gate);
				++Server->Info.TimedOut;
				xrtMutexUnlock(&Server->Gate);
			}
			xmqttServerAbortPeer(Peer);
		} else if (!xmqttServerArm(Peer, Now)) {
			xmqttServerAbortPeer(Peer);
		}
	}
	xmqttServerPeerDrop(Peer);
}

static bool xmqttServerDeadline(xmqttserverpeer* Peer, double Now, unsigned Milliseconds)
{
	if (!isfinite(Now)) return false;
	Peer->Deadline = Now + (double)Milliseconds * 0.001;
	if (!Milliseconds) {
		xmqttServerCancelTimer(Peer);
		return true;
	}
	/* A busy connection only advances a timestamp. The queued timer can
	 * observe the new deadline and move itself if it fires early. */
	if (Peer->Timer && Peer->Scheduled <= Peer->Deadline) return true;
	xmqttServerCancelTimer(Peer);
	return xmqttServerArm(Peer, Now);
}

static void xmqttServerFinishPeer(xmqttserverpeer* Peer, double Now)
{
	Peer->Closing = true;
	if (!xmqttServerDeadline(Peer, Now, (unsigned)Peer->Server->Config.ConnectTimeout))
		xrtNetStreamAbort(Peer->Stream);
	else
		xrtNetStreamClose(Peer->Stream);
}

static bool xmqttServerWriteConnack(xmqttserverpeer* Peer, uint8 Reason5, uint8 Reason311, size_t PacketLimit)
{
	unsigned char Wire[48];
	size_t Bytes;
	xmqttpacket Reply;
	xmqttPacketInit(XMQTT_PACKET_CONNACK, &Reply);
	if (Peer->Version == XMQTT_VERSION_5) {
		xmqttproperty Limits[] = {
			{XMQTT_PROPERTY_RECEIVE_MAXIMUM, XMQTT_PROPERTY_UINT16, {.Number = Peer->Server->Config.ReceiveMaximum}},
			{XMQTT_PROPERTY_RETAIN_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = Peer->Server->Config.MaxRetainedTopics != 0}},
			{XMQTT_PROPERTY_WILDCARD_AVAILABLE, XMQTT_PROPERTY_BYTE,
				{.Number = Peer->Server->Config.MaxWildcardNodes != 0}},
			{XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
			{XMQTT_PROPERTY_SHARED_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 1}},
			{XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE, XMQTT_PROPERTY_UINT32,
				{.Number = (uint32)Peer->Server->Config.Codec.MaxPacketBytes}},
		};
		Reply.Reason = Reason5;
		if (!Reason5) Reply.Properties = (xmqttpropertylist){Limits, sizeof(Limits)/sizeof(Limits[0])};
		if (!xmqttPacketWrite5(Wire, sizeof(Wire), &Reply, XMQTT_SERVER_TO_CLIENT, NULL, &Bytes))
			return false;
	} else {
		/* MQTT 3.1.1 CONNACK has a fixed four-byte wire form. The refusal
		 * can be written even when the 3.1.1 decoder is deselected. */
		Wire[0] = 0x20;
		Wire[1] = 0x02;
		Wire[2] = 0;
		Wire[3] = Reason311;
		Bytes = 4;
	}
	return Bytes <= PacketLimit && xrtNetStreamSend(Peer->Stream, Wire, Bytes) == XNET_RESULT_OK;
}

static void xmqttServerRefuse(xmqttserverpeer* Peer, uint8 Reason5, uint8 Reason311,
	size_t PacketLimit, double Now)
{
	xmqttserver* Server = Peer->Server;
	xrtMutexLock(&Server->Gate);
	++Server->Info.Refused;
	xrtMutexUnlock(&Server->Gate);
	if (xmqttServerWriteConnack(Peer, Reason5, Reason311, PacketLimit))
		xmqttServerFinishPeer(Peer, Now);
	else
		xmqttServerAbortPeer(Peer);
}

static void xmqttServerSessionInit(xmqttserversession* Session, xmqttserverpeer* Peer, xstrview Name)
{
	Session->Server = Peer->Server;
	Session->Peer = Peer;
	Session->Version = Peer->Version;
	Session->ClientName = Name;
	xrtAVLInit(&Session->Incoming);
	xrtAVLInit(&Session->Outgoing);
	Session->WaitingTail = &Session->Waiting;
	xrtAtomic32Init(&Session->Unavailable, 0);
	xrtAtomic32Init(&Session->OutgoingCount, 0);
}

static bool xmqttServerClaimId(xmqttserverpeer* Peer, xstrview Id)
{
	xmqttserver* Server = Peer->Server;
	xmqttserversession* Session = xrtCalloc(1, sizeof(*Session) + Id.Size);
	if (!Session) return false;
	char* Name = (char*)(Session + 1);
	memcpy(Name, Id.Data, Id.Size);
	xmqttServerSessionInit(Session, Peer, (xstrview){Name, Id.Size});
	xbytesview Key = {(cbytes)Name, Id.Size};

	xrtMutexLock(&Server->Gate);
	bool New;
	xmqttserversession** Entry = Server->Info.Closing ? NULL : xrtMapGetOrAdd(&Server->Identities, Key, &New);
	if (!Entry) {
		xrtMutexUnlock(&Server->Gate);
		xrtFree(Session);
		return false;
	}
	xmqttserverpeer* Previous = NULL;
	if (!New) {
		xmqttserversession* Old = *Entry;
		Old->RegisteredKey = (xbytesview){0};
		Session->Identity = Old->Identity;
		xrtAtomic32Store(&Old->Unavailable, 1, XMEMORY_RELEASE);
		if (xmqttServerPeerTryRef(Old->Peer)) Previous = Old->Peer;
	} else {
		Session->Identity = xrtNetWorkerOperationId(xrtNetStreamWorker(Peer->Stream));
		++Server->Info.Established;
	}
	if (!xrtMapStoredKey(&Server->Identities, Key, &Session->RegisteredKey)) abort();
	Peer->Session = Session;
	*Entry = Session;
	xrtMutexUnlock(&Server->Gate);
	if (Previous) {
		xrtNetStreamAbort(Previous->Stream);
		xmqttServerPeerDrop(Previous);
	}
	return true;
}

static void xmqttServerConnect(xmqttserverpeer* Peer, const xmqttpacket* Packet, double Now)
{
	xmqttserver* Server = Peer->Server;
	const xmqttconnectpacket* Connect = &Packet->Body.Connect;
	uint32 SessionExpiry = 0;
	uint16 SendMaximum = UINT16_MAX;
	size_t PacketLimit = XMQTT_PACKET_MAX_BYTES;
	bool AuthMethod = false;
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		const xmqttproperty* Property = &Packet->Properties.Data[Index];
		if (Property->Id == XMQTT_PROPERTY_SESSION_EXPIRY) SessionExpiry = Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE) PacketLimit = Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_RECEIVE_MAXIMUM) SendMaximum = (uint16)Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_AUTH_METHOD) AuthMethod = true;
	}
	if (!Connect->ClientId.Size || Connect->ClientId.Size > Server->Config.MaxClientIdBytes) {
		xmqttServerRefuse(Peer, 0x85, 2, PacketLimit, Now);
		return;
	}
	if (AuthMethod) {
		xmqttServerRefuse(Peer, 0x8c, 5, PacketLimit, Now);
		return;
	}
	if (Connect->HasWill || SessionExpiry || (Peer->Version == XMQTT_VERSION_311 && !Connect->CleanStart)) {
		xmqttServerRefuse(Peer, 0x83, 3, PacketLimit, Now);
		return;
	}
	xmqttserverpermission Permission = XMQTT_SERVER_ALLOW;
	if (Server->Config.Authorize)
		Permission = Server->Config.Authorize(Peer->Version, Connect, Server->Config.User);
	if (Permission != XMQTT_SERVER_ALLOW) {
		if (Permission == XMQTT_SERVER_BUSY) xmqttServerRefuse(Peer, 0x89, 3, PacketLimit, Now);
		else xmqttServerRefuse(Peer, 0x87, 5, PacketLimit, Now);
		return;
	}
	/* Publish the initialized transport limits along with the session. */
	Peer->PacketLimit = PacketLimit;
	Peer->SendMaximum = SendMaximum;
	Peer->KeepAliveMs = (unsigned)Connect->KeepAlive * 1500u;
	if (!xmqttServerClaimId(Peer, Connect->ClientId)) {
		xmqttServerRefuse(Peer, 0x88, 3, PacketLimit, Now);
		return;
	}
	if (!xmqttServerDeadline(Peer, Now, Peer->KeepAliveMs) ||
		!xmqttServerWriteConnack(Peer, 0, 0, PacketLimit)) {
		xmqttServerAbortPeer(Peer);
		return;
	}
	Peer->Online = true;
}

static void xmqttServerRemoveRetained(xmqttserver* Server, xmqttservershard* Shard,
	xmqttserverretained* Retained);

static bool xmqttServerInitTopics(xmqttserver* Server)
{
	for (; Server->ShardCount < sizeof(Server->Shards) / sizeof(Server->Shards[0]); ++Server->ShardCount) {
		xmqttservershard* Shard = Server->Shards + Server->ShardCount;
		if (!xrtRWLockInit(&Shard->Gate)) return false;
		xrtAVLInit(&Shard->Topics);
		xrtAVLInit(&Shard->WildcardRoot.Children);
		xrtAtomic32Init(&Shard->WildcardFilters, 0);
		if (Server->Config.MaxRetainedTopics && Server->ShardCount < XMQTT_SERVER_TOPIC_SHARDS) {
			if (!xrtRWLockInit(&Shard->RetainedGate)) {
				xrtRWLockUnit(&Shard->Gate);
				return false;
			}
			xrtAVLInit(&Shard->Retained);
			xrtAVLInit(&Shard->RetainedExpiry);
		}
	}
	return true;
}

static void xmqttServerUnitTopics(xmqttserver* Server)
{
	while (Server->ShardCount) {
		xmqttservershard* Shard = Server->Shards + --Server->ShardCount;
		if (Shard->WildcardRoot.Children.Root || Shard->Topics.Root) abort();
		if (Server->Config.MaxRetainedTopics && Server->ShardCount < XMQTT_SERVER_TOPIC_SHARDS) {
			while (Shard->Retained.Root)
				xmqttServerRemoveRetained(Server, Shard, (xmqttserverretained*)Shard->Retained.Root);
			xrtRWLockUnit(&Shard->RetainedGate);
		}
		xrtRWLockUnit(&Shard->Gate);
	}
}

static xmqttservershard* xmqttServerShard(xmqttserver* Server, uint64 Hash)
{
	return Server->Shards + (Hash & (XMQTT_SERVER_TOPIC_SHARDS - 1));
}

static xmqttservershard* xmqttServerWildcardShard(xmqttserver* Server, xbytesview Key)
{
	if (Key.Data[0] == '+' || Key.Data[0] == '#')
		return Server->Shards + XMQTT_SERVER_ROOT_WILDCARDS;
	xbytesview First = xmqttServerLevel(&Key);
	return xmqttServerShard(Server, xmqttServerIdHash(First, &Server->HashKey));
}

static bool xmqttServerHasWildcard(xbytesview Filter)
{
	return memchr(Filter.Data, '#', Filter.Size) || memchr(Filter.Data, '+', Filter.Size);
}

static xbytesview xmqttServerShareName(xbytesview* Key, bool V5)
{
	if (!V5 || Key->Size <= 7 || memcmp(Key->Data, "$share/", 7)) return (xbytesview){0};
	Key->Data += 7;
	Key->Size -= 7;
	return xmqttServerLevel(Key);
}

static xmqttservershare* xmqttServerGetShare(xmqttservertopic* Topic, xbytesview Name)
{
	for (xmqttservershare* Share = Topic->Shares; Share; Share = Share->Next)
		if (xmqttServerIdEqual(Share->Name, Name, NULL)) return Share;

	xmqttservershare* Share = xrtMalloc(sizeof(*Share) + Name.Size);
	if (!Share) return NULL;
	*Share = (xmqttservershare){.Next = Topic->Shares};
	unsigned char* Text = (unsigned char*)(Share + 1);
	memcpy(Text, Name.Data, Name.Size);
	Share->Name = (xbytesview){Text, Name.Size};
	xrtAtomicPtrInit(&Share->Cursor, NULL);
	if (Share->Next) Share->Next->Previous = Share;
	Topic->Shares = Share;
	return Share;
}

static xmqttserverlink** xmqttServerFindLink(xmqttserverpeer* Peer, xbytesview Key, xbytesview Name)
{
	for (xmqttserverlink** Place = &Peer->Session->Links; ; Place = &(*Place)->SessionNext) {
		xmqttserverlink* Link = *Place;
		if (!Link) return Place;
		if (xmqttServerIdEqual(Link->Topic->Key.Text, Key, NULL) &&
			(Link->Share ? Name.Size && xmqttServerIdEqual(Link->Share->Name, Name, NULL) : !Name.Size))
			return Place;
	}
}

static uint8 xmqttServerSubscribe(xmqttserverpeer* Peer, xbytesview Key, xbytesview Name,
	uint8 Options, xbytesview* Replay)
{
	xmqttserver* Server = Peer->Server;
	uint8 Grant = Options & XMQTT_SUBSCRIBE_QOS_MASK;
	uint8 Handling = Options & XMQTT_SUBSCRIBE_RETAIN_HANDLING_MASK;
	bool Retained = !Name.Size && Server->Config.MaxRetainedTopics;
	xmqttserverlink* Existing = *xmqttServerFindLink(Peer, Key, Name);
	if (Existing) {
		xrtRWLockWrite(&Existing->Shard->Gate);
		Existing->Options = Options;
		if (Replay && Retained && !Handling) *Replay = Existing->Topic->Key.Text;
		xrtRWLockWriteUnlock(&Existing->Shard->Gate);
		return Grant;
	}
	if (Peer->Session->LinkCount >= Server->Config.MaxSubscriptionsPerConnection) return 0x97;

	bool Wildcard = xmqttServerHasWildcard(Key);
	uint64 Hash = xmqttServerIdHash(Key, &Server->HashKey);
	xmqttservershard* Shard = Wildcard ? xmqttServerWildcardShard(Server, Key) : xmqttServerShard(Server, Hash);
	xrtRWLockWrite(&Shard->Gate);
	xmqttservertopic* Topic = xmqttServerFindKey(&Shard->Topics, Hash, Key);
	if ((Topic && Topic->Count >= Server->Config.MaxSubscribersPerTopic) ||
		!xmqttServerReserve(&Server->Subscriptions, Server->Config.MaxSubscriptions)) {
		xrtRWLockWriteUnlock(&Shard->Gate);
		return 0x97;
	}

	xmqttserverlink* Link = xrtMalloc(sizeof(*Link));
	uint8 Rejected = 0x83;
	if (Link && !Topic) {
		Topic = xrtMalloc(sizeof(*Topic) + Key.Size);
		if (Topic) {
			*Topic = (xmqttservertopic){0};
			xmqttServerInsertKey(&Shard->Topics, &Topic->Key, Hash, Key, (unsigned char*)(Topic + 1));
			if (Wildcard) {
				uint8 Reason = xmqttServerAddWildcard(Server, Shard, Topic);
				if (Reason) {
					Rejected = Reason;
					xmqttServerRemoveTopic(Server, Shard, Topic);
					Topic = NULL;
				}
			}
		}
	}
	xmqttservershare* Share = Link && Topic && Name.Size ? xmqttServerGetShare(Topic, Name) : NULL;
	if (!Link || !Topic || (Name.Size && !Share)) {
		if (Topic && !Topic->Count) xmqttServerRemoveTopic(Server, Shard, Topic);
		xrtFree(Link);
		xrtRWLockWriteUnlock(&Shard->Gate);
		xrtAtomic32FetchSub(&Server->Subscriptions, 1, XMEMORY_RELAXED);
		return Rejected;
	}
	xmqttserverlink** Head = Share ? &Share->Head : &Topic->Head;
	*Link = (xmqttserverlink){
		.Session = Peer->Session, .Topic = Topic, .Shard = Shard, .Share = Share, .Next = *Head,
		.SessionNext = Peer->Session->Links, .Options = Options
	};
	if (Link->Next) Link->Next->Previous = Link;
	else if (Share) xrtAtomicPtrStore(&Share->Cursor, Link, XMEMORY_RELAXED);
	*Head = Link;
	++Topic->Count;
	Peer->Session->Links = Link;
	++Peer->Session->LinkCount;
	if (Replay && Retained && Handling != 0x20) *Replay = Topic->Key.Text;
	xrtRWLockWriteUnlock(&Shard->Gate);
	return Grant;
}

static bool xmqttServerUnsubscribe(xmqttserverpeer* Peer, xbytesview Key, xbytesview Name)
{
	xmqttserverlink** Place = xmqttServerFindLink(Peer, Key, Name);
	xmqttserverlink* Link = *Place;
	if (!Link) return false;
	*Place = Link->SessionNext;
	xmqttServerUnlinkTopic(Link);
	return true;
}

static void xmqttServerProtocolClose(xmqttserverpeer* Peer, uint8 Reason, double Now)
{
	const unsigned char Wire[] = {0xe0, 1, Reason};
	Peer->Closing = true;
	if (Peer->Version == XMQTT_VERSION_5 && Peer->PacketLimit >= sizeof(Wire) &&
		xrtNetStreamSend(Peer->Stream, Wire, sizeof(Wire)) == XNET_RESULT_OK)
		xmqttServerFinishPeer(Peer, Now);
	else
		xmqttServerAbortPeer(Peer);
}

static size_t xmqttServerIdWire(unsigned char Wire[5], uint8 Header, uint16 Identifier, uint8 Reason)
{
	Wire[0] = Header;
	Wire[1] = Reason ? 3 : 2;
	Wire[2] = (uint8)(Identifier >> 8);
	Wire[3] = (uint8)Identifier;
	Wire[4] = Reason;
	return Reason ? 5 : 4;
}

static void xmqttServerSendId(xmqttserverpeer* Peer, uint8 Header, uint16 Identifier, uint8 Reason)
{
	unsigned char Wire[5];
	size_t Bytes = xmqttServerIdWire(Wire, Header, Identifier, Peer->Version == XMQTT_VERSION_5 ? Reason : 0);
	if (Bytes > Peer->PacketLimit || xrtNetStreamSend(Peer->Stream, Wire, Bytes) != XNET_RESULT_OK)
		xmqttServerAbortPeer(Peer);
}

static uint8 xmqttServerTopicReason(xmqttserverpeer* Peer, xstrview Topic, bool Publishing)
{
	const xmqttserverconfig* Config = &Peer->Server->Config;
	if (!Config->AuthorizeTopic) return 0;
	xmqttserverpermission Permission =
		Config->AuthorizeTopic(Peer->Version, Peer->Session->ClientName, Topic, Publishing, Config->User);
	if (Permission == XMQTT_SERVER_ALLOW) return 0;
	return Permission == XMQTT_SERVER_BUSY ? 0x97 : 0x87;
}

static void xmqttServerReplayRetained(xmqttserverpeer* Peer, xbytesview Filter, uint8 Options);

static void xmqttServerFilterRequest(xmqttserverpeer* Peer, const xmqttpacket* Packet, double Now)
{
	bool Subscribe = Packet->Type == XMQTT_PACKET_SUBSCRIBE;
	bool V5 = Peer->Version == XMQTT_VERSION_5;
	xmqttsubscriptionlist Filters = Packet->Body.Topics;
	if (Subscribe && V5) {
		for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
			if (Packet->Properties.Data[Index].Id == XMQTT_PROPERTY_SUBSCRIPTION_ID) {
				xmqttServerProtocolClose(Peer, 0xa1, Now);
				return;
			}
		}
		for (size_t Index = 0; Index < Filters.Count; ++Index) {
			xstrview Filter = Filters.Data[Index].Filter;
			xbytesview Key = {(cbytes)Filter.Data, Filter.Size};
			if (!Peer->Server->Config.MaxWildcardNodes && xmqttServerHasWildcard(Key)) {
				xmqttServerProtocolClose(Peer, 0xa2, Now);
				return;
			}
		}
	}

	unsigned char Reasons[XMQTT_SERVER_MAX_FILTERS];
	struct { xbytesview Filter; uint8 Options; } Replay[XMQTT_SERVER_MAX_FILTERS];
	size_t ReplayCount = 0;
	for (size_t Index = 0; Index < Filters.Count; ++Index) {
		const xmqttsubscription* Filter = Filters.Data + Index;
		xbytesview Key = {(cbytes)Filter->Filter.Data, Filter->Filter.Size};
		xbytesview Name = xmqttServerShareName(&Key, V5);
		uint8 Reason;
		if (!Subscribe) {
			Reason = xmqttServerUnsubscribe(Peer, Key, Name) ? 0 : 0x11;
		} else if (Filter->Filter.Size > Peer->Server->Config.MaxTopicBytes ||
			(!V5 && !Peer->Server->Config.MaxWildcardNodes && xmqttServerHasWildcard(Key))) {
			Reason = 0x8f;
		} else if (!(Reason = xmqttServerTopicReason(Peer, Filter->Filter, false))) {
			/* Keep the topic-owned filter through native sends. This peer
			 * owns its link until the deferred connection retirement. */
			xbytesview FilterToReplay = {0};
			Reason = xmqttServerSubscribe(Peer, Key, Name, Filter->Options, &FilterToReplay);
			if (FilterToReplay.Data) {
				Replay[ReplayCount].Filter = FilterToReplay;
				Replay[ReplayCount++].Options = (Filter->Options & XMQTT_SUBSCRIBE_NO_LOCAL) | Reason;
			}
		}
		Reasons[Index] = V5 || Reason < 0x80 ? Reason : 0x80;
	}

	xmqttpacket Reply;
	xmqttPacketInit(Subscribe ? XMQTT_PACKET_SUBACK : XMQTT_PACKET_UNSUBACK, &Reply);
	Reply.Identifier = Packet->Identifier;
	if (Subscribe || V5) Reply.Body.Reasons = (xbytesview){Reasons, Filters.Count};
	unsigned char Wire[XMQTT_SERVER_MAX_FILTERS + 10];
	size_t Bytes = 0;
	bool Encoded;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (!V5)
		Encoded = xmqttPacketWrite311(Wire, sizeof(Wire), &Reply, XMQTT_SERVER_TO_CLIENT, NULL, &Bytes);
	else
#endif
		Encoded = xmqttPacketWrite5(Wire, sizeof(Wire), &Reply, XMQTT_SERVER_TO_CLIENT, NULL, &Bytes);
	if (!Encoded || Bytes > Peer->PacketLimit || xrtNetStreamSend(Peer->Stream, Wire, Bytes) != XNET_RESULT_OK) {
		xmqttServerAbortPeer(Peer);
		return;
	}
	for (size_t Index = 0; Index < ReplayCount && !Peer->Closing; ++Index)
		xmqttServerReplayRetained(Peer, Replay[Index].Filter, Replay[Index].Options);
}

struct xmqttserverpublication {
	xatomic32 References;
	size_t Size;
	uint32 IdOffset, ExpiryOffset;
	double Expires;
	unsigned char Wire[];
};

static void xmqttServerFreePublication(ptr Data, cbytes Wire, size_t Size)
{
	(void)Wire;
	(void)Size;
	xmqttserverpublication* Publication = Data;
	if (xrtAtomic32FetchSub(&Publication->References, 1, XMEMORY_ACQ_REL) == 1)
		xrtFree(Publication);
}

static xmqttserverpublication* xmqttServerCopyPublication(xbytesview Wire, uint8 Flags, xstrview Publisher)
{
	xmqttserverpublication* Publication = xrtMalloc(sizeof(*Publication) + Wire.Size + Publisher.Size);
	if (!Publication) return NULL;
	xrtAtomic32Init(&Publication->References, 1);
	Publication->Size = Wire.Size;
	Publication->IdOffset = Publication->ExpiryOffset = 0;
	Publication->Expires = 0;
	memcpy(Publication->Wire, Wire.Data, Wire.Size);
	Publication->Wire[0] = (Publication->Wire[0] & 0xf0u) | Flags;
	if (Publisher.Size) memcpy(Publication->Wire + Wire.Size, Publisher.Data, Publisher.Size);
	return Publication;
}

static xmqttserverpublication* xmqttServerEncodePublish(const xmqttpacket* Packet, xmqttversion Origin,
	xmqttversion Target, uint8 Flags, bool Rewrite, double Expires, const xmqttcodecconfig* Codec)
{
	if (Origin == Target && !Rewrite) return xmqttServerCopyPublication(Packet->Wire, Flags, (xstrview){0});
	xmqttpacket Out = *Packet;
	Out.Flags = Flags;
	bool Reliable = (Flags & XMQTT_PUBLISH_QOS_MASK) != 0;
	Out.Identifier = Reliable ? 1 : 0;
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	if (Target == XMQTT_VERSION_311) Out.Properties = (xmqttpropertylist){0};
	else if (Reliable && Expires) {
		/* Put Expiry first for a small per-subscriber patch. Keep the order
		 * of every other property, including repeated User Properties. */
		size_t Next = 1;
		for (size_t Index = 0; Index < Out.Properties.Count; ++Index) {
			xmqttproperty Property = Out.Properties.Data[Index];
			if (Property.Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Properties[0] = Property;
			else Properties[Next++] = Property;
		}
		Out.Properties.Data = Properties;
	}
	xmqttcodecconfig Encoding = *Codec;
	Encoding.MaxPacketBytes = XMQTT_PACKET_MAX_BYTES;
	size_t Size;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Target == XMQTT_VERSION_311) {
		if (!xmqttPacketSize311(&Out, XMQTT_SERVER_TO_CLIENT, &Encoding, &Size)) return NULL;
	} else
#endif
	{
		if (!xmqttPacketSize5(&Out, XMQTT_SERVER_TO_CLIENT, &Encoding, &Size)) return NULL;
	}

	xmqttserverpublication* Publication = xrtMalloc(sizeof(*Publication) + Size);
	if (!Publication) return NULL;
	xrtAtomic32Init(&Publication->References, 1);
	Publication->Size = Size;
	Publication->IdOffset = Publication->ExpiryOffset = 0;
	Publication->Expires = Expires;
	size_t Written;
	bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Target == XMQTT_VERSION_311)
		OK = xmqttPacketWrite311(Publication->Wire, Size, &Out, XMQTT_SERVER_TO_CLIENT, &Encoding, &Written);
	else
#endif
		OK = xmqttPacketWrite5(Publication->Wire, Size, &Out, XMQTT_SERVER_TO_CLIENT, &Encoding, &Written);
	if (!OK) {
		xrtFree(Publication);
		return NULL;
	}
	if (Reliable) {
		size_t At = 1;
		while (Publication->Wire[At++] & 0x80u) { }
		At += 2 + Out.Body.Publish.Topic.Size;
		Publication->IdOffset = (uint32)At;
		if (Expires && Target == XMQTT_VERSION_5) {
			At += 2;
			while (Publication->Wire[At++] & 0x80u) { }
			Publication->ExpiryOffset = (uint32)(At + 1);
		}
	}
	return Publication;
}

static bool xmqttServerDeliver(xmqttserverpeer* Peer, xmqttserverpublication* Publication)
{
	if (!Publication || Publication->Size > Peer->PacketLimit ||
		xrtAtomic32Load(&Peer->Session->Unavailable, XMEMORY_ACQUIRE)) return false;
	xrtAtomic32FetchAdd(&Publication->References, 1, XMEMORY_RELAXED);
	if (xrtNetStreamSendRef(Peer->Stream, Publication->Wire, Publication->Size,
		xmqttServerFreePublication, Publication) == XNET_RESULT_OK) return true;
	xmqttServerFreePublication(Publication, NULL, 0);
	return false;
}

/* Posted deliveries share the immutable payload. Only the short prefix up
 * through Packet Identifier (and optional expiry) is private to a recipient. */
struct xmqttserverdelivery {
	xavlnode Node;
	xnetpost Post;
	xmqttserversession* Session;
	xmqttserverpublication* Publication;
	xmqttserverdelivery* Next;
	size_t PrefixBytes;
	uint16 Identifier;
	uint8 ExpectedAck;
	unsigned References;
	unsigned char Prefix[];
};

static void xmqttServerDropDelivery(ptr Data, cbytes Wire, size_t Size)
{
	(void)Wire;
	(void)Size;
	xmqttserverdelivery* Delivery = Data;
	if (--Delivery->References) return;
	xmqttserversession* Session = Delivery->Session;
	xmqttserverpeer* Peer = Session->Peer;
	xmqttserverpublication* Publication = Delivery->Publication;
	xrtAtomic64FetchSub(&Session->Server->OutgoingPublishBytes, Publication->Size, XMEMORY_RELAXED);
	xrtAtomic32FetchSub(&Session->Server->OutgoingPublishes, 1, XMEMORY_RELAXED);
	xrtAtomic32FetchSub(&Session->OutgoingCount, 1, XMEMORY_RELAXED);
	xmqttServerFreePublication(Publication, NULL, 0);
	xrtFree(Delivery);
	xmqttServerPeerDrop(Peer);
}

static void xmqttServerDiscardDelivery(xmqttserverdelivery* Delivery)
{
	xrtAtomic64FetchAdd(&Delivery->Session->Server->DroppedDeliveries, 1, XMEMORY_RELAXED);
	xmqttServerDropDelivery(Delivery, NULL, 0);
}

static int xmqttServerCompareDelivery(const void* Key, const xavlnode* Node, ptr User)
{
	(void)User;
	uint16 Identifier = *(const uint16*)Key, Current = ((const xmqttserverdelivery*)Node)->Identifier;
	return (Identifier > Current) - (Identifier < Current);
}

static xmqttserverdelivery* xmqttServerPopWaiting(xmqttserversession* Session)
{
	xmqttserverdelivery* Delivery = Session->Waiting;
	Session->Waiting = Delivery->Next;
	if (!Session->Waiting) Session->WaitingTail = &Session->Waiting;
	return Delivery;
}

static void xmqttServerPumpOutgoing(xmqttserverpeer* Peer)
{
	xmqttserversession* Session = Peer->Session;
	if (!Session || Peer->Pumping) return;
	Peer->Pumping = true;
	while (Session->Waiting && Session->Outgoing.Count < Peer->SendMaximum && !Peer->Closing &&
		!xrtAtomic32Load(&Peer->OutgoingStopped, XMEMORY_RELAXED) &&
		!xrtAtomic32Load(&Session->Unavailable, XMEMORY_ACQUIRE)) {
		xmqttserverdelivery* Delivery = xmqttServerPopWaiting(Session);
		xmqttserverpublication* Publication = Delivery->Publication;
		if (Publication->Expires) {
			double Remaining = Publication->Expires - xrtTimer();
			if (!(Remaining > 0)) {
				xmqttServerDiscardDelivery(Delivery);
				continue;
			}
			if (Publication->ExpiryOffset) {
				const unsigned char* Original = Publication->Wire + Publication->ExpiryOffset;
				uint32 Seconds = 0;
				for (unsigned Byte = 0; Byte < 4; ++Byte) Seconds = (Seconds << 8u) | Original[Byte];
				/* A double timestamp sum can round up. Bound the result by
				 * the encoded interval before converting back to uint32. */
				if (Remaining < Seconds) Seconds = (uint32)ceil(Remaining);
				unsigned char* Value = Delivery->Prefix + Publication->ExpiryOffset;
				for (unsigned Byte = 0; Byte < 4; ++Byte)
					Value[Byte] = (uint8)(Seconds >> ((3u - Byte) * 8u));
			}
		}
		uint16 Id = Session->LastIdentifier;
		do {
			if (!++Id) Id = 1;
		} while (xrtAVLFind(&Session->Outgoing, &Id, xmqttServerCompareDelivery, NULL));
		unsigned char* Identifier = Delivery->Prefix + Publication->IdOffset;
		Identifier[0] = (uint8)(Id >> 8);
		Identifier[1] = (uint8)Id;
		const xnetref Parts[] = {
			{.Data = Delivery->Prefix, .Size = Delivery->PrefixBytes,
				.Release = xmqttServerDropDelivery, .Context = Delivery},
			{.Data = Publication->Wire + Delivery->PrefixBytes, .Size = Publication->Size - Delivery->PrefixBytes,
				.Release = xmqttServerDropDelivery, .Context = Delivery},
		};
		size_t Count = Parts[1].Size ? 2 : 1;
		Delivery->References += (unsigned)Count;
		xnetresult Result = xrtNetStreamSendRefs(Peer->Stream, Parts, Count);
		if (Result != XNET_RESULT_OK) Delivery->References -= (unsigned)Count;
		if (Result == XNET_RESULT_AGAIN && !Peer->Closing) {
			Delivery->Next = Session->Waiting;
			if (!Session->Waiting) Session->WaitingTail = &Delivery->Next;
			Session->Waiting = Delivery;
			break;
		}
		if (Result != XNET_RESULT_OK) {
			xmqttServerDiscardDelivery(Delivery);
			if (!Peer->Closing && !xrtAtomic32Exchange(&Peer->OutgoingStopped, 1, XMEMORY_ACQ_REL)) {
				xrtAtomic64FetchAdd(&Peer->Server->OutgoingPublishOverloads, 1, XMEMORY_RELAXED);
				xmqttServerProtocolClose(Peer, 0x97, xrtTimer());
			}
			break;
		}
		xrtAtomic64FetchAdd(&Peer->Server->Forwarded, 1, XMEMORY_RELAXED);
		if (Peer->Closing) {
			xmqttServerDropDelivery(Delivery, NULL, 0);
			break;
		}
		Session->LastIdentifier = Delivery->Identifier = Id;
		Delivery->ExpectedAck = (Publication->Wire[0] & XMQTT_PUBLISH_QOS_MASK) == 4 ?
			XMQTT_PACKET_PUBREC : XMQTT_PACKET_PUBACK;
		xrtAVLNodeInit(&Delivery->Node);
		if (xrtAVLInsert(&Session->Outgoing, &Delivery->Node, &Id, xmqttServerCompareDelivery, NULL, NULL) !=
			&Delivery->Node) abort();
	}
	Peer->Pumping = false;
}

static void xmqttServerPostDelivery(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverdelivery* Delivery = Data;
	xmqttserversession* Session = Delivery->Session;
	xmqttserverpeer* Peer = Session->Peer;
	if (Peer->Closing || xrtAtomic32Load(&Peer->OutgoingStopped, XMEMORY_RELAXED) ||
		xrtAtomic32Load(&Session->Unavailable, XMEMORY_ACQUIRE)) {
		xmqttServerDiscardDelivery(Delivery);
		return;
	}
	*Session->WaitingTail = Delivery;
	Session->WaitingTail = &Delivery->Next;
	xmqttServerPumpOutgoing(Peer);
}

static void xmqttServerPostOverload(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	if (!Peer->Closing) xmqttServerProtocolClose(Peer, 0x97, xrtTimer());
	xmqttServerPeerDrop(Peer);
}

/* Consumes the caller's peer reference. */
static void xmqttServerStopOutgoing(xmqttserverpeer* Peer)
{
	if (!xrtAtomic32Exchange(&Peer->OutgoingStopped, 1, XMEMORY_ACQ_REL)) {
		xrtAtomic64FetchAdd(&Peer->Server->OutgoingPublishOverloads, 1, XMEMORY_RELAXED);
		if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Peer->OutgoingOverload,
			xmqttServerPostOverload, Peer)) return;
		xrtNetStreamAbort(Peer->Stream);
	}
	xmqttServerPeerDrop(Peer);
}

/* The caller holds a link shard or is the owner. Every accepted hand-off owns
 * one live peer ref through worker and TCP retirement. Clean sessions end on
 * quota or allocation failure so QoS 1 is never silently downgraded. */
static bool xmqttServerQueueOutgoing(xmqttserverpeer* Peer, xmqttserverpublication* Publication)
{
	if (!Publication || Publication->Size > Peer->PacketLimit ||
		xrtAtomic32Load(&Peer->Session->Unavailable, XMEMORY_ACQUIRE) || !xmqttServerPeerTryRef(Peer)) return false;
	if (xrtAtomic32Load(&Peer->OutgoingStopped, XMEMORY_RELAXED)) {
		xmqttServerPeerDrop(Peer);
		return false;
	}
	xmqttserver* Server = Peer->Server;
	const xmqttserverconfig* Config = &Server->Config;
	if (Publication->Size > xrtNetStreamWriteLimit(Peer->Stream) ||
		!xmqttServerReserve(&Peer->Session->OutgoingCount, Config->MaxOutgoingPublishesPerConnection)) goto Overload;
	if (!xmqttServerReserve(&Server->OutgoingPublishes, Config->MaxOutgoingPublishes)) goto ReleasePeerCredit;
	if (!xmqttServerReserveBytes(&Server->OutgoingPublishBytes, Config->MaxOutgoingPublishBytes, Publication->Size))
		goto ReleaseServerCredit;
	size_t Bytes = Publication->ExpiryOffset ? Publication->ExpiryOffset + 4u : Publication->IdOffset + 2u;
	xmqttserverdelivery* Delivery = xrtMalloc(sizeof(*Delivery) + Bytes);
	if (Delivery) {
		Delivery->Session = Peer->Session;
		Delivery->Publication = Publication;
		Delivery->Next = NULL;
		Delivery->References = 1;
		Delivery->PrefixBytes = Bytes;
		memcpy(Delivery->Prefix, Publication->Wire, Bytes);
		if (!xrtNetPostInit(&Delivery->Post)) abort();
		xrtAtomic32FetchAdd(&Publication->References, 1, XMEMORY_RELAXED);
		if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Delivery->Post, xmqttServerPostDelivery, Delivery))
			return true;
		xmqttServerFreePublication(Publication, NULL, 0);
		xrtFree(Delivery);
	}
	xrtAtomic64FetchSub(&Server->OutgoingPublishBytes, Publication->Size, XMEMORY_RELAXED);
ReleaseServerCredit:
	xrtAtomic32FetchSub(&Server->OutgoingPublishes, 1, XMEMORY_RELAXED);
ReleasePeerCredit:
	xrtAtomic32FetchSub(&Peer->Session->OutgoingCount, 1, XMEMORY_RELAXED);
Overload:
	xmqttServerStopOutgoing(Peer);
	return false;
}

static xmqttserverretained* xmqttServerExpiryRecord(xavlnode* Node)
{
	return (xmqttserverretained*)((unsigned char*)Node - offsetof(xmqttserverretained, ExpiryNode));
}

static int xmqttServerCompareExpiry(const void* Key, const xavlnode* Node, ptr User)
{
	(void)User;
	const xmqttserverretained* Left = Key;
	const xmqttserverretained* Right = xmqttServerExpiryRecord((xavlnode*)Node);
	if (Left->Expires != Right->Expires) return Left->Expires < Right->Expires ? -1 : 1;
	uintptr_t A = (uintptr_t)Left, B = (uintptr_t)Right;
	return (A > B) - (A < B);
}

static void xmqttServerUnlinkExpiry(xmqttserver* Server, xmqttservershard* Shard, xmqttserverretained* Retained)
{
	if (!Retained->Expires) return;
	if (xrtAVLRemove(&Shard->RetainedExpiry, Retained, xmqttServerCompareExpiry, NULL) !=
		&Retained->ExpiryNode) abort();
	xrtAtomic32FetchSub(&Server->RetainedExpiring, 1, XMEMORY_RELAXED);
}

static void xmqttServerRemoveRetained(xmqttserver* Server, xmqttservershard* Shard,
	xmqttserverretained* Retained)
{
	xmqttServerUnlinkExpiry(Server, Shard, Retained);
	if (xrtAVLRemove(&Shard->Retained, &Retained->Key, xmqttServerCompareKey, NULL) !=
		&Retained->Key.Node) abort();
	xmqttserverpublication* Publication = Retained->Publication;
	xrtAtomic64FetchSub(&Server->RetainedBytes, Publication->Size + Retained->PublisherIdBytes, XMEMORY_RELAXED);
	xrtAtomic32FetchSub(&Server->RetainedTopics, 1, XMEMORY_RELAXED);
	xmqttServerFreePublication(Publication, NULL, 0);
	xrtFree(Retained);
}

static void xmqttServerExpireRetained(xmqttserver* Server, xmqttservershard* Shard, double Now)
{
	for (;;) {
		xavlnode* First = xrtAVLFirst(&Shard->RetainedExpiry);
		if (!First) return;
		xmqttserverretained* Earliest = xmqttServerExpiryRecord(First);
		if (Earliest->Expires > Now) return;
		xmqttServerRemoveRetained(Server, Shard, Earliest);
	}
}

static bool xmqttServerReserveRetained(xmqttserver* Server, size_t Bytes, bool New)
{
	if (New && !xmqttServerReserve(&Server->RetainedTopics, Server->Config.MaxRetainedTopics)) return false;
	if (xmqttServerReserveBytes(&Server->RetainedBytes, Server->Config.MaxRetainedBytes, Bytes)) return true;
	if (New) xrtAtomic32FetchSub(&Server->RetainedTopics, 1, XMEMORY_RELAXED);
	return false;
}

static bool xmqttServerStoreRetained(xmqttserverpeer* Peer, const xmqttpacket* Packet,
	const xmqttproperty* Expiry, double Now)
{
	xmqttserver* Server = Peer->Server;
	xstrview Name = Packet->Body.Publish.Topic;
	xbytesview Key = {(cbytes)Name.Data, Name.Size};
	xmqttservershard* Shard = xmqttServerWildcardShard(Server, Key);
	uint64 Hash = xmqttServerIdHash(Key, &Server->HashKey);
	size_t Bytes = Packet->Wire.Size + Peer->Session->ClientName.Size;
	bool Store = Packet->Body.Publish.Payload.Size && (!Expiry || Expiry->Value.Number);
	bool Room = Bytes <= Server->Config.MaxRetainedBytes;

	xrtRWLockWrite(&Shard->RetainedGate);
	xmqttServerExpireRetained(Server, Shard, Now);
	xmqttserverretained* Previous = xmqttServerFindKey(&Shard->Retained, Hash, Key);
	size_t PreviousBytes = Previous ? Previous->Publication->Size + Previous->PublisherIdBytes : 0;
	size_t Growth = Bytes > PreviousBytes ? Bytes - PreviousBytes : 0;
	/* Try other shards only under pressure from records that might expire.
	 * Never hold two retention locks, or a routing and retention lock. */
	if (Store && Room && xrtAtomic32Load(&Server->RetainedExpiring, XMEMORY_RELAXED) &&
		((!Previous && xrtAtomic32Load(&Server->RetainedTopics, XMEMORY_RELAXED) >= Server->Config.MaxRetainedTopics) ||
		Growth >
			Server->Config.MaxRetainedBytes - xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED))) {
		xrtRWLockWriteUnlock(&Shard->RetainedGate);
		for (size_t Index = 0; Index < XMQTT_SERVER_TOPIC_SHARDS; ++Index) {
			xmqttservershard* Other = Server->Shards + Index;
			if (Other == Shard || !xrtRWLockTryWrite(&Other->RetainedGate)) continue;
			xmqttServerExpireRetained(Server, Other, Now);
			xrtRWLockWriteUnlock(&Other->RetainedGate);
		}
		xrtRWLockWrite(&Shard->RetainedGate);
		xmqttServerExpireRetained(Server, Shard, Now);
		Previous = xmqttServerFindKey(&Shard->Retained, Hash, Key);
	}
	if (!Store) {
		if (Previous) xmqttServerRemoveRetained(Server, Shard, Previous);
		xrtRWLockWriteUnlock(&Shard->RetainedGate);
		return true;
	}

	/* A replacement reuses its index node and only reserves positive growth.
	 * Keep the previous retained QoS 1 obligation if admission fails. */
	PreviousBytes = Previous ? Previous->Publication->Size + Previous->PublisherIdBytes : 0;
	Growth = Bytes > PreviousBytes ? Bytes - PreviousBytes : 0;
	bool New = !Previous;
	xmqttserverretained* Entry = NULL;
	xmqttserverpublication* Copy = NULL;
	if (Room && xmqttServerReserveRetained(Server, Growth, New)) {
		uint8 Flags = Packet->Flags & (XMQTT_PUBLISH_QOS_MASK | XMQTT_PUBLISH_RETAIN);
		Copy = xmqttServerCopyPublication(Packet->Wire, Flags, Peer->Session->ClientName);
		if (Copy) Entry = Previous ? Previous : xrtMalloc(sizeof(*Entry) + Key.Size);
		if (!Entry) {
			if (Copy) xmqttServerFreePublication(Copy, NULL, 0);
			if (Growth) xrtAtomic64FetchSub(&Server->RetainedBytes, Growth, XMEMORY_RELAXED);
			if (New) xrtAtomic32FetchSub(&Server->RetainedTopics, 1, XMEMORY_RELAXED);
		}
	}
	if (!Entry) {
		/* The specification permits discarding an unstoreable QoS 0
		 * replacement and its previous value. QoS 1 is refused upstream. */
		if (Previous && !(Packet->Flags & XMQTT_PUBLISH_QOS_MASK))
			xmqttServerRemoveRetained(Server, Shard, Previous);
		xrtAtomic64FetchAdd(&Server->DroppedRetained, 1, XMEMORY_RELAXED);
		xrtRWLockWriteUnlock(&Shard->RetainedGate);
		return false;
	}
	if (Previous) {
		xmqttServerUnlinkExpiry(Server, Shard, Previous);
		xmqttServerFreePublication(Previous->Publication, NULL, 0);
		if (PreviousBytes > Bytes)
			xrtAtomic64FetchSub(&Server->RetainedBytes, PreviousBytes - Bytes, XMEMORY_RELAXED);
	} else {
		*Entry = (xmqttserverretained){0};
		xmqttServerInsertKey(&Shard->Retained, &Entry->Key, Hash, Key, (unsigned char*)(Entry + 1));
	}
	Entry->Publication = Copy;
	Entry->Version = Peer->Version;
	Entry->PublisherIdBytes = Peer->Session->ClientName.Size;
	Entry->Received = Now;
	Entry->Expires = Expiry ? Now + Expiry->Value.Number : 0;
	if (Entry->Expires) {
		xrtAVLNodeInit(&Entry->ExpiryNode);
		if (xrtAVLInsert(&Shard->RetainedExpiry, &Entry->ExpiryNode, Entry,
			xmqttServerCompareExpiry, NULL, NULL) != &Entry->ExpiryNode) abort();
		xrtAtomic32FetchAdd(&Server->RetainedExpiring, 1, XMEMORY_RELAXED);
	}
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
	return true;
}

static bool xmqttServerRetainedMatch(xbytesview Filter, xbytesview Name)
{
	if (Name.Data[0] == '$' && Filter.Data[0] != '$') return false;
	while (Filter.Data) {
		xbytesview Part = xmqttServerLevel(&Filter);
		if (Part.Size == 1 && Part.Data[0] == '#') return true;
		if (!Name.Data) return false;
		xbytesview Level = xmqttServerLevel(&Name);
		if (!(Part.Size == 1 && Part.Data[0] == '+') &&
			!xmqttServerIdEqual(Part, Level, NULL)) return false;
	}
	return !Name.Data;
}

static xmqttserverpublication* xmqttServerRecodeRetained(xmqttserverpeer* Peer,
	const xmqttserverretained* Retained, uint32 Elapsed, uint8 Qos)
{
	const xmqttcodecconfig* Codec = &Peer->Server->Config.Codec;
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
	xbytesview Input = {Retained->Publication->Wire, Retained->Publication->Size};
	xmqttpacket Packet;
	xmqttcodecstatus Decoded;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Retained->Version == XMQTT_VERSION_311)
		Decoded = xmqttPacketRead311(Input, XMQTT_SERVER_TO_CLIENT, Codec, &Scratch, &Packet, NULL);
	else
#endif
		Decoded = xmqttPacketRead5(Input, XMQTT_SERVER_TO_CLIENT, Codec, &Scratch, &Packet, NULL);
	if (Decoded != XMQTT_CODEC_DONE) return NULL;
	for (size_t Index = 0; Index < Packet.Properties.Count; ++Index) {
		if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) {
			if (Elapsed >= Properties[Index].Value.Number) return NULL;
			Properties[Index].Value.Number -= Elapsed;
			break;
		}
	}
	uint8 Flags = XMQTT_PUBLISH_RETAIN | (uint8)(Qos << 1);
	return xmqttServerEncodePublish(&Packet, Retained->Version, Peer->Version, Flags,
		Elapsed != 0 || (Packet.Flags & XMQTT_PUBLISH_QOS_MASK), Qos ? Retained->Expires : 0, Codec);
}

static void xmqttServerReplayEntry(xmqttserverpeer* Peer, const xmqttserverretained* Retained, uint8 Options)
{
	if ((Options & XMQTT_SUBSCRIBE_NO_LOCAL) && Peer->Session->ClientName.Size == Retained->PublisherIdBytes &&
		!memcmp(Peer->Session->ClientName.Data, Retained->Publication->Wire + Retained->Publication->Size,
			Retained->PublisherIdBytes)) return;
	uint32 Elapsed = 0;
	if (Retained->Expires) {
		double Now = xrtTimer();
		if (!isfinite(Now) || Now >= Retained->Expires) return;
		if (Now > Retained->Received) Elapsed = (uint32)(Now - Retained->Received);
	}
	uint8 SourceQos = (Retained->Publication->Wire[0] & XMQTT_PUBLISH_QOS_MASK) >> 1;
	uint8 Qos = Options & XMQTT_SUBSCRIBE_QOS_MASK;
	if (SourceQos < Qos) Qos = SourceQos;
	bool Recode = Peer->Version != Retained->Version || Elapsed || SourceQos;
	xmqttserverpublication* Frame = Recode ?
		xmqttServerRecodeRetained(Peer, Retained, Elapsed, Qos) : Retained->Publication;
	if (Qos) {
		if (!Frame && xmqttServerPeerTryRef(Peer)) xmqttServerStopOutgoing(Peer);
		if (!xmqttServerQueueOutgoing(Peer, Frame))
			xrtAtomic64FetchAdd(&Peer->Server->DroppedDeliveries, 1, XMEMORY_RELAXED);
	} else if (xmqttServerDeliver(Peer, Frame)) {
		xrtAtomic64FetchAdd(&Peer->Server->Forwarded, 1, XMEMORY_RELAXED);
	} else {
		xrtAtomic64FetchAdd(&Peer->Server->DroppedDeliveries, 1, XMEMORY_RELAXED);
	}
	if (Recode && Frame) xmqttServerFreePublication(Frame, NULL, 0);
}

static void xmqttServerReplayRetained(xmqttserverpeer* Peer, xbytesview Filter, uint8 Options)
{
	xmqttserver* Server = Peer->Server;
	if (!xrtAtomic32Load(&Server->RetainedTopics, XMEMORY_RELAXED)) return;
	xmqttservershard* Shard = xmqttServerWildcardShard(Server, Filter);
	bool Wildcard = xmqttServerHasWildcard(Filter);
	xmqttservershard* End = Shard + 1;
	if (Shard == Server->Shards + XMQTT_SERVER_ROOT_WILDCARDS) {
		End = Shard;
		Shard = Server->Shards;
	}
	for (; Shard != End && !Peer->Closing; ++Shard) {
		xrtRWLockRead(&Shard->RetainedGate);
		if (!Wildcard) {
			uint64 Hash = xmqttServerIdHash(Filter, &Server->HashKey);
			const xmqttserverretained* Entry = xmqttServerFindKey(&Shard->Retained, Hash, Filter);
			if (Entry) xmqttServerReplayEntry(Peer, Entry, Options);
		} else {
			xavliter Iterator;
			xrtAVLIterBegin(&Shard->Retained, &Iterator);
			const xmqttserverretained* Entry;
			while (!Peer->Closing && (Entry = (const xmqttserverretained*)xrtAVLIterNext(&Iterator)) != NULL)
				if (xmqttServerRetainedMatch(Filter, Entry->Key.Text)) xmqttServerReplayEntry(Peer, Entry, Options);
		}
		xrtRWLockReadUnlock(&Shard->RetainedGate);
	}
}

typedef union xmqttserversharedelivery {
	xmqttservershare* Share;
	xmqttserverlink* Member;
} xmqttserversharedelivery;

typedef struct xmqttservertargets {
	size_t Count, Capacity;
	xmqttserverlink** Slots;
	xmqttserverlink* Local[64];
	size_t ShareCount, ShareCapacity;
	xmqttserversharedelivery* Shares;
	xmqttserversharedelivery LocalShares[16];
	bool Retain;
} xmqttservertargets;

static unsigned xmqttServerWireMode(const xmqttserverlink* Link, bool Retain)
{
	if (Link->Session->Version != XMQTT_VERSION_5) return 0;
	return Retain && (Link->Options & XMQTT_SUBSCRIBE_RETAIN_AS_PUBLISHED) ? 2 : 1;
}

static unsigned xmqttServerDeliveryMode(const xmqttserverlink* Link, bool Retain, unsigned SourceQos)
{
	unsigned Qos = Link->Options & XMQTT_SUBSCRIBE_QOS_MASK;
	if (SourceQos < Qos) Qos = SourceQos;
	return xmqttServerWireMode(Link, Retain) + 3u * Qos;
}

static xmqttserverlink** xmqttServerTargetSlot(xmqttserverlink** Slots, size_t Capacity,
	xmqttserverlink* Link, bool Retain)
{
	uintptr_t Address = (uintptr_t)Link->Session;
	unsigned Mode = xmqttServerWireMode(Link, Retain);
	size_t At = (size_t)((Address >> 4u) ^ (Address >> 12u) ^ (Mode == 2)) & (Capacity - 1);
	while (Slots[At] && (Slots[At]->Session != Link->Session || xmqttServerWireMode(Slots[At], Retain) != Mode))
		At = (At + 1) & (Capacity - 1);
	return Slots + At;
}

static bool xmqttServerAddTarget(xmqttservertargets* Targets, xmqttserverlink* Link)
{
	xmqttserverlink** Slot = xmqttServerTargetSlot(Targets->Slots, Targets->Capacity, Link, Targets->Retain);
	if (*Slot) {
		if ((Link->Options & XMQTT_SUBSCRIBE_QOS_MASK) > ((*Slot)->Options & XMQTT_SUBSCRIBE_QOS_MASK))
			*Slot = Link;
		return true;
	}
	if (Targets->Count == Targets->Capacity - Targets->Capacity / 4) {
		if (Targets->Capacity > SIZE_MAX / 2 / sizeof(*Targets->Slots)) return false;
		size_t Next = Targets->Capacity * 2;
		xmqttserverlink** Slots = xrtCalloc(Next, sizeof(*Slots));
		if (!Slots) return false;
		for (size_t At = 0; At < Targets->Capacity; ++At) {
			xmqttserverlink* Item = Targets->Slots[At];
			if (Item) *xmqttServerTargetSlot(Slots, Next, Item, Targets->Retain) = Item;
		}
		if (Targets->Slots != Targets->Local) xrtFree(Targets->Slots);
		Targets->Slots = Slots;
		Targets->Capacity = Next;
		Slot = xmqttServerTargetSlot(Slots, Next, Link, Targets->Retain);
	}
	*Slot = Link;
	++Targets->Count;
	return true;
}

/* Keep shared delivery independent of ordinary duplicate elimination and of
 * every other (ShareName, Filter) pair. Membership stays pinned by the shard
 * read locks until the native send admission finishes. */
static bool xmqttServerCollectShares(xmqttservertargets* Targets, const xmqttservertopic* Topic)
{
	for (xmqttservershare* Share = Topic ? Topic->Shares : NULL; Share; Share = Share->Next) {
		if (Targets->ShareCount == Targets->ShareCapacity) {
			if (Targets->ShareCapacity > SIZE_MAX / 2 / sizeof(*Targets->Shares)) return false;
			size_t Capacity = Targets->ShareCapacity * 2;
			xmqttserversharedelivery* Shares = xrtMalloc(Capacity * sizeof(*Shares));
			if (!Shares) return false;
			memcpy(Shares, Targets->Shares, Targets->ShareCount * sizeof(*Shares));
			if (Targets->Shares != Targets->LocalShares) xrtFree(Targets->Shares);
			Targets->Shares = Shares;
			Targets->ShareCapacity = Capacity;
		}
		Targets->Shares[Targets->ShareCount++].Share = Share;
	}
	return true;
}

static xmqttserverlink* xmqttServerShareMember(xmqttservershare* Share)
{
	ptr Expected = xrtAtomicPtrLoad(&Share->Cursor, XMEMORY_RELAXED);
	for (;;) {
		xmqttserverlink* Current = Expected;
		while (xrtAtomic32Load(&Current->Session->Unavailable, XMEMORY_ACQUIRE)) {
			Current = Current->Next ? Current->Next : Share->Head;
			if (Current == Expected) return NULL;
		}
		xmqttserverlink* Next = Current->Next ? Current->Next : Share->Head;
		if (xrtAtomicPtrCompareExchange(&Share->Cursor, &Expected, Next, XMEMORY_RELAXED, XMEMORY_RELAXED))
			return Current;
	}
}

static bool xmqttServerCollectFilter(xmqttservertargets* Targets, const xmqttservertopic* Filter, uint64 Origin)
{
	for (xmqttserverlink* Link = Filter ? Filter->Head : NULL; Link; Link = Link->Next) {
		if (xrtAtomic32Load(&Link->Session->Unavailable, XMEMORY_ACQUIRE) ||
			((Link->Options & XMQTT_SUBSCRIBE_NO_LOCAL) && Link->Session->Identity == Origin)) continue;
		if (!xmqttServerAddTarget(Targets, Link)) return false;
	}
	return true;
}

static bool xmqttServerCollectWildcard(xmqttservertargets* Targets, const xmqttservershard* Shard,
	xbytesview Name, uint64 Origin)
{
	struct Branch { const xmqttserverwildnode* Node; xbytesview Tail; };
	struct Branch Deferred[XMQTT_SERVER_MAX_WILDCARD_LEVELS];
	size_t Depth = 0;
	struct Branch Current = {&Shard->WildcardRoot, Name};
	for (;;) {
		const xmqttserverwildnode* Hash = xmqttServerFindLevel(Current.Node, XRT_BYTES_LITERAL("#"));
		if (Hash && (!xmqttServerCollectFilter(Targets, Hash->Topic, Origin) ||
			!xmqttServerCollectShares(Targets, Hash->Topic))) return false;
		if (!Current.Tail.Data) {
			if (!xmqttServerCollectFilter(Targets, Current.Node->Topic, Origin) ||
				!xmqttServerCollectShares(Targets, Current.Node->Topic)) return false;
		} else {
			xbytesview Level = xmqttServerLevel(&Current.Tail);
			const xmqttserverwildnode* Plus = xmqttServerFindLevel(Current.Node, XRT_BYTES_LITERAL("+"));
			const xmqttserverwildnode* Literal = xmqttServerFindLevel(Current.Node, Level);
			if (Plus && Literal) Deferred[Depth++] = (struct Branch){Plus, Current.Tail};
			Current.Node = Literal ? Literal : Plus;
			if (Current.Node) continue;
		}
		if (!Depth) return true;
		Current = Deferred[--Depth];
	}
}

typedef struct xmqttserverrouteiter {
	const xmqttservertargets* Targets;
	xmqttserverlink* Link;
	uint64 Origin;
	size_t Index;
} xmqttserverrouteiter;

static xmqttserverlink* xmqttServerRouteNext(xmqttserverrouteiter* Iter)
{
	if (Iter->Targets) {
		while (Iter->Index < Iter->Targets->Capacity) {
			xmqttserverlink* Link = Iter->Targets->Slots[Iter->Index++];
			if (Link) return Link;
		}
	} else {
		while (Iter->Link) {
			xmqttserverlink* Link = Iter->Link;
			Iter->Link = Link->Next;
			if (!xrtAtomic32Load(&Link->Session->Unavailable, XMEMORY_ACQUIRE) &&
				(!(Link->Options & XMQTT_SUBSCRIBE_NO_LOCAL) || Link->Session->Identity != Iter->Origin)) return Link;
		}
	}
	return NULL;
}

static uint8 xmqttServerRoute(xmqttserverpeer* Peer, const xmqttpacket* Packet, xmqttproperty* Expiry, double Now)
{
	xmqttserver* Server = Peer->Server;
	xstrview Name = Packet->Body.Publish.Topic;
	xbytesview Key = {(cbytes)Name.Data, Name.Size};
	uint64 Hash = xmqttServerIdHash(Key, &Server->HashKey);
	xmqttservershard* Exact = xmqttServerShard(Server, Hash);
	xmqttservershard *Wildcard = NULL, *Root = NULL;
	if (xrtAtomic32Load(&Server->WildcardFilters, XMEMORY_ACQUIRE)) {
		xmqttservershard* Candidate = xmqttServerWildcardShard(Server, Key);
		if (xrtAtomic32Load(&Candidate->WildcardFilters, XMEMORY_ACQUIRE)) Wildcard = Candidate;
		Candidate = Server->Shards + XMQTT_SERVER_ROOT_WILDCARDS;
		if (Key.Data[0] != '$' && xrtAtomic32Load(&Candidate->WildcardFilters, XMEMORY_ACQUIRE)) Root = Candidate;
	}
	/* Writers (and retirement) hold one shard. Publishers acquire up to
	 * three shared locks in index order, releasing in reverse order.
	 * The root wildcard shard permits parallel publishers, while writers
	 * wait for admission to release its borrowed connection references. */
	xmqttservershard* First = Wildcard && Wildcard < Exact ? Wildcard : Exact;
	xmqttservershard* Second = First == Exact ? Wildcard : Exact;
	if (First == Second) Second = NULL;
	xrtRWLockRead(&First->Gate);
	if (Second) xrtRWLockRead(&Second->Gate);
	if (Root) xrtRWLockRead(&Root->Gate);

	xmqttservertargets Targets;
	Targets.Slots = Targets.Local;
	Targets.Capacity = sizeof(Targets.Local) / sizeof(Targets.Local[0]);
	Targets.Count = 0;
	Targets.Shares = Targets.LocalShares;
	Targets.ShareCapacity = sizeof(Targets.LocalShares) / sizeof(Targets.LocalShares[0]);
	Targets.ShareCount = 0;
	Targets.Retain = (Packet->Flags & XMQTT_PUBLISH_RETAIN) != 0;
	const xmqttservertopic* Topic = xmqttServerFindKey(&Exact->Topics, Hash, Key);
	bool Rejected = false;
	unsigned SourceQos = (Packet->Flags & XMQTT_PUBLISH_QOS_MASK) >> 1;
	unsigned Modes = 3u * (SourceQos + 1u);
	uint8 Reason = 0x10; /* No independent delivery selected. */
	xmqttserverpublication* Frames[9] = {0};
	uint64 Forwarded = 0, Dropped = 0;
	if (Wildcard || Root) {
		memset(Targets.Local, 0, sizeof(Targets.Local));
		if ((Wildcard && !xmqttServerCollectWildcard(&Targets, Wildcard, Key, Peer->Session->Identity)) ||
			(Root && !xmqttServerCollectWildcard(&Targets, Root, Key, Peer->Session->Identity)) ||
			(Targets.Count && !xmqttServerCollectFilter(&Targets, Topic, Peer->Session->Identity))) {
			Reason = 0x97;
			Rejected = true;
			goto Done;
		}
	}
	if (!xmqttServerCollectShares(&Targets, Topic)) {
		Reason = 0x97;
		Rejected = true;
		goto Done;
	}
	if (!Topic && !Targets.Count && !Targets.ShareCount) goto Done;

	bool Rewrite = SourceQos != 0;
	double Expires = Expiry ? Now + Expiry->Value.Number : 0;
	if (Expiry) {
		double Elapsed = xrtTimer() - Now;
		if (Elapsed >= Expiry->Value.Number) {
			Rejected = true;
			goto Done;
		}
		if (Elapsed >= 1.0) {
			Expiry->Value.Number -= (uint32)Elapsed;
			Rewrite = true;
		}
	}
	xmqttserverrouteiter Start = {
		.Targets = Targets.Count ? &Targets : NULL, .Link = Topic ? Topic->Head : NULL, .Origin = Peer->Session->Identity
	};
	bool Needed[9] = {false};
	xmqttserverrouteiter Iter = Start;
	for (xmqttserverlink* Link; (Link = xmqttServerRouteNext(&Iter)) != NULL;)
		Needed[xmqttServerDeliveryMode(Link, Targets.Retain, SourceQos)] = true;
	for (size_t Index = 0; Index < Targets.ShareCount; ++Index) {
		xmqttserverlink* Member = xmqttServerShareMember(Targets.Shares[Index].Share);
		Targets.Shares[Index].Member = Member;
		if (Member) Needed[xmqttServerDeliveryMode(Member, Targets.Retain, SourceQos)] = true;
	}
	/* Prepare every wire form before native sends can invalidate the
	 * publisher's borrowed input on a synchronous transport failure. */
	for (unsigned Mode = 0; Mode < Modes; ++Mode) {
		if (!Needed[Mode]) continue;
		unsigned Base = Mode % 3u;
		uint8 Flags = (Base == 2 ? XMQTT_PUBLISH_RETAIN : 0) | ((Mode / 3u) << 1);
		Frames[Mode] = xmqttServerEncodePublish(Packet, Peer->Version,
			Base ? XMQTT_VERSION_5 : XMQTT_VERSION_311, Flags, Rewrite,
			Mode >= 3 ? Expires : 0, &Server->Config.Codec);
		if (SourceQos && !Frames[Mode]) {
			Reason = 0x97;
			Rejected = true;
			goto Done;
		}
		Reason = 0;
	}
	Iter = Start;
	for (xmqttserverlink* Link; (Link = xmqttServerRouteNext(&Iter)) != NULL;) {
		unsigned Mode = xmqttServerDeliveryMode(Link, Targets.Retain, SourceQos);
		if (Mode >= 3) Dropped += !xmqttServerQueueOutgoing(Link->Session->Peer, Frames[Mode]);
		else if (xmqttServerDeliver(Link->Session->Peer, Frames[Mode])) ++Forwarded;
		else ++Dropped;
	}
	for (size_t Index = 0; Index < Targets.ShareCount; ++Index) {
		xmqttserverlink* Member = Targets.Shares[Index].Member;
		if (!Member) continue;
		unsigned Mode = xmqttServerDeliveryMode(Member, Targets.Retain, SourceQos);
		if (Mode >= 3) Dropped += !xmqttServerQueueOutgoing(Member->Session->Peer, Frames[Mode]);
		else if (xmqttServerDeliver(Member->Session->Peer, Frames[Mode])) ++Forwarded;
		else ++Dropped;
	}

Done:
	if (Root) xrtRWLockReadUnlock(&Root->Gate);
	if (Second) xrtRWLockReadUnlock(&Second->Gate);
	xrtRWLockReadUnlock(&First->Gate);
	if (Targets.Slots != Targets.Local) xrtFree(Targets.Slots);
	if (Targets.Shares != Targets.LocalShares) xrtFree(Targets.Shares);
	for (unsigned Mode = 0; Mode < Modes; ++Mode)
		if (Frames[Mode]) xmqttServerFreePublication(Frames[Mode], NULL, 0);
	if (Forwarded) xrtAtomic64FetchAdd(&Server->Forwarded, Forwarded, XMEMORY_RELAXED);
	if (Dropped) xrtAtomic64FetchAdd(&Server->DroppedDeliveries, Dropped, XMEMORY_RELAXED);
	if (Rejected) xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
	return Reason;
}

static int xmqttServerCompareAck(const void* Key, const xavlnode* Node, ptr User)
{
	(void)User;
	uint16 Identifier = *(const uint16*)Key, Current = ((const xmqttserverack*)Node)->Identifier;
	return (Identifier > Current) - (Identifier < Current);
}

/* Terminal replies hold receive credit until TCP retires their buffer.
 * Clean-session teardown also uses this path for a QoS 2 ID awaiting PUBREL. */
static void xmqttServerReleaseAck(ptr Data, cbytes Wire, size_t Size)
{
	(void)Wire;
	(void)Size;
	xmqttserverack* Ack = Data;
	xmqttserversession* Session = Ack->Session;
	if (xrtAVLRemove(&Session->Incoming, &Ack->Identifier, xmqttServerCompareAck, NULL) != &Ack->Node) abort();
	if (Ack == &Session->InlineAck) Ack->Identifier = 0;
	else xrtFree(Ack);
	xrtAtomic32FetchSub(&Session->Server->IncomingPublishes, 1, XMEMORY_RELAXED);
	xmqttServerPeerDrop(Session->Peer);
}

static xmqttserverack* xmqttServerReserveAck(xmqttserverpeer* Peer, uint16 Identifier, uint8* Reason)
{
	xmqttserver* Server = Peer->Server;
	if (xrtAVLFind(&Peer->Session->Incoming, &Identifier, xmqttServerCompareAck, NULL)) {
		*Reason = 0x82;
		return NULL;
	}
	if (Peer->Session->Incoming.Count >= Server->Config.ReceiveMaximum) {
		*Reason = 0x93;
		return NULL;
	}
	*Reason = 0x97;
	if (!xmqttServerReserve(&Server->IncomingPublishes, Server->Config.MaxIncomingPublishes)) return NULL;
	xmqttserverack* Ack = Peer->Session->InlineAck.Identifier ? xrtMalloc(sizeof(*Ack)) : &Peer->Session->InlineAck;
	if (!Ack) {
		xrtAtomic32FetchSub(&Server->IncomingPublishes, 1, XMEMORY_RELAXED);
		return NULL;
	}
	Ack->Session = Peer->Session;
	Ack->Identifier = Identifier;
	Ack->Phase = XMQTT_SERVER_ACK_TERMINAL;
	xrtAVLNodeInit(&Ack->Node);
	if (xrtAVLInsert(&Peer->Session->Incoming, &Ack->Node, &Ack->Identifier, xmqttServerCompareAck, NULL, NULL) !=
		&Ack->Node) abort();
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	return Ack;
}

static void xmqttServerSendAck(xmqttserverpeer* Peer, xmqttserverack* Ack, uint8 Header, uint8 Reason)
{
	bool V5 = Peer->Version == XMQTT_VERSION_5;
	size_t Bytes = xmqttServerIdWire(Ack->Wire, Header, Ack->Identifier, V5 ? Reason : 0);
	if ((V5 || Reason < 0x80) && Bytes <= Peer->PacketLimit &&
		xrtNetStreamSendRef(Peer->Stream, Ack->Wire, Bytes, xmqttServerReleaseAck, Ack) == XNET_RESULT_OK)
		return;
	xmqttServerReleaseAck(Ack, NULL, 0);
	xmqttServerAbortPeer(Peer);
}

static void xmqttServerReceiveRel(xmqttserverpeer* Peer, uint16 Identifier)
{
	xmqttserverack* Ack = (xmqttserverack*)xrtAVLFind(&Peer->Session->Incoming, &Identifier, xmqttServerCompareAck, NULL);
	if (Ack && Ack->Phase == XMQTT_SERVER_ACK_AWAIT_PUBREL) {
		Ack->Phase = XMQTT_SERVER_ACK_PUBCOMP;
		xmqttServerSendAck(Peer, Ack, 0x70, 0);
	} else {
		uint8 Reason = Ack && Ack->Phase == XMQTT_SERVER_ACK_PUBCOMP ? 0 : 0x92;
		xmqttServerSendId(Peer, 0x70, Identifier, Reason);
	}
}

static void xmqttServerDeliveryAck(xmqttserverpeer* Peer, const xmqttpacket* Packet, double Now)
{
	uint16 Id = Packet->Identifier;
	xavlnode* Node = xrtAVLFind(&Peer->Session->Outgoing, &Id, xmqttServerCompareDelivery, NULL);
	xmqttserverdelivery* Delivery = (xmqttserverdelivery*)Node;
	if (!Delivery) {
		if (Packet->Type == XMQTT_PACKET_PUBREC && Packet->Reason < 0x80) xmqttServerSendId(Peer, 0x62, Id, 0x92);
		else if (Packet->Type == XMQTT_PACKET_PUBACK) xmqttServerProtocolClose(Peer, 0x82, Now);
		return;
	}
	if (Packet->Type == XMQTT_PACKET_PUBREC && Packet->Reason < 0x80 &&
		(Delivery->ExpectedAck == XMQTT_PACKET_PUBREC || Delivery->ExpectedAck == XMQTT_PACKET_PUBCOMP)) {
		Delivery->ExpectedAck = XMQTT_PACKET_PUBCOMP;
		xmqttServerSendId(Peer, 0x62, Id, 0);
		return;
	}
	if (Packet->Type != Delivery->ExpectedAck) {
		xmqttServerProtocolClose(Peer, 0x82, Now);
		return;
	}
	if (xrtAVLRemove(&Peer->Session->Outgoing, &Id, xmqttServerCompareDelivery, NULL) != Node) abort();
	xmqttServerDropDelivery(Delivery, NULL, 0);
	xmqttServerPumpOutgoing(Peer);
}

static void xmqttServerPublish(xmqttserverpeer* Peer, const xmqttpacket* Packet, xmqttproperty* Properties, double Now)
{
	xmqttserver* Server = Peer->Server;
	unsigned Qos = (Packet->Flags & XMQTT_PUBLISH_QOS_MASK) >> 1;
	if (Qos == 2) {
		xmqttserverack* Pending = (xmqttserverack*)xrtAVLFind(&Peer->Session->Incoming, &Packet->Identifier,
			xmqttServerCompareAck, NULL);
		if (Pending && Pending->Phase == XMQTT_SERVER_ACK_AWAIT_PUBREL) {
			xmqttServerSendId(Peer, 0x50, Packet->Identifier, Pending->Reason);
			return;
		}
	}
	bool Retain = (Packet->Flags & XMQTT_PUBLISH_RETAIN) != 0;
	if (Retain && !Server->Config.MaxRetainedTopics && Peer->Version == XMQTT_VERSION_5) {
		xmqttServerProtocolClose(Peer, 0x9a, Now);
		return;
	}

	xmqttproperty* Expiry = NULL;
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		if (Properties[Index].Id == XMQTT_PROPERTY_TOPIC_ALIAS) {
			xmqttServerProtocolClose(Peer, 0x94, Now);
			return;
		}
		if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Expiry = Properties + Index;
	}
	uint8 Reason = 0;
	xmqttserverack* Ack = NULL;
	if (Qos) {
		Ack = xmqttServerReserveAck(Peer, Packet->Identifier, &Reason);
		if (!Ack) {
			xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
			xmqttServerProtocolClose(Peer, Reason, Now);
			return;
		}
		Reason = 0;
	}

	xstrview Name = Packet->Body.Publish.Topic;
	if (Name.Size > Server->Config.MaxTopicBytes) Reason = 0x90;
	else Reason = xmqttServerTopicReason(Peer, Name, true);
	if (Reason) {
		xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
	} else {
		bool Stored = Retain && Server->Config.MaxRetainedTopics &&
			xmqttServerStoreRetained(Peer, Packet, Expiry, Now);
		if (Ack && Retain && !Stored) {
			xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
			Reason = 0x97;
		} else if (Expiry && !Expiry->Value.Number) {
			xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
		} else {
			Reason = xmqttServerRoute(Peer, Packet, Expiry, Now);
			/* A committed retained message is accepted even if the
			 * independent QoS 0 deliveries could not be allocated. */
			if (Stored && Reason >= 0x80) Reason = 0;
		}
	}
	if (!Ack) return;
	if (Qos == 2 && Reason < 0x80 && !Peer->Closing) {
		/* Own only the identifier after routing, before sending PUBREC.
		 * Duplicate PUBLISH packets then have no routing or retain effects. */
		Ack->Phase = XMQTT_SERVER_ACK_AWAIT_PUBREL;
		Ack->Reason = Reason;
		xmqttServerSendId(Peer, 0x50, Ack->Identifier, Reason);
	} else {
		xmqttServerSendAck(Peer, Ack, Qos == 2 ? 0x50 : 0x40, Reason);
	}
}

static void xmqttServerOnline(xmqttserverpeer* Peer, const xmqttpacket* Packet, xmqttproperty* Properties, double Now)
{
	if (Peer->KeepAliveMs) Peer->Deadline = Now + (double)Peer->KeepAliveMs * 0.001;
	switch (Packet->Type) {
	case XMQTT_PACKET_SUBSCRIBE:
	case XMQTT_PACKET_UNSUBSCRIBE:
		xmqttServerFilterRequest(Peer, Packet, Now);
		break;
	case XMQTT_PACKET_PUBLISH:
		xmqttServerPublish(Peer, Packet, Properties, Now);
		break;
	case XMQTT_PACKET_PUBACK:
	case XMQTT_PACKET_PUBREC:
	case XMQTT_PACKET_PUBCOMP:
		xmqttServerDeliveryAck(Peer, Packet, Now);
		break;
	case XMQTT_PACKET_PUBREL:
		xmqttServerReceiveRel(Peer, Packet->Identifier);
		break;
	case XMQTT_PACKET_DISCONNECT:
		xmqttServerFinishPeer(Peer, Now);
		break;
	default:
		xmqttServerProtocolClose(Peer, 0x82, Now);
		break;
	}
}
static void xmqttServerRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	xmqttserverpeer* Peer = Data;
	const xmqttcodecconfig* Codec = &Peer->Server->Config.Codec;
	size_t Available;
	while (!Peer->Closing && (Available = xrtNetBufSize(Buffer)) != 0) {
		if (Peer->Session && xrtAtomic32Load(&Peer->Session->Unavailable, XMEMORY_ACQUIRE)) {
			xmqttServerAbortPeer(Peer);
			return;
		}
		xnetspan Front;
		if (!xrtNetBufFront(Buffer, &Front)) {
			xmqttServerAbortPeer(Peer);
			return;
		}
		unsigned char Prefix[12];
		size_t Need = Peer->Version ? 5u : sizeof(Prefix);
		if (Need > Available) Need = Available;
		if (Front.Size < Need) {
			size_t Bytes = xrtNetBufPeek(Buffer, 0, Prefix, Need);
			Front = (xnetspan){Prefix, Bytes};
		}
		xbytesview Input = {Front.Data, Front.Size};
		if (!Peer->Version) {
			xmqttwirestatus Status = xmqttConnectVersionRead(Input, Codec->MaxPacketBytes, &Peer->Version);
			if (Status == XMQTT_WIRE_MORE) return;
			if (Status == XMQTT_WIRE_ERROR) {
				xmqttServerAbortPeer(Peer);
				return;
			}
		}
		const xmqttwireconfig Wire = {sizeof(Wire), Peer->Version, Codec->MaxPacketBytes};
		xmqttfixedheader Header;
		xmqttwirestatus Status = xmqttHeaderRead(Input, &Wire, &Header);
		if (Status == XMQTT_WIRE_MORE) return;
		if (Status == XMQTT_WIRE_ERROR) {
			xmqttServerAbortPeer(Peer);
			return;
		}
		if (Available < Header.PacketBytes) return;
		if (!Peer->Online && Peer->Version == XMQTT_VERSION_311 && !Peer->Server->Config.Allow311) {
			xmqttServerRefuse(Peer, 0, 1, XMQTT_PACKET_MAX_BYTES, xrtTimer());
			return;
		}
		if (Peer->Online && Header.Type == XMQTT_PACKET_PINGREQ && !Header.RemainingLength) {
			static const unsigned char Pong[] = {0xd0, 0x00};
			if (Peer->KeepAliveMs) {
				double Now = xrtTimer();
				if (!isfinite(Now) || Now >= Peer->Deadline) {
					xmqttServerAbortPeer(Peer);
					return;
				}
				Peer->Deadline = Now + (double)Peer->KeepAliveMs * 0.001;
			}
			if (xrtNetStreamSend(Stream, Pong, sizeof(Pong)) != XNET_RESULT_OK) {
				xmqttServerAbortPeer(Peer);
				return;
			}
		} else {
			if (!xrtNetBufPullup(Buffer, Header.PacketBytes, &Front)) {
				xmqttServerAbortPeer(Peer);
				return;
			}
			Input = (xbytesview){Front.Data, Header.PacketBytes};
			xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
			xmqttsubscription Filters[XMQTT_SERVER_MAX_FILTERS];
			const xmqttcodecbuffers Scratch = {
				Properties, XMQTT_SERVER_MAX_PROPERTIES, Filters, XMQTT_SERVER_MAX_FILTERS
			};
			xmqttpacket Packet;
			xmqttcodecstatus Decoded;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
			if (Peer->Version == XMQTT_VERSION_311)
				Decoded = xmqttPacketRead311(Input, XMQTT_CLIENT_TO_SERVER, Codec, &Scratch, &Packet, NULL);
			else
#endif
				Decoded = xmqttPacketRead5(Input, XMQTT_CLIENT_TO_SERVER, Codec, &Scratch, &Packet, NULL);
			double Now = xrtTimer();
			if (Decoded != XMQTT_CODEC_DONE || !isfinite(Now) || (Peer->Timer && Now >= Peer->Deadline)) {
				xmqttServerAbortPeer(Peer);
				return;
			}
			if (!Peer->Online && Packet.Type == XMQTT_PACKET_CONNECT)
				xmqttServerConnect(Peer, &Packet, Now);
			else if (Peer->Online)
				xmqttServerOnline(Peer, &Packet, Properties, Now);
			else
				xmqttServerAbortPeer(Peer);
		}
		xrtNetBufConsume(Buffer, Header.PacketBytes);
	}
}

static void xmqttServerEnd(xnetstream* Stream, ptr Data)
{
	(void)Stream;
	xmqttserverpeer* Peer = Data;
	if (!Peer->Closing) xmqttServerAbortPeer(Peer);
}

static void xmqttServerDrain(xnetstream* Stream, ptr Data)
{
	(void)Stream;
	xmqttServerPumpOutgoing(Data);
}

static void xmqttServerClosed(xnetstream* Stream, xnetresult Result, const xerror* Cause, ptr Data)
{
	(void)Stream;
	(void)Result;
	(void)Cause;
	xmqttserverpeer* Peer = Data;
	Peer->Closing = true;
	xrtAtomic32Store(&Peer->OutgoingStopped, 1, XMEMORY_RELAXED);
	xmqttServerCancelTimer(Peer);
	xmqttserversession* Session = Peer->Session;
	if (!Session) return;
	xrtAtomic32Store(&Session->Unavailable, 1, XMEMORY_RELEASE);
	for (xavlnode* Node = xrtAVLFirst(&Session->Incoming); Node;) {
		xmqttserverack* Ack = (xmqttserverack*)Node;
		Node = xrtAVLUpperBound(&Session->Incoming, &Ack->Identifier, xmqttServerCompareAck, NULL);
		if (Ack->Phase == XMQTT_SERVER_ACK_AWAIT_PUBREL) xmqttServerReleaseAck(Ack, NULL, 0);
	}
	while (Session->Waiting) xmqttServerDiscardDelivery(xmqttServerPopWaiting(Session));
	while (Session->Outgoing.Count) {
		xmqttserverdelivery* Delivery = (xmqttserverdelivery*)xrtAVLFirst(&Session->Outgoing);
		xavlnode* Removed = xrtAVLRemove(&Session->Outgoing, &Delivery->Identifier, xmqttServerCompareDelivery, NULL);
		if (Removed != &Delivery->Node) abort();
		xmqttServerDropDelivery(Delivery, NULL, 0);
	}
}

static const xnetstreameventsownershipv1 xmqttServerEvents = {
	sizeof(xnetstreameventsownershipv1),
	{.Read = xmqttServerRead, .End = xmqttServerEnd, .Drain = xmqttServerDrain, .Close = xmqttServerClosed},
	xmqttServerPeerDrop, &xmqttServerPeerOps
};

XMQTT_API bool xmqttServerConfigInit(xmqttserverconfig* Config)
{
	if (!xrtMemRangeValid(Config, sizeof(*Config))) return xmqttServerError(XERR_ARGUMENT, "invalid server config");
	*Config = (xmqttserverconfig){
		.Size = sizeof(*Config), .MaxConnections = 1024, .MaxClientIdBytes = 256, .ConnectTimeout = 5000,
		.MaxSubscriptions = 65536, .MaxSubscriptionsPerConnection = 64,
		.MaxSubscribersPerTopic = 1024, .MaxTopicBytes = 1024, .MaxWildcardNodes = 65536,
		.MaxRetainedTopics = 4096, .MaxRetainedBytes = 16u * 1024u * 1024u,
		.ReceiveMaximum = 64, .MaxIncomingPublishes = 65536,
		.MaxOutgoingPublishes = 16384, .MaxOutgoingPublishesPerConnection = 256, .MaxOutgoingPublishBytes = 64u * 1024u * 1024u
	};
	xmqttCodecConfigInit(&Config->Codec);
	Config->Codec.MaxPacketBytes = 16384;
	Config->Codec.MaxProperties = XMQTT_SERVER_MAX_PROPERTIES;
	Config->Codec.MaxTopics = XMQTT_SERVER_MAX_FILTERS;
	return true;
}

XMQTT_API bool xmqttServerCreate(xnetengine* Engine, const xmqttserverconfig* Config, xmqttserver** Out)
{
	if (!Engine || !xrtMemRangeValid(Config, sizeof(*Config)) || !xrtMemRangeValid(Out, sizeof(*Out)) ||
		xrtMemRangesOverlap(Config, sizeof(*Config), Out, sizeof(*Out)) ||
		Config->Size != sizeof(*Config) || !Config->MaxConnections || Config->MaxConnections > UINT32_MAX ||
		!Config->MaxClientIdBytes || Config->MaxClientIdBytes > UINT16_MAX ||
		!Config->MaxSubscriptions || Config->MaxSubscriptions > UINT32_MAX ||
		!Config->MaxSubscriptionsPerConnection || !Config->MaxSubscribersPerTopic ||
		Config->MaxSubscribersPerTopic >= UINT32_MAX || !Config->MaxTopicBytes || Config->MaxTopicBytes > UINT16_MAX ||
		Config->MaxWildcardNodes > UINT32_MAX || Config->MaxRetainedTopics > UINT32_MAX ||
		!Config->ReceiveMaximum || !Config->MaxIncomingPublishes || Config->MaxIncomingPublishes > UINT32_MAX ||
		!Config->MaxOutgoingPublishes || Config->MaxOutgoingPublishes > UINT32_MAX || !Config->MaxOutgoingPublishBytes ||
		!Config->MaxOutgoingPublishesPerConnection || Config->MaxOutgoingPublishesPerConnection > UINT16_MAX ||
		Config->ConnectTimeout < 1 || Config->ConnectTimeout > INT32_MAX ||
		Config->Codec.Size != sizeof(Config->Codec) || Config->Codec.MaxPacketBytes < 2 ||
		Config->Codec.MaxPacketBytes > XMQTT_PACKET_MAX_BYTES ||
		Config->Codec.MaxProperties > XMQTT_SERVER_MAX_PROPERTIES ||
		Config->Codec.MaxTopics > XMQTT_SERVER_MAX_FILTERS ||
		(Config->Codec.Utf8Flags & ~XMQTT_UTF8_REJECT_DISALLOWED))
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT server setup");
#if !defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Config->Allow311) return xmqttServerError(XERR_UNSUPPORTED, "MQTT 3.1.1 codec is unavailable");
#endif
	if (!xrtNetEnginePin(Engine)) return false;
	xmqttserver* Server = xrtCalloc(1, sizeof(*Server));
	if (!Server) {
		xrtNetEngineUnpin(Engine);
		return false;
	}
	Server->Engine = Engine;
	Server->Config = *Config;
	if (!Config->MaxRetainedBytes) Server->Config.MaxRetainedTopics = 0;
	xrtAtomic32Init(&Server->IncomingPublishes, 0);
	xrtAtomic32Init(&Server->OutgoingPublishes, 0);
	xrtAtomic64Init(&Server->OutgoingPublishBytes, 0);
	xrtAtomic64Init(&Server->OutgoingPublishOverloads, 0);
	xrtAtomic32Init(&Server->RetainedTopics, 0);
	xrtAtomic32Init(&Server->RetainedExpiring, 0);
	xrtAtomic64Init(&Server->RetainedBytes, 0);
	xrtAtomic64Init(&Server->DroppedRetained, 0);
	xrtAtomic32Init(&Server->Subscriptions, 0);
	xrtAtomic32Init(&Server->WildcardNodes, 0);
	xrtAtomic32Init(&Server->WildcardFilters, 0);
	xrtAtomic64Init(&Server->Forwarded, 0);
	xrtAtomic64Init(&Server->DroppedDeliveries, 0);
	xrtAtomic64Init(&Server->RejectedPublications, 0);
	bool Gate = xrtMutexInit(&Server->Gate);
	bool Cond = Gate && xrtCondInit(&Server->Drained);
	bool Map = Cond && xrtMapInit(&Server->Identities, sizeof(xmqttserversession*));
	if (Map && xrtSecureRandom(&Server->HashKey, sizeof(Server->HashKey)) &&
		xrtMapSetKeyPolicy(&Server->Identities, xmqttServerIdHash, xmqttServerIdEqual, &Server->HashKey) &&
		xmqttServerInitTopics(Server)) {
		*Out = Server;
		return true;
	}
	xmqttServerUnitTopics(Server);
	if (Map) xrtMapUnit(&Server->Identities);
	if (Cond) xrtCondUnit(&Server->Drained);
	if (Gate) xrtMutexUnit(&Server->Gate);
	xrtFree(Server);
	xrtNetEngineUnpin(Engine);
	return false;
}

XMQTT_API bool xmqttServerAttach(xmqttserver* Server, xnetstream* Stream)
{
	if (!Server || !Stream) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT server attachment");
	xnetworker* Worker = xrtNetStreamWorker(Stream);
	if (!Worker || xrtNetWorkerEngine(Worker) != Server->Engine || !xrtNetWorkerIsCurrent(Worker))
		return xmqttServerError(XERR_STATE, "MQTT attach requires the accepted stream worker");

	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing || Server->Info.Connections == Server->Config.MaxConnections) {
		++Server->Info.Refused;
		xrtMutexUnlock(&Server->Gate);
		return false;
	}
	xmqttserverpeer* Peer = xrtCalloc(1, sizeof(*Peer));
	if (!Peer) {
		++Server->Info.Refused;
		xrtMutexUnlock(&Server->Gate);
		return false;
	}
	xrtAtomic32Init(&Peer->References, 1);
	xrtAtomic32Init(&Peer->OutgoingStopped, 0);
	Peer->Server = Server;
	Peer->Stream = Stream;
	Peer->Worker = xrtNetWorkerIndex(Worker);
	double Now = xrtTimer();
	if (!xrtNetPostInit(&Peer->Retirement) || !xrtNetPostInit(&Peer->OutgoingOverload) ||
		!xmqttServerDeadline(Peer, Now, (unsigned)Server->Config.ConnectTimeout) ||
		!xrtNetStreamSetEventsOwnedV1(Stream, Peer, &xmqttServerEvents)) {
		xmqttServerCancelTimer(Peer);
		xrtFree(Peer);
		++Server->Info.Refused;
		xrtMutexUnlock(&Server->Gate);
		return false;
	}
	Peer->Next = Server->Peers;
	if (Peer->Next) Peer->Next->Previous = Peer;
	Server->Peers = Peer;
	++Server->Info.Connections;
	++Server->Info.Accepted;
	xrtMutexUnlock(&Server->Gate);
	return true;
}

XMQTT_API bool xmqttServerInfo(xmqttserver* Server, xmqttserverinfo* Info)
{
	if (!Server || !xrtMemRangeValid(Info, sizeof(*Info))) return xmqttServerError(XERR_ARGUMENT, "invalid server info");
	xrtMutexLock(&Server->Gate);
	*Info = Server->Info;
	xrtMutexUnlock(&Server->Gate);
	Info->Subscriptions = xrtAtomic32Load(&Server->Subscriptions, XMEMORY_RELAXED);
	Info->WildcardNodes = xrtAtomic32Load(&Server->WildcardNodes, XMEMORY_RELAXED);
	Info->IncomingPublishes = xrtAtomic32Load(&Server->IncomingPublishes, XMEMORY_RELAXED);
	Info->OutgoingPublishes = xrtAtomic32Load(&Server->OutgoingPublishes, XMEMORY_RELAXED);
	Info->OutgoingPublishBytes = (size_t)xrtAtomic64Load(&Server->OutgoingPublishBytes, XMEMORY_RELAXED);
	Info->OutgoingPublishOverloads = xrtAtomic64Load(&Server->OutgoingPublishOverloads, XMEMORY_RELAXED);
	Info->RetainedTopics = xrtAtomic32Load(&Server->RetainedTopics, XMEMORY_RELAXED);
	Info->RetainedBytes = (size_t)xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED);
	Info->DroppedRetained = xrtAtomic64Load(&Server->DroppedRetained, XMEMORY_RELAXED);
	Info->Forwarded = xrtAtomic64Load(&Server->Forwarded, XMEMORY_RELAXED);
	Info->DroppedDeliveries = xrtAtomic64Load(&Server->DroppedDeliveries, XMEMORY_RELAXED);
	Info->RejectedPublications = xrtAtomic64Load(&Server->RejectedPublications, XMEMORY_RELAXED);
	return true;
}

XMQTT_API bool xmqttServerClose(xmqttserver* Server)
{
	if (!Server) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT server");
	xrtMutexLock(&Server->Gate);
	if (!Server->Info.Closing) {
		Server->Info.Closing = true;
		for (xmqttserverpeer* Peer = Server->Peers; Peer; Peer = Peer->Next)
			xrtNetStreamAbort(Peer->Stream);
	}
	xrtMutexUnlock(&Server->Gate);
	return true;
}

XMQTT_API xwaitresult xmqttServerWaitFor(xmqttserver* Server, int64 Timeout)
{
	if (!Server || Timeout < XRT_WAIT_FOREVER) {
		xmqttServerError(XERR_ARGUMENT, "invalid MQTT server wait");
		return XWAIT_ERROR;
	}
	if (xrtNetEngineCurrent(Server->Engine)) {
		xmqttServerError(XERR_STATE, "cannot join MQTT server from an engine worker");
		return XWAIT_ERROR;
	}
	double Deadline = Timeout == XRT_WAIT_FOREVER ? INFINITY : __xrtWaitAfter(Timeout);
	xrtMutexLock(&Server->Gate);
	xwaitresult Result = XWAIT_OK;
	if (!Server->Info.Closing) {
		xmqttServerError(XERR_STATE, "close MQTT server before joining");
		Result = XWAIT_ERROR;
	}
	while (Result == XWAIT_OK && Server->Info.Connections) {
		int64 Remaining = __xrtWaitRemaining(Deadline);
		if (Remaining < XRT_WAIT_FOREVER) Result = XWAIT_ERROR;
		else if (!Remaining) Result = XWAIT_TIMEOUT;
		else Result = xrtCondWaitFor(&Server->Drained, &Server->Gate, Remaining);
	}
	if (Result == XWAIT_TIMEOUT && !Server->Info.Connections) Result = XWAIT_OK;
	xrtMutexUnlock(&Server->Gate);
	return Result;
}

XMQTT_API bool xmqttServerDestroy(xmqttserver* Server)
{
	if (!Server) return true;
	if (xmqttServerWaitFor(Server, 0) != XWAIT_OK)
		return xmqttServerError(XERR_STATE, "MQTT server still owns connections or is open");
	xmqttServerUnitTopics(Server);
	xrtMapUnit(&Server->Identities);
	xrtCondUnit(&Server->Drained);
	xrtMutexUnit(&Server->Gate);
	xrtNetEngineUnpin(Server->Engine);
	xrtFree(Server);
	return true;
}

#endif
