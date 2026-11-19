#include <xmqtt/server.h>
#include <xmqtt/server_auth.h>
#include <xmqtt/server_store.h>
#include <xmqtt/server_retained_commit.h>
#include <xmqtt/server_tls.h>
#include <xmqtt/server_ws.h>
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
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
typedef struct xmqttserverauthstate {
	xstrview ClientId, Method;
	ptr Exchange;
	unsigned char* Connack;
	size_t ConnackBytes, SessionPresentOffset, Rounds, Bytes;
	uint64 Timer;
	double Deadline;
	bool CleanStart;
	char Text[];
} xmqttserverauthstate;
#endif
typedef struct xmqttserverwill {
	struct xmqttserverwill* Next;
	double Due;
	uint64 Identity;
	size_t Bytes, ClientBytes;
	uint32 Delay;
	xmqttversion Version;
	unsigned char Wire[]; /* Canonical PUBLISH followed by the publisher's Client ID. */
} xmqttserverwill;
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
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	bool RetainedPending; /* Protected by RetainedGate. */
	xmqttserverretainedticket *WillHead, *WillTail;
#endif
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

enum { XMQTT_SERVER_ONLINE, XMQTT_SERVER_SEALED, XMQTT_SERVER_OFFLINE };

typedef struct xmqttserverack {
	xavlnode Node;
	xmqttserversession* Session;
	xmqttserverpeer* Peer; /* Only held while the acknowledgement owns native work. */
	uint16 Identifier;
	uint8 Phase, Reason;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	bool Durable;
#endif
	unsigned char Wire[5];
} xmqttserverack;

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
struct xmqttserversessionresetticket {
	xmqttserverpeer* Peer;
	xnetworker* Worker;
	xnetpost Completion;
	xmqttserverack* Ack; /* Null for fresh CONNECT. */
	xmqttserversessionresult Result;
};
#endif

struct xmqttserver {
	xnetengine* Engine;
	xmqttserverconfig Config;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	xmqttserverauthconfig Auth;
#endif
	xmutex Gate;
	xcond Drained;
	xsipkey HashKey;
	xmap Identities;
	xmqttservershard Shards[XMQTT_SERVER_TOPIC_SHARDS + 1u];
	size_t ShardCount;
	xatomic32 Subscriptions, WildcardNodes, WildcardFilters, RetainedTopics, RetainedExpiring, IncomingPublishes;
	xatomic32 OutgoingPublishes;
	xatomic64 Forwarded, DroppedDeliveries, RejectedPublications, RetainedBytes, DroppedRetained;
	xatomic64 OutgoingPublishBytes, OutgoingPublishOverloads, WillBytes;
	xmqttserverpeer* Peers;
	xmqttserversession* Offline;
	uint64 ExpiryTimer;
	size_t SessionCleanup;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	xmqttserverretainedsubmit RetainedSubmit;
	ptr RetainedUser;
	size_t RetainedPendingLimit;
	size_t RetainedWillWork; /* Protected by Gate. */
	xatomic64 RetainedPendingBytes;
	xatomic32 RetainedFailed;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	xmqttserversessionresetsubmit ResetSubmit;
	xmqttserversessionsubmit SessionSubmit;
	ptr SessionStoreUser;
	bool Checkpoint; /* Written once with the admission gate held. */
	bool SessionStoreFailed; /* Protected by Gate. */
#endif
	xmqttserverinfo Info;
};

/* The protocol state has an identity and lifetime distinct from its stream.
 * A topic shard pins the session through every published subscription link. */
struct xmqttserversession {
	xmqttserver* Server;
	xmqttserverpeer* Peer; /* Rebound under Server->Gate and Gate for persistent sessions. */
	xmqttserverpeer* Successor; /* A pending CONNECT pin owned by the registry. */
	xmqttserverwill* Will; /* Registry-owned while detached. */
	xmutex Gate; /* Serializes the offline queue and peer acquisition. */
	xmqttserversession *OfflineNext, **OfflinePrevious;
	double Expires;
	bool Persistent;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	bool ResetPending; /* Published and changed under the registry gate. */
#endif
	xmqttversion Version;
	xbytesview RegisteredKey; /* Borrows the registry key under Server->Gate. */
	xstrview ClientName;
	uint64 Identity;
	xatomic32 State;
	xmqttserverlink* Links;
	size_t LinkCount;
	xavl Incoming, Outgoing;
	xmqttserverack InlineAck;
	xmqttserverdelivery *Waiting, **WaitingTail;
	xmqttserverdelivery *OfflineQueue, **OfflineTail;
	xmqttserverdelivery *SentHead, *SentTail;
	xatomic32 OutgoingCount;
	uint64 Generation;
	uint16 LastIdentifier;
};

#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS) || defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
#define XMQTT_SERVER_QUEUED_OUTPUT
typedef struct xmqttserverwrite {
	struct xmqttserverwrite* Next;
	size_t Size, Written, Count;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	unsigned char* Frame;
#endif
	xnetref Parts[2];
	unsigned char Copy[];
} xmqttserverwrite;

typedef struct xmqttserveroutput {
	xmutex Gate;
	xnetpost Write;
	xmqttserverwrite *Head, **Tail;
	size_t Queued, Limit;
	bool Stopped, Finishing, Closed, Reading, Driving;
} xmqttserveroutput;
#endif

struct xmqttserverpeer {
	xatomic32 References;
	xmqttserver* Server;
	xnetstream* Stream;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	xmqttserverauthstate* Auth;
#endif
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	xmqttserveroutput* Output;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	xtlsstream* Tls;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	xwsstream* Ws;
	unsigned char* WsInput;
	size_t WsFirst, WsBytes, WsCapacity;
#endif
	xnetpost Retirement, OutgoingOverload, Takeover;
	xmqttserverpeer *Previous, *Next;
	xmqttserversession* Session;
	xmqttserversession* AwaitingSession; /* Resumable old state or reserved fresh state. */
	xatomic32 OutgoingStopped, TakenOver;
	xmqttserverdelivery* Replay;
	xmqttserverwill* Will;
	uint16 SendMaximum, InFlight;
	xatomic32 SessionExpiry;
	double ClosedAt, ResumeExpiry;
	bool Pumping, Resumed;
	size_t PacketLimit;
	uint32 Worker;
	xmqttversion Version;
	bool Online, Closing;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	xmqttserversessionticket* SessionCommit; /* Connection worker only. */
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	xmqttserverretainedticket* Retaining; /* Publishing worker only. */
#endif
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

static void xmqttServerFreeSession(xmqttserversession* Session);
static void xmqttServerPublishWill(xmqttserver* Server, xmqttserverwill* Will);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
static bool xmqttServerSubmitRetainedWill(xmqttserver* Server, const xmqttserverwill* Will,
	xstrview Client, const xmqttpacket* Packet, const xmqttproperty* Expiry);
#endif

static void xmqttServerDropWill(xmqttserver* Server, xmqttserverwill* Will)
{
	if (!Will) return;
	xrtAtomic64FetchSub(&Server->WillBytes, sizeof(*Will) + Will->Bytes + Will->ClientBytes, XMEMORY_RELAXED);
	xrtFree(Will);
}

static void xmqttServerFinishTakeover(xnetworker* Worker, ptr Data);
static void xmqttServerSessionTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);

/* The registry gate serializes list membership and expiry with CONNECT. */
static void xmqttServerUnlinkOffline(xmqttserver* Server, xmqttserversession* Session)
{
	if (!Session->OfflinePrevious) return;
	*Session->OfflinePrevious = Session->OfflineNext;
	if (Session->OfflineNext) Session->OfflineNext->OfflinePrevious = Session->OfflinePrevious;
	Session->OfflinePrevious = NULL;
	Session->OfflineNext = NULL;
	--Server->Info.OfflineSessions;
}

static void xmqttServerForgetSession(xmqttserver* Server, xmqttserversession* Session)
{
	xrtAtomic32Store(&Session->State, XMQTT_SERVER_SEALED, XMEMORY_RELEASE);
	xmqttServerUnlinkOffline(Server, Session);
	if (!Session->RegisteredKey.Size) return;
	if (!xrtMapRemove(&Server->Identities, Session->RegisteredKey)) abort();
	Session->RegisteredKey = (xbytesview){0};
	if (Session->Persistent) --Server->Info.PersistentSessions;
}

static void xmqttServerParkSession(xmqttserver* Server, xmqttserversession* Session)
{
	Session->OfflinePrevious = &Server->Offline;
	Session->OfflineNext = Server->Offline;
	if (Server->Offline) Server->Offline->OfflinePrevious = &Session->OfflineNext;
	Server->Offline = Session;
	++Server->Info.OfflineSessions;
}

static bool xmqttServerKeepsSessions(const xmqttserver* Server)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	if (Server->Checkpoint) return true;
#endif
	return !Server->Info.Closing;
}

static xmqttserversession* xmqttServerDetachOffline(xmqttserver* Server)
{
	xmqttserversession* Garbage = NULL;
	while (Server->Offline) {
		xmqttserversession* Session = Server->Offline;
		xmqttServerForgetSession(Server, Session);
		Session->OfflineNext = Garbage;
		Garbage = Session;
	}
	return Garbage;
}

static void xmqttServerFreeOffline(xmqttserversession* Garbage)
{
	while (Garbage) {
		xmqttserversession* Next = Garbage->OfflineNext;
		xmqttServerFreeSession(Garbage);
		Garbage = Next;
	}
}

static bool xmqttServerDrained(const xmqttserver* Server)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	if (Server->RetainedWillWork) return false;
#endif
	return !Server->Info.Connections && !Server->ExpiryTimer && !Server->SessionCleanup;
}

static void xmqttServerSignalDrained(xmqttserver* Server)
{
	if (Server->Info.Closing && xmqttServerDrained(Server))
		xrtCondBroadcast(&Server->Drained);
}

static void xmqttServerSessionTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	xmqttserver* Server = Data;
	xmqttserversession* Garbage = NULL;
	xmqttserverwill* Due = NULL;
	xrtMutexLock(&Server->Gate);
	if (Server->ExpiryTimer != Id) abort();
	if (Result != XNET_RESULT_OK) {
		Server->ExpiryTimer = 0;
		xmqttServerSignalDrained(Server);
		xrtMutexUnlock(&Server->Gate);
		return;
	}
	if (Server->Info.Closing) {
		/* Close can still finish when its asynchronous cancellation
		 * could not allocate or the command queue was full. */
		xrtMutexUnlock(&Server->Gate);
		if (!xrtNetEngineTimerCancelCurrent(xrtNetWorkerEngine(Worker), Id)) abort();
		return;
	}
	double Now = xrtTimer();
	for (xmqttserversession* Session = Server->Offline; Session;) {
		xmqttserversession* Next = Session->OfflineNext;
		bool Remove = Session->Expires <= Now ||
			xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_SEALED;
		if (Session->Will && (Remove || Session->Will->Due <= Now)) {
			Session->Will->Next = Due;
			Due = Session->Will;
			Session->Will = NULL;
		}
		if (Remove) {
			xmqttServerForgetSession(Server, Session);
			Session->OfflineNext = Garbage;
			Garbage = Session;
		}
		Session = Next;
	}
	xrtMutexUnlock(&Server->Gate);
	while (Due) {
		xmqttserverwill* Next = Due->Next;
		xmqttServerPublishWill(Server, Due);
		Due = Next;
	}
	xmqttServerFreeOffline(Garbage);
}

static void xmqttServerAbortTransport(xmqttserverpeer* Peer)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) {
		xrtWsStreamAbort(Peer->Ws);
		return;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (Peer->Tls) {
		xrtTlsStreamAbort(Peer->Tls);
		return;
	}
#endif
	xrtNetStreamAbort(Peer->Stream);
}

#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
static void xmqttServerReleaseWrite(xmqttserverwrite* Write)
{
	for (size_t Index = 0; Index < Write->Count; ++Index) {
		const xnetref* Part = Write->Parts + Index;
		if (Part->Release) Part->Release(Part->Context, Part->Data, Part->Size);
	}
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	xrtFree(Write->Frame);
#endif
	xrtFree(Write);
}

/* No new submitter modifies the list after Stopped is set. */
static void xmqttServerDiscardWrites(xmqttserveroutput* Tls)
{
	while (Tls->Head) {
		xmqttserverwrite* Write = Tls->Head;
		Tls->Head = Write->Next;
		xmqttServerReleaseWrite(Write);
	}
}

static void xmqttServerFreeOutput(xmqttserveroutput* Tls)
{
	if (!Tls) return;
	if (Tls->Head) abort();
	xrtMutexUnit(&Tls->Gate);
	xrtFree(Tls);
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
static void xmqttServerAuthRelease(xmqttserverpeer* Peer)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	ptr Exchange = Auth->Exchange;
	Auth->Exchange = NULL;
	if (Exchange) Peer->Server->Auth.Release(Exchange, Peer->Server->Auth.User);
}

static void xmqttServerAuthFreeConnack(xmqttserverauthstate* Auth)
{
	if (!Auth->Connack) return;
	xrtSecureZero(Auth->Connack, Auth->ConnackBytes);
	xrtFree(Auth->Connack);
	Auth->Connack = NULL;
}
#endif

static void xmqttServerPeerRetire(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	if (Peer->Output) {
		xwaitresult Result;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
		if (Peer->Ws) Result = xrtWsStreamPollRetired(Peer->Ws);
		else
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
			Result = xrtTlsStreamPollRetired(Peer->Tls);
#else
			abort();
#endif
		if (Result != XWAIT_OK) {
			if (Result == XWAIT_ERROR || !xrtNetPost(Worker, &Peer->Retirement, xmqttServerPeerRetire, Peer)) abort();
			return;
		}
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	if (Peer->Auth) {
		xmqttServerAuthRelease(Peer);
		xmqttServerAuthFreeConnack(Peer->Auth);
		xrtFree(Peer->Auth);
	}
#endif
	xmqttserver* Server = Peer->Server;
	xmqttserversession* Session = Peer->Session;
	xmqttserverpeer* Successor = NULL;
	xmqttserverwill* Will = Peer->Will;
	xrtMutexLock(&Server->Gate);
	bool Closing = Server->Info.Closing;
	bool Publish = !Closing && Peer->Online;
	if (Will && Publish) Will->Identity = Session->Identity;
	if (Session) Successor = Session->Successor;
	if (Session && Session->RegisteredKey.Size) {
		--Server->Info.Established;
		if (Successor || (xmqttServerKeepsSessions(Server) &&
			xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_OFFLINE)) {
			xrtMutexLock(&Session->Gate);
			Session->Peer = NULL;
			uint32 Expiry = xrtAtomic32Load(&Peer->SessionExpiry, XMEMORY_RELAXED);
			Session->Expires = !Peer->Online && Peer->Resumed ? Peer->ResumeExpiry :
				(Expiry == UINT32_MAX ? INFINITY : Peer->ClosedAt + Expiry);
			if (Will && Publish && Will->Delay &&
				xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_OFFLINE) {
				Will->Due = fmin(Peer->ClosedAt + Will->Delay, Session->Expires);
				if (Will->Due > xrtTimer()) {
					if (Session->Will) abort();
					Session->Will = Will;
					Will = NULL;
				}
			}
			xrtMutexUnlock(&Session->Gate);
			if (!Successor) xmqttServerParkSession(Server, Session);
			Session = NULL;
		} else xmqttServerForgetSession(Server, Session);
	}
	if (Peer->Previous) Peer->Previous->Next = Peer->Next;
	else Server->Peers = Peer->Next;
	if (Peer->Next) Peer->Next->Previous = Peer->Previous;
	/* Charge the connection until its subscriptions and native stream retire. */
	xrtMutexUnlock(&Server->Gate);
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	xmqttServerFreeOutput(Peer->Output);
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) {
		xrtWsStreamDestroy(Peer->Ws);
		xrtFree(Peer->WsInput);
	} else
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (Peer->Tls) xrtTlsStreamDestroy(Peer->Tls);
	else
#endif
		xrtNetStreamDestroy(Peer->Stream);
	if (Session && !Closing && Session->Will) {
		xmqttServerPublishWill(Server, Session->Will);
		Session->Will = NULL;
	}
	if (Session) xmqttServerFreeSession(Session);
	if (Will && Publish) xmqttServerPublishWill(Server, Will);
	else xmqttServerDropWill(Server, Will);
	xrtFree(Peer);
	if (Successor && !xrtNetPost(xrtNetStreamWorker(Successor->Stream), &Successor->Takeover,
		xmqttServerFinishTakeover, Successor)) abort();
	xrtMutexLock(&Server->Gate);
	--Server->Info.Connections;
	xmqttServerSignalDrained(Server);
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

static void xmqttServerPumpOutgoing(xmqttserverpeer* Peer);

#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
static void xmqttServerWritePosted(xnetworker* Worker, ptr Data);

/* Run with the TLS queue gate held. */
static bool xmqttServerScheduleWrite(xmqttserverpeer* Peer)
{
	if (xrtNetPostPending(&Peer->Output->Write)) return true;
	if (!xmqttServerPeerTryRef(Peer)) return false;
	if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Peer->Output->Write, xmqttServerWritePosted, Peer))
		return true;
	xmqttServerPeerDrop(Peer);
	return false;
}

static xnetresult xmqttServerWriteNative(xmqttserverpeer* Peer, xmqttserverwrite* Write, size_t* Sent)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) {
		size_t Required;
		if (!xrtWsStreamSendSize(Peer->Ws, Write->Size, &Required) ||
			Required > xrtWsStreamWriteLimit(Peer->Ws)) return XNET_RESULT_ERROR;
		xnetresult Result;
		if (Write->Count == 1 && Write->Parts[0].Release) {
			Result = xrtWsStreamBinaryRef(Peer->Ws, Write->Parts);
			if (Result == XNET_RESULT_OK) Write->Parts[0].Release = NULL;
		} else if (Write->Count == 1) {
			Result = xrtWsStreamBinary(Peer->Ws, (xbytesview){Write->Parts[0].Data, Write->Size});
		} else {
			if (!Write->Frame) {
				Write->Frame = xrtMalloc(Write->Size);
				if (!Write->Frame) return XNET_RESULT_ERROR;
				memcpy(Write->Frame, Write->Parts[0].Data, Write->Parts[0].Size);
				memcpy(Write->Frame + Write->Parts[0].Size, Write->Parts[1].Data, Write->Parts[1].Size);
			}
			Result = xrtWsStreamSendTake(Peer->Ws, XWS_OPCODE_BINARY, Write->Frame, Write->Size);
			if (Result == XNET_RESULT_OK) Write->Frame = NULL;
		}
		if (Result == XNET_RESULT_OK) *Sent = Write->Size;
		return Result;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	xnetspan Spans[2];
	size_t Count = 0, Skip = Write->Written;
	for (size_t Index = 0; Index < Write->Count; ++Index) {
		xnetref Part = Write->Parts[Index];
		if (Skip >= Part.Size) {
			Skip -= Part.Size;
			continue;
		}
		Spans[Count++] = (xnetspan){Part.Data + Skip, Part.Size - Skip};
		Skip = 0;
	}
	xtlsresult Result = xrtTlsStreamSendVec(Peer->Tls, Spans, Count, Sent);
	return Result == XTLS_OK ? XNET_RESULT_OK :
		Result == XTLS_AGAIN ? XNET_RESULT_AGAIN : XNET_RESULT_ERROR;
#else
	abort();
#endif
}

static void xmqttServerCloseNative(xmqttserverpeer* Peer)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) {
		if (xrtWsStreamClose(Peer->Ws, 1000, (xstrview){0}) != XNET_RESULT_OK)
			xmqttServerAbortTransport(Peer);
		return;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (!xrtTlsStreamClose(Peer->Tls)) xmqttServerAbortTransport(Peer);
#else
	abort();
#endif
}

static void xmqttServerDriveOutput(xmqttserverpeer* Peer)
{
	xmqttserveroutput* Out = Peer->Output;
	if (Out->Driving) return;
	Out->Driving = true;
	xrtMutexLock(&Out->Gate);
	size_t Budget = Out->Queued;
	while (Budget && Out->Head && !Out->Closed) {
		xmqttserverwrite* Write = Out->Head;
		xrtMutexUnlock(&Out->Gate);
		size_t Sent = 0;
		xnetresult Result = xmqttServerWriteNative(Peer, Write, &Sent);
		xrtMutexLock(&Out->Gate);
		if (Out->Closed) break;
		if (Result != XNET_RESULT_OK && Result != XNET_RESULT_AGAIN) {
			xrtMutexUnlock(&Out->Gate);
			Out->Driving = false;
			xmqttServerAbortTransport(Peer);
			return;
		}
		Write->Written += Sent;
		if (Write->Written != Write->Size) break;
		Out->Head = Write->Next;
		if (!Out->Head) Out->Tail = &Out->Head;
		size_t Charge = sizeof(*Write) + Write->Size;
		Out->Queued -= Charge;
		Budget -= Charge;
		xrtMutexUnlock(&Out->Gate);
		xmqttServerReleaseWrite(Write);
		xrtMutexLock(&Out->Gate);
	}
	bool Finish = Out->Finishing && !Out->Head && !Out->Closed;
	if (Finish) Out->Finishing = false;
	bool Continue = Budget || !Out->Head || Out->Closed || xmqttServerScheduleWrite(Peer);
	xrtMutexUnlock(&Out->Gate);
	Out->Driving = false;
	if (Out->Closed) {
		xmqttServerDiscardWrites(Out);
		return;
	}
	if (!Continue) xmqttServerAbortTransport(Peer);
	else if (Finish) xmqttServerCloseNative(Peer);
	else xmqttServerPumpOutgoing(Peer);
}

static void xmqttServerWritePosted(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttServerDriveOutput(Data);
	xmqttServerPeerDrop(Data);
}

/* Transfer complete records only. The owner retires references after their
 * last bytes have entered TLS, or when the channel has physically closed. */
static xnetresult xmqttServerQueueOutput(xmqttserverpeer* Peer, const xnetref* Parts, size_t Count, bool Copy)
{
	xmqttserveroutput* Out = Peer->Output;
	size_t Size = Parts[0].Size + (Count == 2 ? Parts[1].Size : 0);
	size_t Charge = sizeof(xmqttserverwrite) + Size;
	if (Charge > Out->Limit) return XNET_RESULT_ERROR;
	xnetresult Result = XNET_RESULT_AGAIN;
	xrtMutexLock(&Out->Gate);
	if (Out->Stopped) Result = XNET_RESULT_CLOSED;
	else if (Charge <= Out->Limit - Out->Queued) {
		xmqttserverwrite* Write = xrtMalloc(sizeof(*Write) + (Copy ? Size : 0));
		Result = XNET_RESULT_ERROR;
		if (Write) {
			if (Out->Head || xmqttServerScheduleWrite(Peer)) {
				*Write = (xmqttserverwrite){.Size = Size, .Count = Count};
				memcpy(Write->Parts, Parts, Count * sizeof(*Parts));
				if (Copy) {
					memcpy(Write->Copy, Parts[0].Data, Size);
					Write->Parts[0].Data = Write->Copy;
				}
				*Out->Tail = Write;
				Out->Tail = &Write->Next;
				Out->Queued += Charge;
				Result = XNET_RESULT_OK;
			} else xrtFree(Write);
		}
	}
	xrtMutexUnlock(&Out->Gate);
	return Result;
}
#endif

static xnetresult xmqttServerSend(xmqttserverpeer* Peer, const void* Data, size_t Bytes)
{
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	if (Peer->Output) {
		const xnetref Part = {.Data = Data, .Size = Bytes};
		return xmqttServerQueueOutput(Peer, &Part, 1, true);
	}
#endif
	return xrtNetStreamSend(Peer->Stream, Data, Bytes);
}

static xnetresult xmqttServerSendRefs(xmqttserverpeer* Peer, const xnetref* Parts, size_t Count)
{
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	if (Peer->Output) return xmqttServerQueueOutput(Peer, Parts, Count, false);
#endif
	return xrtNetStreamSendRefs(Peer->Stream, Parts, Count);
}

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
	xmqttServerAbortTransport(Peer);
}

static void xmqttServerTimedOut(xmqttserverpeer* Peer)
{
	xmqttserver* Server = Peer->Server;
	xrtMutexLock(&Server->Gate);
	++Server->Info.TimedOut;
	xrtMutexUnlock(&Server->Gate);
	xmqttServerAbortPeer(Peer);
}

static void xmqttServerTimeout(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	if (Peer->Timer == Id) {
		Peer->Timer = 0;
		double Now = xrtTimer();
		if (Result != XNET_RESULT_OK || Now >= Peer->Deadline || !isfinite(Now)) {
			if (Result == XNET_RESULT_OK) xmqttServerTimedOut(Peer);
			else xmqttServerAbortPeer(Peer);
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

#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
static void xmqttServerAuthCancelTimer(xmqttserverpeer* Peer);
#endif

static void xmqttServerFinishPeer(xmqttserverpeer* Peer, double Now)
{
	Peer->Closing = true;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	xmqttServerAuthCancelTimer(Peer);
#endif
	if (!xmqttServerDeadline(Peer, Now, (unsigned)Peer->Server->Config.ConnectTimeout)) {
		xmqttServerAbortTransport(Peer);
		return;
	}
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
	if (Peer->Output) {
		xmqttserveroutput* Tls = Peer->Output;
		xrtMutexLock(&Tls->Gate);
		Tls->Stopped = Tls->Finishing = true;
		bool Idle = !Tls->Head;
		xrtMutexUnlock(&Tls->Gate);
		if (Idle) xmqttServerDriveOutput(Peer);
		return;
	}
#endif
	xrtNetStreamClose(Peer->Stream);
}

static void xmqttServerConnack5(xmqttserverpeer* Peer, uint8 Reason, xmqttproperty* Limits, xmqttpacket* Reply)
{
	xmqttPacketInit(XMQTT_PACKET_CONNACK, Reply);
	Reply->Reason = Reason;
	if (Reason) return;
	Reply->SessionPresent = Peer->Resumed;
	const xmqttproperty Properties[] = {
		{XMQTT_PROPERTY_RECEIVE_MAXIMUM, XMQTT_PROPERTY_UINT16, {.Number = Peer->Server->Config.ReceiveMaximum}},
		{XMQTT_PROPERTY_RETAIN_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = Peer->Server->Config.MaxRetainedTopics != 0}},
		{XMQTT_PROPERTY_WILDCARD_AVAILABLE, XMQTT_PROPERTY_BYTE,
			{.Number = Peer->Server->Config.MaxWildcardNodes != 0}},
		{XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
		{XMQTT_PROPERTY_SHARED_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 1}},
		{XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE, XMQTT_PROPERTY_UINT32,
			{.Number = (uint32)Peer->Server->Config.Codec.MaxPacketBytes}},
	};
	memcpy(Limits, Properties, sizeof(Properties));
	Reply->Properties = (xmqttpropertylist){Limits, sizeof(Properties) / sizeof(Properties[0])};
}

static bool xmqttServerWriteConnack(xmqttserverpeer* Peer, uint8 Reason5, uint8 Reason311, size_t PacketLimit)
{
	unsigned char Wire[48];
	size_t Bytes;
	if (Peer->Version == XMQTT_VERSION_5) {
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
		xmqttserverauthstate* Auth = Peer->Auth;
		if (!Reason5 && Auth && Auth->Connack) {
			/* Session Present is known only after the identity is installed. */
			Auth->Connack[Auth->SessionPresentOffset] = Peer->Resumed;
			xnetresult Sent = xmqttServerSend(Peer, Auth->Connack, Auth->ConnackBytes);
			xmqttServerAuthFreeConnack(Auth);
			return Sent == XNET_RESULT_OK;
		}
#endif
		xmqttpacket Reply;
		xmqttproperty Limits[6];
		xmqttServerConnack5(Peer, Reason5, Limits, &Reply);
		if (!xmqttPacketWrite5(Wire, sizeof(Wire), &Reply, XMQTT_SERVER_TO_CLIENT, NULL, &Bytes))
			return false;
	} else {
		/* MQTT 3.1.1 CONNACK has a fixed four-byte wire form. The refusal
		 * can be written even when the 3.1.1 decoder is deselected. */
		Wire[0] = 0x20;
		Wire[1] = 0x02;
		Wire[2] = Peer->Resumed && !Reason311;
		Wire[3] = Reason311;
		Bytes = 4;
	}
	return Bytes <= PacketLimit && xmqttServerSend(Peer, Wire, Bytes) == XNET_RESULT_OK;
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

static void xmqttServerSessionInit(xmqttserversession* Session, xmqttserver* Server, xmqttserverpeer* Peer,
	xmqttversion Version, xstrview Name)
{
	Session->Server = Server;
	Session->Peer = Peer;
	Session->Version = Version;
	Session->ClientName = Name;
	xrtAVLInit(&Session->Incoming);
	xrtAVLInit(&Session->Outgoing);
	Session->WaitingTail = &Session->Waiting;
	Session->OfflineTail = &Session->OfflineQueue;
	Session->Generation = 1;
	xrtAtomic32Init(&Session->State, XMQTT_SERVER_ONLINE);
	xrtAtomic32Init(&Session->OutgoingCount, 0);
}

static xmqttserversession* xmqttServerNewSession(xmqttserver* Server, xmqttserverpeer* Peer,
	xmqttversion Version, xstrview Id, bool Persistent)
{
	xmqttserversession* Session = xrtCalloc(1, sizeof(*Session) + Id.Size);
	if (!Session || (Persistent && !xrtMutexInit(&Session->Gate))) {
		xrtFree(Session);
		return NULL;
	}
	Session->Persistent = Persistent;
	char* Name = (char*)(Session + 1);
	memcpy(Name, Id.Data, Id.Size);
	xmqttServerSessionInit(Session, Server, Peer, Version, (xstrview){Name, Id.Size});
	return Session;
}

static void xmqttServerProtocolClose(xmqttserverpeer* Peer, uint8 Reason, double Now);

static void xmqttServerCloseSuperseded(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	if (!Peer->Closing) xmqttServerProtocolClose(Peer, 0x8e, xrtTimer());
	xmqttServerPeerDrop(Peer);
}

/* Consumes the old stream pin. Queue the MQTT 5 notification on its owner. */
static void xmqttServerRequestTakeoverClose(xmqttserverpeer* Peer)
{
	if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Peer->Takeover, xmqttServerCloseSuperseded, Peer)) return;
	xmqttServerAbortTransport(Peer);
	xmqttServerPeerDrop(Peer);
}

/* Called with both the registry gate and the session gate held. */
static bool xmqttServerBindOffline(xmqttserverpeer* Peer, xmqttserversession* Session)
{
	if (xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) != XMQTT_SERVER_OFFLINE) return false;
	xmqttserver* Server = Peer->Server;
	xmqttServerUnlinkOffline(Server, Session);
	Session->Peer = Peer;
	Peer->Session = Session;
	Peer->Resumed = true;
	Peer->ResumeExpiry = Session->Expires;
	Session->Expires = 0;
	++Server->Info.Established;
	return true;
}

static void xmqttServerPauseInput(xmqttserverpeer* Peer)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) xrtWsStreamPause(Peer->Ws);
	else
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (!Peer->Tls)
#endif
		xrtNetStreamPause(Peer->Stream);
}

/* The registry gate serializes reservations with native owner retirement. */
static bool xmqttServerReserveSuccessor(xmqttserverpeer* Peer, xmqttserversession* Old,
	xmqttserversession* Awaiting)
{
	Old->Successor = Peer;
	Peer->AwaitingSession = Awaiting;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	xmqttServerPauseInput(Peer);
	return xmqttServerPeerTryRef(Old->Peer);
}

static uint8 xmqttServerClaimId(xmqttserverpeer* Peer, xstrview Id, bool CleanStart, double Now)
{
	xmqttserver* Server = Peer->Server;
	xbytesview Key = {(cbytes)Id.Data, Id.Size};
	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
		|| Server->SessionStoreFailed
#endif
	) {
		xrtMutexUnlock(&Server->Gate);
		return 0x88;
	}
	xmqttserversession** Entry = xrtMapGet(&Server->Identities, Key);
	xmqttserversession* Old = Entry ? *Entry : NULL;
	if (Old && (Old->Successor || (Old->Peer && Old->Peer->AwaitingSession)
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
		|| Old->ResetPending
#endif
	)) {
		xrtMutexUnlock(&Server->Gate);
		return 0x89;
	}
	if (Old && !CleanStart && Old->Persistent) {
		xmqttserverpeer* Previous = Old->Peer;
		bool Live = Previous && xrtAtomic32Load(&Previous->SessionExpiry, XMEMORY_RELAXED);
		bool Saved = !Previous && Old->Expires > Now;
		if ((Live || Saved) && Old->Version != Peer->Version) {
			xrtMutexUnlock(&Server->Gate);
			return 0x83;
		}
		xrtMutexLock(&Old->Gate);
		if (Live && xrtAtomic32Load(&Old->State, XMEMORY_ACQUIRE) != XMQTT_SERVER_SEALED) {
			/* Stop reads and routing on the old transport. While its worker
			 * retires, later ordinary messages join the offline FIFO. */
			xrtAtomic32Store(&Previous->TakenOver, 1, XMEMORY_RELEASE);
			uint32 State = XMQTT_SERVER_ONLINE;
			if (xrtAtomic32CompareExchange(&Old->State, &State, XMQTT_SERVER_OFFLINE,
				XMEMORY_ACQ_REL, XMEMORY_ACQUIRE) || State == XMQTT_SERVER_OFFLINE) {
				bool Notify = xmqttServerReserveSuccessor(Peer, Old, Old);
				xrtMutexUnlock(&Old->Gate);
				xrtMutexUnlock(&Server->Gate);
				if (Notify) xmqttServerRequestTakeoverClose(Previous);
				return 0;
			}
		}
		bool Bound = Saved && xmqttServerBindOffline(Peer, Old);
		xrtMutexUnlock(&Old->Gate);
		if (Bound) {
			xrtMutexUnlock(&Server->Gate);
			return 0;
		}
	}

	bool Persistent = xrtAtomic32Load(&Peer->SessionExpiry, XMEMORY_RELAXED) != 0;
	size_t Persisting = Server->Info.PersistentSessions - (Old && Old->Persistent ? 1u : 0u);
	if (Persistent && Persisting >= Server->Config.MaxPersistentSessions) {
		xrtMutexUnlock(&Server->Gate);
		return 0x97;
	}
	if (Persistent && !Server->ExpiryTimer) {
		/* Admission owns the future wakeup before the broker commits
		 * session state. Every tick continues to use this one credit. */
		Server->ExpiryTimer = xrtNetEngineEvery(Server->Engine, Peer->Worker, 1000,
			xmqttServerSessionTimer, Server);
		if (!Server->ExpiryTimer) {
			uint8 Reason = xrtErrorKind(xrtGetError()) == XERR_AGAIN ? 0x89 : 0x88;
			xrtMutexUnlock(&Server->Gate);
			return Reason;
		}
	}
	xmqttserversession* Session = xmqttServerNewSession(Server, Peer, Peer->Version, Id, Persistent);
	if (!Session) {
		xrtMutexUnlock(&Server->Gate);
		return 0x88;
	}
	if (!Entry) {
		Entry = xrtMapGetOrAdd(&Server->Identities, Key, NULL);
		if (!Entry) {
			if (Session->Persistent) xrtMutexUnit(&Session->Gate);
			xrtFree(Session);
			xrtMutexUnlock(&Server->Gate);
			return 0x88;
		}
	}
	xmqttserverpeer* Previous = Old ? Old->Peer : NULL;
	if (Old) {
		Old->RegisteredKey = (xbytesview){0};
		if (Previous) xrtAtomic32Store(&Previous->TakenOver, 1, XMEMORY_RELEASE);
		xrtAtomic32Store(&Old->State, XMQTT_SERVER_SEALED, XMEMORY_RELEASE);
		xmqttServerUnlinkOffline(Server, Old);
		if (Old->Persistent) --Server->Info.PersistentSessions;
	}
	Session->Identity = Old ? Old->Identity : xrtNetWorkerOperationId(xrtNetStreamWorker(Peer->Stream));
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	Session->ResetPending = Server->ResetSubmit || Server->SessionSubmit;
#endif
	if (Session->Persistent) ++Server->Info.PersistentSessions;
	if (!Previous) ++Server->Info.Established;
	if (!xrtMapStoredKey(&Server->Identities, Key, &Session->RegisteredKey)) abort();
	Peer->Session = Session;
	*Entry = Session;
	bool Notify = Previous && xmqttServerReserveSuccessor(Peer, Old, Session);
	xrtMutexUnlock(&Server->Gate);
	if (Notify) xmqttServerRequestTakeoverClose(Previous);
	else if (Old && !Previous) {
		if (Old->Will) {
			xmqttServerPublishWill(Server, Old->Will);
			Old->Will = NULL;
		}
		xmqttServerFreeSession(Old);
	}
	return 0;
}

static void xmqttServerActivateSession(xmqttserverpeer* Peer);
static bool xmqttServerCanRead(xmqttserverpeer* Peer);
static bool xmqttServerReadPackets(xmqttserverpeer* Peer, const xnetbuf* Buffer, xnetbuf* Writable);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
static void xmqttServerWsRead(xwsstream* Stream, xbytesview Data, ptr User);
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
static void xmqttServerTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data);
#endif

static void xmqttServerResumeInput(xmqttserverpeer* Peer)
{
	if (!xmqttServerCanRead(Peer)) return;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	if (Peer->Ws) {
		xmqttServerWsRead(Peer->Ws, (xbytesview){0}, Peer);
		if (xmqttServerCanRead(Peer) && !xrtWsStreamResume(Peer->Ws)) xmqttServerAbortPeer(Peer);
		return;
	}
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (Peer->Tls) {
		const xnetbuf* Buffer = xrtTlsStreamBuffer(Peer->Tls);
		if (Buffer) xmqttServerTlsRead(Peer->Tls, Buffer, Peer);
		return;
	}
#endif
	const xnetbuf* Buffer = xrtNetStreamBuffer(Peer->Stream);
	if (Buffer) (void)xmqttServerReadPackets(Peer, Buffer, NULL);
	if (xmqttServerCanRead(Peer) && !xrtNetStreamResume(Peer->Stream)) xmqttServerAbortPeer(Peer);
}

static void xmqttServerAcceptSession(xmqttserverpeer* Peer, double Now)
{
	if (Peer->Closing || xrtAtomic32Load(&Peer->TakenOver, XMEMORY_ACQUIRE)) return;
	if (!xmqttServerDeadline(Peer, Now, Peer->KeepAliveMs) ||
		!xmqttServerWriteConnack(Peer, 0, 0, Peer->PacketLimit)) {
		xmqttServerAbortPeer(Peer);
		return;
	}
	if (!Peer->Closing) {
		if (Peer->Resumed && Peer->Session->Will) {
			/* Binding removed this session from the registry's expiry sweep. */
			xmqttserverwill* Will = Peer->Session->Will;
			Peer->Session->Will = NULL;
			if (Will->Due <= Now) xmqttServerPublishWill(Peer->Server, Will);
			else xmqttServerDropWill(Peer->Server, Will);
		}
		Peer->Online = true;
		xmqttServerActivateSession(Peer);
	}
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
static void xmqttServerReleaseAck(ptr Data, cbytes Wire, size_t Size);
static void xmqttServerFinishPubrel(xmqttserverpeer* Peer, xmqttserverack* Ack);

static void xmqttServerSessionCommitCompleted(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserversessionticket* Ticket = Data;
	xmqttserverpeer* Peer = Ticket->Peer;
	xmqttserver* Server = Peer->Server;
	xmqttserverack* Ack = Ticket->Ack;
	xmqttserversessionresult Result = Ticket->Result;
	Peer->SessionCommit = NULL;
	xrtFree(Ticket);
	xrtMutexLock(&Server->Gate);
	if (Result == XMQTT_SERVER_SESSION_UNKNOWN) Server->SessionStoreFailed = true;
	bool Active = !Server->Info.Closing && !Server->SessionStoreFailed;
	xrtMutexUnlock(&Server->Gate);

	bool Resume = false;
	if (Result == XMQTT_SERVER_SESSION_UNKNOWN) xmqttServerClose(Server);
	double Now = xrtTimer();
	Active = Active && xmqttServerCanRead(Peer);
	if (Active && (!isfinite(Now) || (Peer->Timer && Now >= Peer->Deadline))) {
		xmqttServerTimedOut(Peer);
		Active = false;
	}
	if (Ack) {
		if (Result == XMQTT_SERVER_SESSION_COMMITTED) {
			if (Active) {
				xmqttServerFinishPubrel(Peer, Ack);
				Resume = true;
			} else xmqttServerReleaseAck(Ack, NULL, 0);
		} else if (Active) xmqttServerAbortPeer(Peer);
	} else if (Active) {
		if (Result == XMQTT_SERVER_SESSION_REJECTED)
			xmqttServerRefuse(Peer, 0x88, 3, Peer->PacketLimit, Now);
		else {
			xmqttServerAcceptSession(Peer, Now);
			if (Peer->Online) {
				xrtMutexLock(&Server->Gate);
				Peer->Session->ResetPending = false;
				xrtMutexUnlock(&Server->Gate);
				Resume = true;
			}
		}
	}
	if (Resume) xmqttServerResumeInput(Peer);
	xmqttServerPeerDrop(Peer);
}

XMQTT_API bool xmqttServerCompleteSessionCommit(xmqttserversessionticket* Ticket,
	xmqttserversessionresult Result)
{
	if (!Ticket || (Result != XMQTT_SERVER_SESSION_COMMITTED &&
		Result != XMQTT_SERVER_SESSION_REJECTED && Result != XMQTT_SERVER_SESSION_UNKNOWN))
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT session commit completion");
	Ticket->Result = Result;
	return xrtNetPost(Ticket->Worker, &Ticket->Completion, xmqttServerSessionCommitCompleted, Ticket);
}

XMQTT_API bool xmqttServerCompleteSessionReset(xmqttserversessionresetticket* Ticket,
	xmqttserversessionresetresult Result)
{
	return xmqttServerCompleteSessionCommit(Ticket, Result);
}

static void xmqttServerSubmitSessionCommit(xmqttserverpeer* Peer, xmqttserverack* Ack, double Now)
{
	xmqttserversessionticket* Ticket = xrtMalloc(sizeof(*Ticket));
	if (!Ticket || !xrtNetPostInit(&Ticket->Completion)) {
		xrtFree(Ticket);
		if (Ack) xmqttServerAbortPeer(Peer);
		else xmqttServerRefuse(Peer, 0x88, 3, Peer->PacketLimit, Now);
		return;
	}
	Ticket->Peer = Peer;
	Ticket->Worker = xrtNetStreamWorker(Peer->Stream);
	Ticket->Ack = Ack;
	Peer->SessionCommit = Ticket;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	xmqttServerPauseInput(Peer);
	xmqttserver* Server = Peer->Server;
	xstrview ClientId = Peer->Session->ClientName;
	bool Submitted = Server->SessionSubmit ?
		Server->SessionSubmit(Ticket, Ack ? XMQTT_SERVER_SESSION_FINISH_INCOMING_QOS2 : XMQTT_SERVER_SESSION_DISCARD,
			ClientId, Ack ? Ack->Identifier : 0, Server->SessionStoreUser) :
		Server->ResetSubmit(Ticket, ClientId, Server->SessionStoreUser);
	if (Submitted) return;
	Ticket->Result = XMQTT_SERVER_SESSION_REJECTED;
	xmqttServerSessionCommitCompleted(Ticket->Worker, Ticket);
}
#endif

static void xmqttServerAcceptConnect(xmqttserverpeer* Peer, double Now)
{
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	if (Peer->Session->ResetPending) {
		if (!Peer->Closing) xmqttServerSubmitSessionCommit(Peer, NULL, Now);
		return;
	}
#endif
	xmqttServerAcceptSession(Peer, Now);
}

static void xmqttServerFinishTakeover(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	xmqttserver* Server = Peer->Server;
	xmqttserversession* Saved = Peer->AwaitingSession;
	double Now = xrtTimer();
	xrtMutexLock(&Server->Gate);
	if (!Saved) abort();
	Peer->AwaitingSession = NULL;
	bool Closing = Server->Info.Closing;
	bool Accepting = !Peer->Closing && !Closing && isfinite(Now) && Now < Peer->Deadline;
	bool Bound = Accepting;
	if (Saved != Peer->Session) {
		if (Saved->Successor != Peer || Saved->Peer) abort();
		Saved->Successor = NULL;
		xmutex* Gate = &Saved->Gate;
		xrtMutexLock(Gate);
		Bound = Accepting && xmqttServerBindOffline(Peer, Saved);
		if (Bound) Saved = NULL;
		else if (xmqttServerKeepsSessions(Server) && (Saved->Expires > Now || (Closing && Saved->Will)) &&
			xrtAtomic32Load(&Saved->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_OFFLINE) {
			xmqttServerParkSession(Server, Saved);
			Saved = NULL;
		} else xmqttServerForgetSession(Server, Saved);
		xrtMutexUnlock(Gate);
	} else Saved = NULL;
	xrtMutexUnlock(&Server->Gate);

	if (Bound) {
		xmqttServerAcceptConnect(Peer, Now);
		if (Peer->Online) xmqttServerResumeInput(Peer);
	} else {
		if (Saved) {
			if (Saved->Will && !Closing) {
				xmqttServerPublishWill(Server, Saved->Will);
				Saved->Will = NULL;
			}
			xmqttServerFreeSession(Saved);
		}
		if (Accepting) xmqttServerRefuse(Peer, 0x88, 3, Peer->PacketLimit, Now);
		else if (!Peer->Closing) xmqttServerAbortPeer(Peer);
	}
	xmqttServerPeerDrop(Peer);
}

static uint8 xmqttServerTopicPermission(const xmqttserverconfig* Config, xmqttversion Version,
	xstrview Client, xstrview Topic, bool Publishing)
{
	if (!Config->AuthorizeTopic) return 0;
	xmqttserverpermission Permission = Config->AuthorizeTopic(Version, Client, Topic, Publishing, Config->User);
	if (Permission == XMQTT_SERVER_ALLOW) return 0;
	return Permission == XMQTT_SERVER_BUSY ? 0x97 : 0x87;
}

static uint8 xmqttServerPrepareWill(xmqttserverpeer* Peer, const xmqttconnectpacket* Connect)
{
	xmqttserver* Server = Peer->Server;
	if (Connect->WillTopic.Size > Server->Config.MaxTopicBytes) return 0x90;
	if (Connect->WillRetain && !Server->Config.MaxRetainedTopics) return 0x9a;
	uint8 Reason = xmqttServerTopicPermission(&Server->Config, Peer->Version,
		Connect->ClientId, Connect->WillTopic, true);
	if (Reason) return Reason;

	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	size_t Count = 0;
	uint32 Delay = 0;
	for (size_t Index = 0; Index < Connect->WillProperties.Count; ++Index) {
		const xmqttproperty* Property = &Connect->WillProperties.Data[Index];
		if (Property->Id == XMQTT_PROPERTY_WILL_DELAY) Delay = Property->Value.Number;
		else Properties[Count++] = *Property;
	}
	xmqttpacket Packet = {
		.Type = XMQTT_PACKET_PUBLISH, .Identifier = Connect->WillQos ? 1 : 0,
		.Flags = ((uint8)Connect->WillQos << 1) | (Connect->WillRetain ? XMQTT_PUBLISH_RETAIN : 0),
		.Properties = {Properties, Count}, .Body.Publish = {Connect->WillTopic, Connect->WillPayload}
	};
	const xmqttcodecconfig* Codec = &Server->Config.Codec;
	bool (*Size)(const xmqttpacket*, xmqttdirection, const xmqttcodecconfig*, size_t*) = xmqttPacketSize5;
	bool (*Write)(ptr, size_t, const xmqttpacket*, xmqttdirection, const xmqttcodecconfig*, size_t*) = xmqttPacketWrite5;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Peer->Version == XMQTT_VERSION_311) {
		Size = xmqttPacketSize311;
		Write = xmqttPacketWrite311;
	}
#endif
	size_t Bytes;
	if (!Size(&Packet, XMQTT_SERVER_TO_CLIENT, Codec, &Bytes)) return 0x83;
	size_t Charge = sizeof(xmqttserverwill) + Bytes + Connect->ClientId.Size;
	if (!xmqttServerReserveBytes(&Server->WillBytes, Server->Config.MaxWillBytes, Charge)) return 0x97;
	xmqttserverwill* Will = xrtMalloc(Charge);
	if (!Will) {
		xrtAtomic64FetchSub(&Server->WillBytes, Charge, XMEMORY_RELAXED);
		return 0x88;
	}
	*Will = (xmqttserverwill){.Version = Peer->Version, .Delay = Delay, .Bytes = Bytes,
		.ClientBytes = Connect->ClientId.Size};
	if (!Write(Will->Wire, Bytes, &Packet, XMQTT_SERVER_TO_CLIENT, Codec, NULL)) {
		xmqttServerDropWill(Server, Will);
		return 0x83;
	}
	memcpy(Will->Wire + Bytes, Connect->ClientId.Data, Connect->ClientId.Size);
	Peer->Will = Will;
	return 0;
}

static void xmqttServerInstallConnect(xmqttserverpeer* Peer, xstrview ClientId, bool CleanStart, double Now)
{
	uint8 Refusal = xmqttServerClaimId(Peer, ClientId, CleanStart, Now);
	if (Refusal) xmqttServerRefuse(Peer, Refusal, 3, Peer->PacketLimit, Now);
	else if (!Peer->AwaitingSession) xmqttServerAcceptConnect(Peer, Now);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
static void xmqttServerAuthCancelTimer(xmqttserverpeer* Peer)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	if (!Auth || !Auth->Timer) return;
	uint64 Id = Auth->Timer;
	Auth->Timer = 0;
	xrtNetEngineTimerCancelCurrent(Peer->Server->Engine, Id);
}

static void xmqttServerAuthTimeout(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	if (Peer->Auth->Timer == Id) {
		Peer->Auth->Timer = 0;
		if (!Peer->Closing) {
			if (Result == XNET_RESULT_OK) xmqttServerTimedOut(Peer);
			else xmqttServerAbortPeer(Peer);
		}
	}
	xmqttServerPeerDrop(Peer);
}

static bool xmqttServerAuthStartTimer(xmqttserverpeer* Peer, double Now)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	int64 Timeout = Peer->Server->Auth.ReauthTimeout;
	Auth->Deadline = Now + (double)Timeout * 0.001;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	Auth->Timer = xrtNetEngineAfter(Peer->Server->Engine, Peer->Worker, Timeout, xmqttServerAuthTimeout, Peer);
	if (Auth->Timer) return true;
	xmqttServerPeerDrop(Peer);
	return false;
}

static bool xmqttServerAuthCharge(xmqttserverpeer* Peer, size_t Bytes)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	if (Bytes > Peer->Server->Auth.MaxExchangeBytes - Auth->Bytes) return false;
	Auth->Bytes += Bytes;
	return true;
}

static uint8 xmqttServerAuthReply(xmqttserverpeer* Peer, bool Accepted, xbytesview Data)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	bool Connack = Accepted && !Peer->Online;
	xmqttpacket Reply;
	xmqttproperty Properties[8];
	if (Connack) xmqttServerConnack5(Peer, 0, Properties, &Reply);
	else {
		xmqttPacketInit(XMQTT_PACKET_AUTH, &Reply);
		Reply.Reason = Accepted ? 0 : 0x18;
	}
	size_t Count = Reply.Properties.Count;
	Properties[Count++] = (xmqttproperty){XMQTT_PROPERTY_AUTH_METHOD, XMQTT_PROPERTY_STRING, {.Text = Auth->Method}};
	if (Data.Data)
		Properties[Count++] = (xmqttproperty){XMQTT_PROPERTY_AUTH_DATA, XMQTT_PROPERTY_BINARY, {.Binary = Data}};
	Reply.Properties = (xmqttpropertylist){Properties, Count};
	size_t Bytes;
	if ((!Data.Data && Data.Size) ||
		!xmqttPacketSize5(&Reply, XMQTT_SERVER_TO_CLIENT, NULL, &Bytes)) return 0x83;
	if (Bytes > Peer->PacketLimit) return 0x95;
	if (!xmqttServerAuthCharge(Peer, Bytes)) return 0x97;
	unsigned char Local[128];
	unsigned char* Wire = Connack || Bytes > sizeof(Local) ? xrtMalloc(Bytes) : Local;
	if (!Wire) return 0x97;
	bool Ready = xmqttPacketWrite5(Wire, Bytes, &Reply, XMQTT_SERVER_TO_CLIENT, NULL, NULL);
	if (Ready && Connack) {
		uint32 Remaining;
		size_t Width;
		Ready = xmqttVbiRead((xbytesview){Wire + 1, Bytes - 1}, XMQTT_VERSION_5,
			&Remaining, &Width) == XMQTT_WIRE_DONE;
		if (Ready) {
			Auth->Connack = Wire;
			Auth->ConnackBytes = Bytes;
			Auth->SessionPresentOffset = 1 + Width;
			return 0;
		}
	}
	if (Ready) Ready = xmqttServerSend(Peer, Wire, Bytes) == XNET_RESULT_OK;
	xrtSecureZero(Wire, Bytes);
	if (Wire != Local) xrtFree(Wire);
	return Ready ? 0 : 0x83;
}

static void xmqttServerAuthReject(xmqttserverpeer* Peer, uint8 Reason, double Now)
{
	xmqttServerAuthCancelTimer(Peer);
	xmqttServerAuthRelease(Peer);
	if (Peer->Closing) return;
	if (Peer->Online) xmqttServerProtocolClose(Peer, Reason, Now);
	else xmqttServerRefuse(Peer, Reason, 5, Peer->PacketLimit, Now);
}

static void xmqttServerAuthStep(xmqttserverpeer* Peer, xmqttserverauthphase Phase,
	const xmqttconnectpacket* Connect, xbytesview Data, size_t Bytes, double Now)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	if (!xmqttServerAuthCharge(Peer, Bytes)) {
		xmqttServerAuthReject(Peer, 0x97, Now);
		return;
	}
	const xmqttserverauthconfig* Config = &Peer->Server->Auth;
	const xmqttserverauthrequest Request = {Phase, Auth->ClientId, Auth->Method, Data, Connect};
	xbytesview Reply = {0};
	xmqttserverauthdecision Decision = Config->Step(&Request, &Auth->Exchange, &Reply, Config->User);
	if (Peer->Closing) return;
	Now = xrtTimer();
	if (!isfinite(Now) || Now >= (Peer->Online ? Auth->Deadline : Peer->Deadline)) {
		xmqttServerTimedOut(Peer);
		return;
	}
	switch (Decision) {
	case XMQTT_SERVER_AUTH_CHALLENGE:
		if (Auth->Rounds == Config->MaxRounds) xmqttServerAuthReject(Peer, 0x97, Now);
		else {
			++Auth->Rounds;
			uint8 Reason = xmqttServerAuthReply(Peer, false, Reply);
			if (Reason) xmqttServerAuthReject(Peer, Reason, Now);
		}
		break;
	case XMQTT_SERVER_AUTH_ACCEPT: {
		uint8 Reason = xmqttServerAuthReply(Peer, true, Reply);
		if (Reason) {
			xmqttServerAuthReject(Peer, Reason, Now);
			break;
		}
		xmqttServerAuthCancelTimer(Peer);
		xmqttServerAuthRelease(Peer);
		if (!Peer->Online && !Peer->Closing) {
			Now = xrtTimer();
			if (!isfinite(Now) || Now >= Peer->Deadline) xmqttServerTimedOut(Peer);
			else xmqttServerInstallConnect(Peer, Auth->ClientId, Auth->CleanStart, Now);
		}
		break;
	}
	case XMQTT_SERVER_AUTH_UNSUPPORTED:
		xmqttServerAuthReject(Peer, Peer->Online ? 0x87 : 0x8c, Now);
		break;
	default:
		xmqttServerAuthReject(Peer, 0x87, Now);
		break;
	}
}

static void xmqttServerAuthConnect(xmqttserverpeer* Peer, const xmqttconnectpacket* Connect,
	xstrview Method, xbytesview Data, size_t Bytes, double Now)
{
	xmqttserverauthstate* Auth = xrtCalloc(1, sizeof(*Auth) + Connect->ClientId.Size + Method.Size);
	if (!Auth) {
		xmqttServerRefuse(Peer, 0x97, 3, Peer->PacketLimit, Now);
		return;
	}
	Peer->Auth = Auth;
	Auth->CleanStart = Connect->CleanStart;
	Auth->ClientId = (xstrview){Auth->Text, Connect->ClientId.Size};
	Auth->Method = (xstrview){Auth->Text + Connect->ClientId.Size, Method.Size};
	memcpy(Auth->Text, Connect->ClientId.Data, Connect->ClientId.Size);
	memcpy(Auth->Text + Connect->ClientId.Size, Method.Data, Method.Size);
	xmqttServerAuthStep(Peer, XMQTT_SERVER_AUTH_CONNECT, Connect, Data, Bytes, Now);
}

static void xmqttServerReadAuth(xmqttserverpeer* Peer, const xmqttpacket* Packet, size_t Bytes, double Now)
{
	xmqttserverauthstate* Auth = Peer->Auth;
	if (!Auth) {
		xmqttServerProtocolClose(Peer, 0x82, Now);
		return;
	}
	bool Starting = Peer->Online && !Auth->Timer;
	xstrview Method = {0};
	xbytesview Data = {0};
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		const xmqttproperty* Property = &Packet->Properties.Data[Index];
		if (Property->Id == XMQTT_PROPERTY_AUTH_METHOD) Method = Property->Value.Text;
		else if (Property->Id == XMQTT_PROPERTY_AUTH_DATA) Data = Property->Value.Binary;
	}
	if (Packet->Reason != (Starting ? 0x19 : 0x18) || !Method.Data ||
		Method.Size != Auth->Method.Size || memcmp(Method.Data, Auth->Method.Data, Method.Size)) {
		xmqttServerAuthReject(Peer, 0x82, Now);
		return;
	}
	if (Starting) {
		Auth->Rounds = Auth->Bytes = 0;
		if (!xmqttServerAuthStartTimer(Peer, Now)) {
			xmqttServerAuthReject(Peer, 0x97, Now);
			return;
		}
	}
	if (Peer->Online && Now >= Auth->Deadline) {
		xmqttServerTimedOut(Peer);
		return;
	}
	xmqttserverauthphase Phase = Starting ? XMQTT_SERVER_AUTH_REAUTHENTICATE : XMQTT_SERVER_AUTH_CONTINUE;
	xmqttServerAuthStep(Peer, Phase, NULL, Data, Bytes, Now);
}
#endif

static void xmqttServerConnect(xmqttserverpeer* Peer, const xmqttpacket* Packet, size_t WireBytes, double Now)
{
	xmqttserver* Server = Peer->Server;
	const xmqttconnectpacket* Connect = &Packet->Body.Connect;
	uint32 SessionExpiry = 0;
	uint16 SendMaximum = UINT16_MAX;
	size_t PacketLimit = XMQTT_PACKET_MAX_BYTES;
	const xmqttproperty* AuthMethod = NULL;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	xbytesview AuthData = {0};
#else
	(void)WireBytes;
#endif
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		const xmqttproperty* Property = &Packet->Properties.Data[Index];
		if (Property->Id == XMQTT_PROPERTY_SESSION_EXPIRY) SessionExpiry = Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE) PacketLimit = Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_RECEIVE_MAXIMUM) SendMaximum = (uint16)Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_AUTH_METHOD) AuthMethod = Property;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
		else if (Property->Id == XMQTT_PROPERTY_AUTH_DATA) AuthData = Property->Value.Binary;
#endif
	}
	if (!Connect->ClientId.Size || Connect->ClientId.Size > Server->Config.MaxClientIdBytes) {
		xmqttServerRefuse(Peer, 0x85, 2, PacketLimit, Now);
		return;
	}
	if (AuthMethod
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
		&& !Server->Auth.Step
#endif
	) {
		xmqttServerRefuse(Peer, 0x8c, 5, PacketLimit, Now);
		return;
	}
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	if (AuthMethod && WireBytes > Server->Auth.MaxExchangeBytes) {
		xmqttServerRefuse(Peer, 0x97, 3, PacketLimit, Now);
		return;
	}
#endif
	if (Peer->Version == XMQTT_VERSION_311 && !Connect->CleanStart) SessionExpiry = UINT32_MAX;
	if (SessionExpiry && !Server->Config.MaxPersistentSessions) {
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
	if (Connect->HasWill) {
		uint8 Reason = xmqttServerPrepareWill(Peer, Connect);
		if (Reason) {
			xmqttServerRefuse(Peer, Reason, Reason == 0x87 ? 5 : 3, PacketLimit, Now);
			return;
		}
	}
	/* Publish the initialized transport limits along with the session. */
	Peer->PacketLimit = PacketLimit;
	Peer->SendMaximum = SendMaximum;
	Peer->KeepAliveMs = (unsigned)Connect->KeepAlive * 1500u;
	xrtAtomic32Store(&Peer->SessionExpiry, SessionExpiry, XMEMORY_RELAXED);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	if (AuthMethod) {
		xmqttServerAuthConnect(Peer, Connect, AuthMethod->Value.Text, AuthData, WireBytes, Now);
		return;
	}
#endif
	xmqttServerInstallConnect(Peer, Connect->ClientId, Connect->CleanStart, Now);
}

static void xmqttServerRemoveRetained(xmqttserver* Server, xmqttservershard* Shard,
	xmqttserverretained* Retained);

static void xmqttServerClearRetained(xmqttserver* Server, xmqttservershard* Shard)
{
	while (Shard->Retained.Root)
		xmqttServerRemoveRetained(Server, Shard, (xmqttserverretained*)Shard->Retained.Root);
}

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
			xmqttServerClearRetained(Server, Shard);
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

static xmqttserverlink** xmqttServerFindLink(xmqttserversession* Session, xbytesview Key, xbytesview Name)
{
	for (xmqttserverlink** Place = &Session->Links; ; Place = &(*Place)->SessionNext) {
		xmqttserverlink* Link = *Place;
		if (!Link) return Place;
		if (xmqttServerIdEqual(Link->Topic->Key.Text, Key, NULL) &&
			(Link->Share ? Name.Size && xmqttServerIdEqual(Link->Share->Name, Name, NULL) : !Name.Size))
			return Place;
	}
}

static uint8 xmqttServerSubscribeSession(xmqttserversession* Session, xbytesview Key, xbytesview Name,
	uint8 Options, xbytesview* Replay)
{
	xmqttserver* Server = Session->Server;
	uint8 Grant = Options & XMQTT_SUBSCRIBE_QOS_MASK;
	uint8 Handling = Options & XMQTT_SUBSCRIBE_RETAIN_HANDLING_MASK;
	bool Retained = !Name.Size && Server->Config.MaxRetainedTopics;
	xmqttserverlink* Existing = *xmqttServerFindLink(Session, Key, Name);
	if (Existing) {
		xrtRWLockWrite(&Existing->Shard->Gate);
		Existing->Options = Options;
		if (Replay && Retained && !Handling) *Replay = Existing->Topic->Key.Text;
		xrtRWLockWriteUnlock(&Existing->Shard->Gate);
		return Grant;
	}
	if (Session->LinkCount >= Server->Config.MaxSubscriptionsPerConnection) return 0x97;

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
		.Session = Session, .Topic = Topic, .Shard = Shard, .Share = Share, .Next = *Head,
		.SessionNext = Session->Links, .Options = Options
	};
	if (Link->Next) Link->Next->Previous = Link;
	else if (Share) xrtAtomicPtrStore(&Share->Cursor, Link, XMEMORY_RELAXED);
	*Head = Link;
	++Topic->Count;
	Session->Links = Link;
	++Session->LinkCount;
	if (Replay && Retained && Handling != 0x20) *Replay = Topic->Key.Text;
	xrtRWLockWriteUnlock(&Shard->Gate);
	return Grant;
}

static uint8 xmqttServerSubscribe(xmqttserverpeer* Peer, xbytesview Key, xbytesview Name,
	uint8 Options, xbytesview* Replay)
{
	return xmqttServerSubscribeSession(Peer->Session, Key, Name, Options, Replay);
}

static bool xmqttServerUnsubscribe(xmqttserverpeer* Peer, xbytesview Key, xbytesview Name)
{
	xmqttserverlink** Place = xmqttServerFindLink(Peer->Session, Key, Name);
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
		xmqttServerSend(Peer, Wire, sizeof(Wire)) == XNET_RESULT_OK)
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
	if (Bytes > Peer->PacketLimit || xmqttServerSend(Peer, Wire, Bytes) != XNET_RESULT_OK)
		xmqttServerAbortPeer(Peer);
}

static uint8 xmqttServerTopicReason(xmqttserverpeer* Peer, xstrview Topic, bool Publishing)
{
	return xmqttServerTopicPermission(&Peer->Server->Config, Peer->Version, Peer->Session->ClientName, Topic, Publishing);
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
	if (!Encoded || Bytes > Peer->PacketLimit || xmqttServerSend(Peer, Wire, Bytes) != XNET_RESULT_OK) {
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
		xrtAtomic32Load(&Peer->Session->State, XMEMORY_ACQUIRE) != XMQTT_SERVER_ONLINE) return false;
	xrtAtomic32FetchAdd(&Publication->References, 1, XMEMORY_RELAXED);
	const xnetref Part = {Publication->Wire, Publication->Size, xmqttServerFreePublication, Publication};
	if (xmqttServerSendRefs(Peer, &Part, 1) == XNET_RESULT_OK) return true;
	xmqttServerFreePublication(Publication, NULL, 0);
	return false;
}

/* Posted deliveries share the immutable payload. Only the short prefix up
 * through Packet Identifier (and optional expiry) is private to a recipient. */
struct xmqttserverdelivery {
	xavlnode Node;
	xnetpost Post;
	xmqttserversession* Session;
	xmqttserverpeer* PostedPeer; /* Held from admission to the owner callback. */
	xmqttserverpublication* Publication;
	xmqttserverdelivery *Next, *SentPrevious, *SentNext;
	uint64 SentGeneration;
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
	xmqttserverpublication* Publication = Delivery->Publication;
	xrtAtomic64FetchSub(&Session->Server->OutgoingPublishBytes, Publication->Size, XMEMORY_RELAXED);
	xrtAtomic32FetchSub(&Session->Server->OutgoingPublishes, 1, XMEMORY_RELAXED);
	xrtAtomic32FetchSub(&Session->OutgoingCount, 1, XMEMORY_RELAXED);
	xmqttServerFreePublication(Publication, NULL, 0);
	xrtFree(Delivery);
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

static void xmqttServerPrependWaiting(xmqttserversession* Session, xmqttserverdelivery* Delivery)
{
	Delivery->Next = Session->Waiting;
	if (!Session->Waiting) Session->WaitingTail = &Delivery->Next;
	Session->Waiting = Delivery;
}

static void xmqttServerForgetDelivery(xmqttserverpeer* Peer, xmqttserverdelivery* Delivery)
{
	xmqttserversession* Session = Peer->Session;
	if (xrtAVLRemove(&Session->Outgoing, &Delivery->Identifier, xmqttServerCompareDelivery, NULL) !=
		&Delivery->Node) abort();
	if (Delivery->SentPrevious) Delivery->SentPrevious->SentNext = Delivery->SentNext;
	else Session->SentHead = Delivery->SentNext;
	if (Delivery->SentNext) Delivery->SentNext->SentPrevious = Delivery->SentPrevious;
	else Session->SentTail = Delivery->SentPrevious;
	if (Peer->Replay == Delivery) Peer->Replay = Delivery->SentNext;
	if (Delivery->SentGeneration == Session->Generation) --Peer->InFlight;
}

static void xmqttServerRememberDelivery(xmqttserversession* Session, xmqttserverdelivery* Delivery, uint16 Id)
{
	xmqttserverpublication* Publication = Delivery->Publication;
	Session->LastIdentifier = Delivery->Identifier = Id;
	Delivery->ExpectedAck = (Publication->Wire[0] & XMQTT_PUBLISH_QOS_MASK) == 4 ?
		XMQTT_PACKET_PUBREC : XMQTT_PACKET_PUBACK;
	Delivery->SentPrevious = Session->SentTail;
	Delivery->SentNext = NULL;
	if (Session->SentTail) Session->SentTail->SentNext = Delivery;
	else Session->SentHead = Delivery;
	Session->SentTail = Delivery;
	xrtAVLNodeInit(&Delivery->Node);
	if (xrtAVLInsert(&Session->Outgoing, &Delivery->Node, &Id, xmqttServerCompareDelivery, NULL, NULL) !=
		&Delivery->Node) abort();
}

static bool xmqttServerPrepareWaiting(xmqttserverdelivery* Delivery)
{
	xmqttserverpublication* Publication = Delivery->Publication;
	if (!Publication->Expires) return true;
	double Remaining = Publication->Expires - xrtTimer();
	if (!(Remaining > 0)) return false;
	if (Publication->ExpiryOffset) {
		const unsigned char* Original = Publication->Wire + Publication->ExpiryOffset;
		uint32 Seconds = 0;
		for (unsigned Byte = 0; Byte < 4; ++Byte) Seconds = (Seconds << 8u) | Original[Byte];
		if (Remaining < Seconds) Seconds = (uint32)ceil(Remaining);
		unsigned char* Value = Delivery->Prefix + Publication->ExpiryOffset;
		for (unsigned Byte = 0; Byte < 4; ++Byte) Value[Byte] = (uint8)(Seconds >> ((3u - Byte) * 8u));
	}
	return true;
}

static xnetresult xmqttServerSendDelivery(xmqttserverpeer* Peer, xmqttserverdelivery* Delivery)
{
	xmqttserverpublication* Publication = Delivery->Publication;
	const xnetref Parts[] = {
		{.Data = Delivery->Prefix, .Size = Delivery->PrefixBytes,
			.Release = xmqttServerDropDelivery, .Context = Delivery},
		{.Data = Publication->Wire + Delivery->PrefixBytes, .Size = Publication->Size - Delivery->PrefixBytes,
			.Release = xmqttServerDropDelivery, .Context = Delivery}
	};
	size_t Count = Parts[1].Size ? 2 : 1;
	Delivery->References += (unsigned)Count;
	xnetresult Result = xmqttServerSendRefs(Peer, Parts, Count);
	if (Result != XNET_RESULT_OK) Delivery->References -= (unsigned)Count;
	return Result;
}

static void xmqttServerPumpOutgoing(xmqttserverpeer* Peer)
{
	xmqttserversession* Session = Peer->Session;
	if (!Session || !Peer->Online || Peer->Pumping) return;
	Peer->Pumping = true;
	while ((Peer->Replay || Session->Waiting) && !Peer->Closing &&
		!xrtAtomic32Load(&Peer->OutgoingStopped, XMEMORY_RELAXED) &&
		xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_ONLINE) {
		bool Retry = Peer->Replay != NULL;
		bool Release = Retry && Peer->Replay->ExpectedAck == XMQTT_PACKET_PUBCOMP;
		if (!Release && Peer->InFlight >= Peer->SendMaximum) break;
		xmqttserverdelivery* Delivery = Retry ? Peer->Replay : xmqttServerPopWaiting(Session);
		xmqttserverpublication* Publication = Delivery->Publication;
		if (!Release && (Publication->Size > Peer->PacketLimit || (!Retry && !xmqttServerPrepareWaiting(Delivery)))) {
			if (Retry) xmqttServerForgetDelivery(Peer, Delivery);
			xmqttServerDiscardDelivery(Delivery);
			continue;
		}
		uint16 Id = Delivery->Identifier;
		xnetresult Result;
		if (Release) {
			unsigned char Wire[5];
			size_t Bytes = xmqttServerIdWire(Wire, 0x62, Id, 0);
			Result = xmqttServerSend(Peer, Wire, Bytes);
		} else {
			if (Retry) Delivery->Prefix[0] |= XMQTT_PUBLISH_DUP;
			else {
				Id = Session->LastIdentifier;
				do { if (!++Id) Id = 1; }
				while (xrtAVLFind(&Session->Outgoing, &Id, xmqttServerCompareDelivery, NULL));
				unsigned char* Identifier = Delivery->Prefix + Publication->IdOffset;
				Identifier[0] = (uint8)(Id >> 8);
				Identifier[1] = (uint8)Id;
			}
			Result = Publication->Size <= xrtNetStreamWriteLimit(Peer->Stream) ?
				xmqttServerSendDelivery(Peer, Delivery) : XNET_RESULT_ERROR;
		}
		if (Result != XNET_RESULT_OK) {
			if (!Retry) xmqttServerPrependWaiting(Session, Delivery);
			if (Result != XNET_RESULT_AGAIN && !Peer->Closing) xmqttServerAbortPeer(Peer);
			break;
		}
		if (Retry) Peer->Replay = Delivery->SentNext;
		else xmqttServerRememberDelivery(Session, Delivery, Id);
		if (!Release) {
			Delivery->SentGeneration = Session->Generation;
			++Peer->InFlight;
			xrtAtomic64FetchAdd(&Session->Server->Forwarded, 1, XMEMORY_RELAXED);
		}
	}
	Peer->Pumping = false;
}

static void xmqttServerActivateSession(xmqttserverpeer* Peer)
{
	if (!Peer->Resumed) return;
	xmqttserversession* Session = Peer->Session;
	xrtMutexLock(&Session->Gate);
	uint32 State = XMQTT_SERVER_OFFLINE;
	if (!xrtAtomic32CompareExchange(&Session->State, &State, XMQTT_SERVER_ONLINE, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
		xrtMutexUnlock(&Session->Gate);
		xmqttServerProtocolClose(Peer, 0x97, xrtTimer());
		return;
	}
	if (Session->OfflineQueue) {
		*Session->WaitingTail = Session->OfflineQueue;
		Session->WaitingTail = Session->OfflineTail;
		Session->OfflineQueue = NULL;
		Session->OfflineTail = &Session->OfflineQueue;
	}
	Session->Expires = 0;
	++Session->Generation;
	Peer->Replay = Session->SentHead;
	xrtMutexUnlock(&Session->Gate);
	xmqttServerPumpOutgoing(Peer);
}

static void xmqttServerPostDelivery(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverdelivery* Delivery = Data;
	xmqttserversession* Session = Delivery->Session;
	xmqttserverpeer* Peer = Delivery->PostedPeer;
	uint32 State = xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE);
	if (State == XMQTT_SERVER_SEALED || (Peer->Closing && !Session->Persistent)) {
		xmqttServerDiscardDelivery(Delivery);
	} else {
		*Session->WaitingTail = Delivery;
		Session->WaitingTail = &Delivery->Next;
		xmqttServerPumpOutgoing(Peer);
	}
	xmqttServerPeerDrop(Peer);
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
	if (Peer->Session->Persistent)
		xrtAtomic32Store(&Peer->Session->State, XMQTT_SERVER_SEALED, XMEMORY_RELEASE);
	if (!xrtAtomic32Exchange(&Peer->OutgoingStopped, 1, XMEMORY_ACQ_REL)) {
		xrtAtomic64FetchAdd(&Peer->Server->OutgoingPublishOverloads, 1, XMEMORY_RELAXED);
		if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Peer->OutgoingOverload,
			xmqttServerPostOverload, Peer)) return;
		xmqttServerAbortTransport(Peer);
	}
	xmqttServerPeerDrop(Peer);
}

static xmqttserverdelivery* xmqttServerNewDelivery(xmqttserversession* Session, xmqttserverpublication* Publication)
{
	xmqttserver* Server = Session->Server;
	const xmqttserverconfig* Config = &Server->Config;
	if (!xmqttServerReserve(&Session->OutgoingCount, Config->MaxOutgoingPublishesPerConnection)) return NULL;
	if (!xmqttServerReserve(&Server->OutgoingPublishes, Config->MaxOutgoingPublishes)) goto ReleaseSessionCredit;
	if (!xmqttServerReserveBytes(&Server->OutgoingPublishBytes, Config->MaxOutgoingPublishBytes, Publication->Size))
		goto ReleaseServerCredit;
	size_t Bytes = Publication->ExpiryOffset ? Publication->ExpiryOffset + 4u : Publication->IdOffset + 2u;
	xmqttserverdelivery* Delivery = xrtMalloc(sizeof(*Delivery) + Bytes);
	if (Delivery) {
		Delivery->Session = Session;
		Delivery->Publication = Publication;
		Delivery->Next = NULL;
		Delivery->SentGeneration = 0;
		Delivery->References = 1;
		Delivery->PrefixBytes = Bytes;
		memcpy(Delivery->Prefix, Publication->Wire, Bytes);
		xrtAtomic32FetchAdd(&Publication->References, 1, XMEMORY_RELAXED);
		return Delivery;
	}
	xrtAtomic64FetchSub(&Server->OutgoingPublishBytes, Publication->Size, XMEMORY_RELAXED);
ReleaseServerCredit:
	xrtAtomic32FetchSub(&Server->OutgoingPublishes, 1, XMEMORY_RELAXED);
ReleaseSessionCredit:
	xrtAtomic32FetchSub(&Session->OutgoingCount, 1, XMEMORY_RELAXED);
	return NULL;
}

/* Consumes the peer ref taken under a topic shard or the session gate. */
static bool xmqttServerPostOutgoing(xmqttserverpeer* Peer, xmqttserverpublication* Publication)
{
	if (Publication->Size > Peer->PacketLimit ||
		(!Peer->Session->Persistent && xrtAtomic32Load(&Peer->OutgoingStopped, XMEMORY_RELAXED))) {
		xmqttServerPeerDrop(Peer);
		return false;
	}
	xmqttserverdelivery* Delivery = NULL;
	if (Publication->Size <= xrtNetStreamWriteLimit(Peer->Stream))
		Delivery = xmqttServerNewDelivery(Peer->Session, Publication);
	if (Delivery) {
		Delivery->PostedPeer = Peer;
		if (!xrtNetPostInit(&Delivery->Post)) abort();
		if (xrtNetPost(xrtNetStreamWorker(Peer->Stream), &Delivery->Post, xmqttServerPostDelivery, Delivery))
			return true;
		xmqttServerDropDelivery(Delivery, NULL, 0);
	}
	xmqttServerStopOutgoing(Peer);
	return false;
}

static bool xmqttServerQueueOutgoing(xmqttserverpeer* Peer, xmqttserverpublication* Publication)
{
	return Publication && xrtAtomic32Load(&Peer->Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_ONLINE &&
		xmqttServerPeerTryRef(Peer) && xmqttServerPostOutgoing(Peer, Publication);
}

/* The topic shard pins the session. Acquire the peer while holding the
 * session gate, or enqueue directly while the native owner retires. */
static bool xmqttServerRouteDelivery(xmqttserversession* Session, xmqttserverpublication* Publication, bool Reliable)
{
	if (!Publication) return false;
	if (!Session->Persistent)
		return Reliable ? xmqttServerQueueOutgoing(Session->Peer, Publication) : xmqttServerDeliver(Session->Peer, Publication);
	xrtMutexLock(&Session->Gate);
	uint32 State = xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE);
	xmqttserverpeer* Peer = State == XMQTT_SERVER_ONLINE ? Session->Peer : NULL;
	if (Peer && xmqttServerPeerTryRef(Peer)) {
		xrtMutexUnlock(&Session->Gate);
		if (Reliable) return xmqttServerPostOutgoing(Peer, Publication);
		bool Sent = xmqttServerDeliver(Peer, Publication);
		xmqttServerPeerDrop(Peer);
		return Sent;
	}
	bool Queued = false;
	if (Reliable && State == XMQTT_SERVER_OFFLINE && (!Session->Expires || Session->Expires > xrtTimer())) {
		xmqttserverdelivery* Delivery = xmqttServerNewDelivery(Session, Publication);
		if (Delivery) {
			*Session->OfflineTail = Delivery;
			Session->OfflineTail = &Delivery->Next;
			Queued = true;
		} else {
			xrtAtomic32Store(&Session->State, XMQTT_SERVER_SEALED, XMEMORY_RELEASE);
			xrtAtomic64FetchAdd(&Session->Server->OutgoingPublishOverloads, 1, XMEMORY_RELAXED);
		}
	}
	xrtMutexUnlock(&Session->Gate);
	return Queued;
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
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	if (Shard->RetainedPending) return;
#endif
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

static void xmqttServerInstallRetained(xmqttserver* Server, xmqttservershard* Shard,
	xmqttserverretained* Entry, xmqttserverretained* Previous, xmqttserverpublication* Copy,
	uint64 Hash, xbytesview Key, xmqttversion Version, size_t ClientBytes, double Received, double Expires)
{
	if (Previous) {
		size_t PreviousBytes = Previous->Publication->Size + Previous->PublisherIdBytes;
		size_t Bytes = Copy->Size + ClientBytes;
		xmqttServerUnlinkExpiry(Server, Shard, Previous);
		xmqttServerFreePublication(Previous->Publication, NULL, 0);
		if (PreviousBytes > Bytes)
			xrtAtomic64FetchSub(&Server->RetainedBytes, PreviousBytes - Bytes, XMEMORY_RELAXED);
	} else {
		*Entry = (xmqttserverretained){0};
		xmqttServerInsertKey(&Shard->Retained, &Entry->Key, Hash, Key, (unsigned char*)(Entry + 1));
	}
	Entry->Publication = Copy;
	Entry->Version = Version;
	Entry->PublisherIdBytes = ClientBytes;
	Entry->Received = Received;
	Entry->Expires = Expires;
	if (Expires) {
		xrtAVLNodeInit(&Entry->ExpiryNode);
		if (xrtAVLInsert(&Shard->RetainedExpiry, &Entry->ExpiryNode, Entry,
			xmqttServerCompareExpiry, NULL, NULL) != &Entry->ExpiryNode) abort();
		xrtAtomic32FetchAdd(&Server->RetainedExpiring, 1, XMEMORY_RELAXED);
	}
}

static bool xmqttServerStoreRetainedAt(xmqttserver* Server, xmqttversion Version, xstrview Client,
	const xmqttpacket* Packet, const xmqttproperty* Expiry, double Now, double Age)
{
	xstrview Name = Packet->Body.Publish.Topic;
	xbytesview Key = {(cbytes)Name.Data, Name.Size};
	xmqttservershard* Shard = xmqttServerWildcardShard(Server, Key);
	uint64 Hash = xmqttServerIdHash(Key, &Server->HashKey);
	size_t Bytes = Packet->Wire.Size + Client.Size;
	double Remaining = Expiry ? Expiry->Value.Number - Age : 0;
	bool Store = Packet->Body.Publish.Payload.Size && (!Expiry || Remaining > 0);
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
		Copy = xmqttServerCopyPublication(Packet->Wire, Flags, Client);
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
	xmqttServerInstallRetained(Server, Shard, Entry, Previous, Copy, Hash, Key, Version, Client.Size,
		Now - Age, Expiry ? Now + Remaining : 0);
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
	return true;
}

static bool xmqttServerStoreRetained(xmqttserver* Server, xmqttversion Version, xstrview Client,
	const xmqttpacket* Packet, const xmqttproperty* Expiry, double Now)
{
	return xmqttServerStoreRetainedAt(Server, Version, Client, Packet, Expiry, Now, 0);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
XMQTT_API bool xmqttServerVisitRetained(xmqttserver* Server, xmqttserverretainedvisitor Visit, ptr User)
{
	if (!Server || !Visit) return xmqttServerError(XERR_ARGUMENT, "invalid retained checkpoint visit");
	if (xmqttServerWaitFor(Server, 0) != XWAIT_OK)
		return xmqttServerError(XERR_STATE, "drain MQTT broker before visiting retained state");
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	if (xrtAtomic32Load(&Server->RetainedFailed, XMEMORY_ACQUIRE))
		return xmqttServerError(XERR_STATE, "recover MQTT retained state from the authoritative backend");
#endif
	if (!Server->Config.MaxRetainedTopics) return true;
	double Now = xrtTimer();
	if (!isfinite(Now)) return xmqttServerError(XERR_STATE, "invalid MQTT retained checkpoint clock");
	for (size_t Index = 0; Index < XMQTT_SERVER_TOPIC_SHARDS; ++Index) {
		xavliter Iterator;
		xrtAVLIterBegin(&Server->Shards[Index].Retained, &Iterator);
		const xmqttserverretained* Entry;
		while ((Entry = (const xmqttserverretained*)xrtAVLIterNext(&Iterator)) != NULL) {
			uint64 Remaining = 0;
			if (Entry->Expires) {
				double Milliseconds = (Entry->Expires - Now) * 1000.0;
				if (Milliseconds < 1) continue;
				Remaining = (uint64)Milliseconds;
			}
			const xmqttserverpublication* Publication = Entry->Publication;
			const xmqttserverretainedrecord Record = {
				Entry->Version, {(cstr)(Publication->Wire + Publication->Size), Entry->PublisherIdBytes},
				{Publication->Wire, Publication->Size}, Remaining
			};
			if (!Visit(&Record, User)) return false;
		}
	}
	return true;
}

static bool xmqttServerRestoreRetainedRecord(xmqttserver* Server, const xmqttserverretainedrecord* Record,
	uint64 ElapsedMilliseconds, double Now)
{
	const xmqttcodecconfig* Codec = &Server->Config.Codec;
	if (!Record->PublisherId.Size || Record->PublisherId.Size > Server->Config.MaxClientIdBytes ||
		!xmqttUtf8StringCheck(Record->PublisherId, Codec->Utf8Flags, NULL))
		return xmqttServerError(XERR_PROTOCOL, "invalid retained checkpoint publisher");

	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
	xmqttpacket Packet;
	xmqttcodecstatus Decoded;
	if (Record->Version == XMQTT_VERSION_5)
		Decoded = xmqttPacketRead5(Record->Publish, XMQTT_CLIENT_TO_SERVER, Codec, &Scratch, &Packet, NULL);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	else if (Record->Version == XMQTT_VERSION_311 && Server->Config.Allow311)
		Decoded = xmqttPacketRead311(Record->Publish, XMQTT_CLIENT_TO_SERVER, Codec, &Scratch, &Packet, NULL);
#endif
	else return xmqttServerError(XERR_UNSUPPORTED, "unsupported retained checkpoint MQTT version");
	if (Decoded != XMQTT_CODEC_DONE || Packet.Type != XMQTT_PACKET_PUBLISH ||
		Packet.Wire.Size != Record->Publish.Size || !(Packet.Flags & XMQTT_PUBLISH_RETAIN) ||
		(Packet.Flags & XMQTT_PUBLISH_DUP) || !Packet.Body.Publish.Payload.Size ||
		Packet.Body.Publish.Topic.Size > Server->Config.MaxTopicBytes)
		return xmqttServerError(XERR_PROTOCOL, "invalid retained checkpoint PUBLISH");
	const xmqttproperty* Expiry = NULL;
	for (size_t Index = 0; Index < Packet.Properties.Count; ++Index) {
		if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Expiry = &Properties[Index];
		if (Properties[Index].Id == XMQTT_PROPERTY_TOPIC_ALIAS)
			return xmqttServerError(XERR_PROTOCOL, "retained checkpoint contains a topic alias");
	}
	double Age = 0;
	if (Expiry) {
		uint64 Original = UINT64_C(1000) * Expiry->Value.Number;
		if (!Record->RemainingMilliseconds || Record->RemainingMilliseconds > Original)
			return xmqttServerError(XERR_PROTOCOL, "invalid retained checkpoint expiry");
		if (ElapsedMilliseconds >= Record->RemainingMilliseconds) return true;
		Age = (double)(Original - Record->RemainingMilliseconds + ElapsedMilliseconds) / 1000.0;
	} else if (Record->RemainingMilliseconds)
		return xmqttServerError(XERR_PROTOCOL, "retained checkpoint expiry has no MQTT property");
	xbytesview Key = {(cbytes)Packet.Body.Publish.Topic.Data, Packet.Body.Publish.Topic.Size};
	xmqttservershard* Shard = xmqttServerWildcardShard(Server, Key);
	if (xmqttServerFindKey(&Shard->Retained, xmqttServerIdHash(Key, &Server->HashKey), Key))
		return xmqttServerError(XERR_PROTOCOL, "duplicate retained checkpoint topic");
	size_t Bytes = Packet.Wire.Size + Record->PublisherId.Size;
	if (Bytes > Server->Config.MaxRetainedBytes - xrtAtomic64Load(&Server->RetainedBytes, XMEMORY_RELAXED))
		return xmqttServerError(XERR_RANGE, "retained checkpoint exceeds byte limit");
	return xmqttServerStoreRetainedAt(Server, Record->Version, Record->PublisherId, &Packet, Expiry, Now, Age);
}

XMQTT_API bool xmqttServerRestoreRetained(xmqttserver* Server,
	const xmqttserverretainedrecord* Records, size_t Count, uint64 ElapsedMilliseconds)
{
	if (!Server || Count > SIZE_MAX / sizeof(*Records) ||
		(Count && !xrtMemRangeValid(Records, Count * sizeof(*Records))))
		return xmqttServerError(XERR_ARGUMENT, "invalid retained checkpoint records");
	if (Count > Server->Config.MaxRetainedTopics)
		return xmqttServerError(XERR_RANGE, "retained checkpoint exceeds topic limit");
	if (xrtNetEngineCurrent(Server->Engine))
		return xmqttServerError(XERR_STATE, "restore retained state outside MQTT broker workers");
	double Now = xrtTimer();
	if (!isfinite(Now)) return xmqttServerError(XERR_STATE, "invalid MQTT retained recovery clock");
	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing || Server->Info.Accepted ||
		xrtAtomic32Load(&Server->RetainedTopics, XMEMORY_RELAXED)) {
		xrtMutexUnlock(&Server->Gate);
		return xmqttServerError(XERR_STATE, "restore retained state before the first broker attachment");
	}
	bool Restored = true;
	for (size_t Index = 0; Index < Count; ++Index) {
		if (xmqttServerRestoreRetainedRecord(Server, &Records[Index], ElapsedMilliseconds, Now)) continue;
		Restored = false;
		for (size_t Shard = 0; Shard < XMQTT_SERVER_TOPIC_SHARDS; ++Shard)
			xmqttServerClearRetained(Server, Server->Shards + Shard);
		xrtAtomic64Store(&Server->DroppedRetained, 0, XMEMORY_RELAXED);
		break;
	}
	xrtMutexUnlock(&Server->Gate);
	return Restored;
}
#endif

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
		while (xrtAtomic32Load(&Current->Session->State, XMEMORY_ACQUIRE) != XMQTT_SERVER_ONLINE) {
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
		if (xrtAtomic32Load(&Link->Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_SEALED ||
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
			if (xrtAtomic32Load(&Link->Session->State, XMEMORY_ACQUIRE) != XMQTT_SERVER_SEALED &&
				(!(Link->Options & XMQTT_SUBSCRIBE_NO_LOCAL) || Link->Session->Identity != Iter->Origin)) return Link;
		}
	}
	return NULL;
}

static uint8 xmqttServerRoute(xmqttserver* Server, xmqttversion Version, uint64 Identity,
	const xmqttpacket* Packet, xmqttproperty* Expiry, double Now)
{
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
		if ((Wildcard && !xmqttServerCollectWildcard(&Targets, Wildcard, Key, Identity)) ||
			(Root && !xmqttServerCollectWildcard(&Targets, Root, Key, Identity)) ||
			(Targets.Count && !xmqttServerCollectFilter(&Targets, Topic, Identity))) {
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
		.Targets = Targets.Count ? &Targets : NULL, .Link = Topic ? Topic->Head : NULL, .Origin = Identity
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
		Frames[Mode] = xmqttServerEncodePublish(Packet, Version,
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
		if (!xmqttServerRouteDelivery(Link->Session, Frames[Mode], Mode >= 3)) ++Dropped;
		else if (Mode < 3) ++Forwarded;
	}
	for (size_t Index = 0; Index < Targets.ShareCount; ++Index) {
		xmqttserverlink* Member = Targets.Shares[Index].Member;
		if (!Member) continue;
		unsigned Mode = xmqttServerDeliveryMode(Member, Targets.Retain, SourceQos);
		if (!xmqttServerRouteDelivery(Member->Session, Frames[Mode], Mode >= 3)) ++Dropped;
		else if (Mode < 3) ++Forwarded;
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

static uint8 xmqttServerDistribute(xmqttserver* Server, xmqttversion Version, uint64 Identity,
	xstrview Client, const xmqttpacket* Packet, xmqttproperty* Expiry, double Now)
{
	bool Retain = (Packet->Flags & XMQTT_PUBLISH_RETAIN) != 0;
	bool Stored = Retain && Server->Config.MaxRetainedTopics &&
		xmqttServerStoreRetained(Server, Version, Client, Packet, Expiry, Now);
	if (Retain && !Stored && (Packet->Flags & XMQTT_PUBLISH_QOS_MASK)) {
		xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
		return 0x97;
	}
	if (Expiry && !Expiry->Value.Number) {
		xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
		return 0;
	}
	uint8 Reason = xmqttServerRoute(Server, Version, Identity, Packet, Expiry, Now);
	/* A retained obligation can be committed independently of routing. */
	return Stored && Reason >= 0x80 ? 0 : Reason;
}

static void xmqttServerPublishWill(xmqttserver* Server, xmqttserverwill* Will)
{
	xmqttpacket Packet;
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	xmqttcodecbuffers Scratch = {.Properties = Properties, .PropertyCapacity = XMQTT_SERVER_MAX_PROPERTIES};
	xbytesview Wire = {Will->Wire, Will->Bytes};
	const xmqttcodecconfig* Codec = &Server->Config.Codec;
	xmqttcodecstatus Status;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Will->Version == XMQTT_VERSION_311)
		Status = xmqttPacketRead311(Wire, XMQTT_SERVER_TO_CLIENT, Codec, &Scratch, &Packet, NULL);
	else
#endif
		Status = xmqttPacketRead5(Wire, XMQTT_SERVER_TO_CLIENT, Codec, &Scratch, &Packet, NULL);
	if (Status == XMQTT_CODEC_DONE) {
		xmqttproperty* Expiry = NULL;
		for (size_t Index = 0; Index < Packet.Properties.Count; ++Index)
			if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Expiry = Properties + Index;
		xstrview Client = {(const char*)Will->Wire + Will->Bytes, Will->ClientBytes};
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
		if ((Packet.Flags & XMQTT_PUBLISH_RETAIN) && Server->RetainedSubmit) {
			if (!xmqttServerSubmitRetainedWill(Server, Will, Client, &Packet, Expiry)) {
				xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
				xrtAtomic64FetchAdd(&Server->DroppedRetained, 1, XMEMORY_RELAXED);
			}
		} else
#endif
		(void)xmqttServerDistribute(Server, Will->Version, Will->Identity, Client, &Packet, Expiry, xrtTimer());
	} else xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
	xmqttServerDropWill(Server, Will);
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
	xmqttserverpeer* Peer = Ack->Peer;
	if (xrtAVLRemove(&Session->Incoming, &Ack->Identifier, xmqttServerCompareAck, NULL) != &Ack->Node) abort();
	if (Ack == &Session->InlineAck) Ack->Identifier = 0;
	else xrtFree(Ack);
	xrtAtomic32FetchSub(&Session->Server->IncomingPublishes, 1, XMEMORY_RELAXED);
	if (Peer) xmqttServerPeerDrop(Peer);
}

static xmqttserverack* xmqttServerNewAck(xmqttserversession* Session, xmqttserverpeer* Peer, uint16 Identifier)
{
	xmqttserver* Server = Session->Server;
	if (!xmqttServerReserve(&Server->IncomingPublishes, Server->Config.MaxIncomingPublishes)) return NULL;
	xmqttserverack* Ack = Session->InlineAck.Identifier ? xrtMalloc(sizeof(*Ack)) : &Session->InlineAck;
	if (!Ack) {
		xrtAtomic32FetchSub(&Server->IncomingPublishes, 1, XMEMORY_RELAXED);
		return NULL;
	}
	Ack->Session = Session;
	Ack->Peer = Peer;
	Ack->Identifier = Identifier;
	Ack->Phase = XMQTT_SERVER_ACK_TERMINAL;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	Ack->Durable = false;
#endif
	xrtAVLNodeInit(&Ack->Node);
	if (xrtAVLInsert(&Session->Incoming, &Ack->Node, &Ack->Identifier, xmqttServerCompareAck, NULL, NULL) !=
		&Ack->Node) abort();
	if (Peer) xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	return Ack;
}

static xmqttserverack* xmqttServerReserveAck(xmqttserverpeer* Peer, uint16 Identifier, uint8* Reason)
{
	if (xrtAVLFind(&Peer->Session->Incoming, &Identifier, xmqttServerCompareAck, NULL)) {
		*Reason = 0x82;
		return NULL;
	}
	if (Peer->Session->Incoming.Count >= Peer->Server->Config.ReceiveMaximum) {
		*Reason = 0x93;
		return NULL;
	}
	*Reason = 0x97;
	return xmqttServerNewAck(Peer->Session, Peer, Identifier);
}
static void xmqttServerSendAck(xmqttserverpeer* Peer, xmqttserverack* Ack, uint8 Header, uint8 Reason)
{
	bool V5 = Peer->Version == XMQTT_VERSION_5;
	size_t Bytes = xmqttServerIdWire(Ack->Wire, Header, Ack->Identifier, V5 ? Reason : 0);
	const xnetref Part = {Ack->Wire, Bytes, xmqttServerReleaseAck, Ack};
	if ((V5 || Reason < 0x80) && Bytes <= Peer->PacketLimit && xmqttServerSendRefs(Peer, &Part, 1) == XNET_RESULT_OK)
		return;
	xmqttServerReleaseAck(Ack, NULL, 0);
	xmqttServerAbortPeer(Peer);
}

static void xmqttServerFinishPubrel(xmqttserverpeer* Peer, xmqttserverack* Ack)
{
	Ack->Phase = XMQTT_SERVER_ACK_PUBCOMP;
	Ack->Peer = Peer;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	xmqttServerSendAck(Peer, Ack, 0x70, 0);
}

static void xmqttServerReceiveRel(xmqttserverpeer* Peer, uint16 Identifier, double Now)
{
	xmqttserverack* Ack = (xmqttserverack*)xrtAVLFind(&Peer->Session->Incoming, &Identifier, xmqttServerCompareAck, NULL);
	if (Ack && Ack->Phase == XMQTT_SERVER_ACK_AWAIT_PUBREL) {
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
		if (Ack->Durable && Peer->Server->SessionSubmit) {
			xmqttServerSubmitSessionCommit(Peer, Ack, Now);
			return;
		}
#else
		(void)Now;
#endif
		xmqttServerFinishPubrel(Peer, Ack);
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
	xmqttServerForgetDelivery(Peer, Delivery);
	xmqttServerDropDelivery(Delivery, NULL, 0);
	xmqttServerPumpOutgoing(Peer);
}

static void xmqttServerAcknowledgePublish(xmqttserverpeer* Peer, xmqttserverack* Ack, unsigned Qos, uint8 Reason)
{
	if (!Ack) return;
	if (Qos == 2 && Reason < 0x80) {
		/* Routing was accepted even if the publisher disconnected before PUBREC. */
		Ack->Phase = XMQTT_SERVER_ACK_AWAIT_PUBREL;
		Ack->Reason = Reason;
		Ack->Peer = NULL;
		xmqttServerPeerDrop(Peer);
		if (!Peer->Closing) xmqttServerSendId(Peer, 0x50, Ack->Identifier, Reason);
	} else xmqttServerSendAck(Peer, Ack, Qos == 2 ? 0x50 : 0x40, Reason);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
struct xmqttserverretainedticket {
	xnetpost Completion;
	xmqttserver* Server;
	xnetworker* Worker;
	xmqttserverretainedticket* Next;
	xmqttserverpeer* Peer;
	xmqttserverack* Ack;
	xmqttservershard* Shard;
	xmqttserverretained *Previous, *Entry;
	xmqttserverpublication* Publication;
	xmqttserverretainedrecord Record;
	xbytesview Key;
	uint64 Hash, Identity;
	size_t Charge, Growth;
	double Received;
	xmqttserverretainedresult Result;
	bool Remove;
};

static void xmqttServerFreeRetainedTicket(xmqttserverretainedticket* Ticket)
{
	xatomic64* Pending = &Ticket->Server->RetainedPendingBytes;
	size_t Charge = Ticket->Charge;
	if (Ticket->Publication) xmqttServerFreePublication(Ticket->Publication, NULL, 0);
	xrtFree(Ticket);
	xrtAtomic64FetchSub(Pending, Charge, XMEMORY_RELAXED);
}

static void xmqttServerDropRetainedTicket(xmqttserverretainedticket* Ticket)
{
	xmqttserver* Server = Ticket->Server;
	xmqttserverpeer* Peer = Ticket->Peer;
	xmqttServerFreeRetainedTicket(Ticket);
	if (Peer) xmqttServerPeerDrop(Peer);
	else {
		xrtMutexLock(&Server->Gate);
		--Server->RetainedWillWork;
		xmqttServerSignalDrained(Server);
		xrtMutexUnlock(&Server->Gate);
	}
}

/* Called with RetainedGate held and the shard unclaimed. */
static bool xmqttServerReserveRetainedTicket(xmqttserverretainedticket* Ticket)
{
	xmqttserver* Server = Ticket->Server;
	xmqttservershard* Shard = Ticket->Shard;
	if (xrtAtomic32Load(&Server->RetainedFailed, XMEMORY_ACQUIRE)) return false;
	xmqttServerExpireRetained(Server, Shard, Ticket->Received);
	Ticket->Previous = xmqttServerFindKey(&Shard->Retained, Ticket->Hash, Ticket->Key);
	if (!Ticket->Remove) {
		size_t Bytes = Ticket->Record.Publish.Size + Ticket->Record.PublisherId.Size;
		xmqttserverretained* Previous = Ticket->Previous;
		size_t Old = Previous ? Previous->Publication->Size + Previous->PublisherIdBytes : 0;
		Ticket->Growth = Bytes > Old ? Bytes - Old : 0;
		if (!xmqttServerReserveRetained(Server, Ticket->Growth, !Previous)) return false;
		Ticket->Entry = Previous ? Previous : xrtMalloc(sizeof(*Ticket->Entry) + Ticket->Key.Size);
		if (!Ticket->Entry) {
			xrtAtomic64FetchSub(&Server->RetainedBytes, Ticket->Growth, XMEMORY_RELAXED);
			xrtAtomic32FetchSub(&Server->RetainedTopics, 1, XMEMORY_RELAXED);
			return false;
		}
	}
	Shard->RetainedPending = true;
	return true;
}

/* A queued Will takes priority over new client traffic to its shard. */
static bool xmqttServerPrepareRetainedTicket(xmqttserverretainedticket* Ticket)
{
	xmqttservershard* Shard = Ticket->Shard;
	xrtRWLockWrite(&Shard->RetainedGate);
	bool Prepared = !Shard->RetainedPending && !Shard->WillHead && xmqttServerReserveRetainedTicket(Ticket);
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
	return Prepared;
}

static void xmqttServerResolveRetainedTicket(xmqttserverretainedticket* Ticket, bool Apply)
{
	xmqttserver* Server = Ticket->Server;
	xmqttservershard* Shard = Ticket->Shard;
	xrtRWLockWrite(&Shard->RetainedGate);
	if (Apply && Ticket->Entry) {
		const xmqttserverretainedrecord* Record = &Ticket->Record;
		double Expires = Record->RemainingMilliseconds ?
			Ticket->Received + (double)Record->RemainingMilliseconds * 0.001 : 0;
		xrtAtomic32FetchAdd(&Ticket->Publication->References, 1, XMEMORY_RELAXED);
		xmqttServerInstallRetained(Server, Shard, Ticket->Entry, Ticket->Previous, Ticket->Publication,
			Ticket->Hash, Ticket->Key, Record->Version, Record->PublisherId.Size, Ticket->Received, Expires);
	} else if (Apply && Ticket->Previous) {
		xmqttServerRemoveRetained(Server, Shard, Ticket->Previous);
	} else if (Ticket->Entry) {
		xrtAtomic64FetchSub(&Server->RetainedBytes, Ticket->Growth, XMEMORY_RELAXED);
		if (!Ticket->Previous) {
			xrtAtomic32FetchSub(&Server->RetainedTopics, 1, XMEMORY_RELAXED);
			xrtFree(Ticket->Entry);
		}
	}
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
}

static bool xmqttServerReleaseRetainedShard(xmqttservershard* Shard)
{
	xrtRWLockWrite(&Shard->RetainedGate);
	Shard->RetainedPending = false;
	bool Waiting = Shard->WillHead != NULL;
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
	return Waiting;
}

static uint8 xmqttServerRouteRetainedTicket(xmqttserverretainedticket* Ticket, double Now)
{
	xmqttserver* Server = Ticket->Server;
	xmqttversion Version = Ticket->Record.Version;
	xmqttdirection Direction = Ticket->Peer ? XMQTT_CLIENT_TO_SERVER : XMQTT_SERVER_TO_CLIENT;
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
	xmqttpacket Packet;
	xmqttcodecstatus Status;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Version == XMQTT_VERSION_311)
		Status = xmqttPacketRead311(Ticket->Record.Publish, Direction, &Server->Config.Codec, &Scratch, &Packet, NULL);
	else
#endif
		Status = xmqttPacketRead5(Ticket->Record.Publish, Direction, &Server->Config.Codec, &Scratch, &Packet, NULL);
	if (Status != XMQTT_CODEC_DONE) abort(); /* This immutable packet was validated at admission. */
	xmqttproperty* Expiry = NULL;
	for (size_t Index = 0; Index < Packet.Properties.Count; ++Index)
		if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Expiry = Properties + Index;
	if (Expiry && Now >= Ticket->Received + Expiry->Value.Number) return 0;
	uint8 Reason = xmqttServerRoute(Server, Version, Ticket->Identity, &Packet, Expiry, Ticket->Received);
	return Reason >= 0x80 ? 0 : Reason; /* The retained obligation already committed. */
}

static bool xmqttServerHandOffRetained(xmqttserverretainedticket* Ticket)
{
	xmqttserver* Server = Ticket->Server;
	xstrview Topic = {(cstr)Ticket->Key.Data, Ticket->Key.Size};
	return Server->RetainedSubmit(Ticket, Topic, &Ticket->Record, Ticket->Remove, Ticket->Received, Server->RetainedUser);
}

static void xmqttServerRejectRetainedWill(xmqttserver* Server)
{
	xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
	xrtAtomic64FetchAdd(&Server->DroppedRetained, 1, XMEMORY_RELAXED);
}

/* The caller still owns a broker lifetime pin while draining. A successful
 * handoff leaves one active operation, and its completion starts the next. */
static void xmqttServerDrainRetainedWills(xmqttservershard* Shard, xnetworker* Worker)
{
	for (;;) {
		xrtRWLockWrite(&Shard->RetainedGate);
		xmqttserverretainedticket* Ticket = Shard->WillHead;
		if (!Ticket || Shard->RetainedPending) {
			xrtRWLockWriteUnlock(&Shard->RetainedGate);
			return;
		}
		Shard->WillHead = Ticket->Next;
		if (!Shard->WillHead) Shard->WillTail = NULL;
		Ticket->Worker = Worker;
		Ticket->Received = xrtTimer();
		bool Prepared = isfinite(Ticket->Received) && xmqttServerReserveRetainedTicket(Ticket);
		xrtRWLockWriteUnlock(&Shard->RetainedGate);
		if (Prepared && xmqttServerHandOffRetained(Ticket)) return;
		if (Prepared) {
			xmqttServerResolveRetainedTicket(Ticket, false);
			xmqttServerReleaseRetainedShard(Shard);
		}
		xmqttServerRejectRetainedWill(Ticket->Server);
		xmqttServerDropRetainedTicket(Ticket);
	}
}

static void xmqttServerRetainedCompleted(xnetworker* Worker, ptr Data)
{
	xmqttserverretainedticket* Ticket = Data;
	xmqttserverpeer* Peer = Ticket->Peer;
	xmqttserver* Server = Ticket->Server;
	if (Peer) Peer->Retaining = NULL;
	bool Committed = Ticket->Result == XMQTT_SERVER_RETAINED_COMMITTED;
	bool Unknown = Ticket->Result == XMQTT_SERVER_RETAINED_UNKNOWN;
	/* Seal admission before making a shard with an unknown backend outcome writable. */
	if (Unknown) {
		xrtAtomic32Store(&Server->RetainedFailed, 1, XMEMORY_RELEASE);
		xmqttServerClose(Server);
	}
	xmqttServerResolveRetainedTicket(Ticket, Committed);
	xrtMutexLock(&Server->Gate);
	bool Closing = Server->Info.Closing;
	xrtMutexUnlock(&Server->Gate);
	double Now = xrtTimer();
	bool Active = !Closing && !xrtAtomic32Load(&Server->RetainedFailed, XMEMORY_ACQUIRE);
	bool Resume = false;
	if (Active && Peer && !Peer->Closing && (!isfinite(Now) || (Peer->Timer && Now >= Peer->Deadline)))
		xmqttServerTimedOut(Peer);
	if (!Peer) {
		if (!Committed) xmqttServerRejectRetainedWill(Server);
		else if (Active && isfinite(Now)) (void)xmqttServerRouteRetainedTicket(Ticket, Now);
	} else if (Active && xmqttServerCanRead(Peer)) {
		unsigned Qos = (Ticket->Record.Publish.Data[0] & XMQTT_PUBLISH_QOS_MASK) >> 1;
		if (Committed) {
			uint8 Reason = xmqttServerRouteRetainedTicket(Ticket, Now);
			xmqttServerAcknowledgePublish(Peer, Ticket->Ack, Qos, Reason);
		} else {
			xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
			xmqttServerAcknowledgePublish(Peer, Ticket->Ack, Qos, 0x83);
		}
		Resume = true;
	} else if (Ticket->Ack) xmqttServerReleaseAck(Ticket->Ack, NULL, 0);
	if (xmqttServerReleaseRetainedShard(Ticket->Shard))
		xmqttServerDrainRetainedWills(Ticket->Shard, Worker);
	if (Resume) xmqttServerResumeInput(Peer);
	xmqttServerDropRetainedTicket(Ticket);
}

XMQTT_API bool xmqttServerCompleteRetained(xmqttserverretainedticket* Ticket, xmqttserverretainedresult Result)
{
	if (!Ticket || (Result != XMQTT_SERVER_RETAINED_COMMITTED &&
		Result != XMQTT_SERVER_RETAINED_REJECTED && Result != XMQTT_SERVER_RETAINED_UNKNOWN))
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT retained completion");
	Ticket->Result = Result;
	return xrtNetPost(Ticket->Worker, &Ticket->Completion, xmqttServerRetainedCompleted, Ticket);
}

static xmqttserverretainedticket* xmqttServerNewRetainedTicket(xmqttserver* Server, xmqttversion Version,
	xstrview Client, const xmqttpacket* Packet, const xmqttproperty* Expiry)
{
	xstrview Name = Packet->Body.Publish.Topic;
	size_t Charge = sizeof(xmqttserverretainedticket) + sizeof(xmqttserverpublication) + Packet->Wire.Size +
		Client.Size + sizeof(xmqttserverretained) + Name.Size;
	if (!xmqttServerReserveBytes(&Server->RetainedPendingBytes, Server->RetainedPendingLimit, Charge)) return NULL;
	xmqttserverretainedticket* Ticket = xrtCalloc(1, sizeof(*Ticket));
	if (!Ticket) {
		xrtAtomic64FetchSub(&Server->RetainedPendingBytes, Charge, XMEMORY_RELAXED);
		return NULL;
	}
	Ticket->Server = Server;
	Ticket->Charge = Charge;
	uint8 Flags = Packet->Flags & ~XMQTT_PUBLISH_DUP;
	Ticket->Publication = xmqttServerCopyPublication(Packet->Wire, Flags, Client);
	if (!Ticket->Publication || !xrtNetPostInit(&Ticket->Completion)) {
		xmqttServerFreeRetainedTicket(Ticket);
		return NULL;
	}
	bytes Wire = Ticket->Publication->Wire;
	Ticket->Key = (xbytesview){Wire + ((cbytes)Name.Data - Packet->Wire.Data), Name.Size};
	Ticket->Shard = xmqttServerWildcardShard(Server, Ticket->Key);
	Ticket->Hash = xmqttServerIdHash(Ticket->Key, &Server->HashKey);
	Ticket->Remove = !Packet->Body.Publish.Payload.Size || (Expiry && !Expiry->Value.Number);
	Ticket->Record = (xmqttserverretainedrecord){
		Version, {(cstr)Wire + Packet->Wire.Size, Client.Size}, {Wire, Packet->Wire.Size},
		Expiry ? UINT64_C(1000) * Expiry->Value.Number : 0
	};
	return Ticket;
}

static bool xmqttServerSubmitRetainedWill(xmqttserver* Server, const xmqttserverwill* Will,
	xstrview Client, const xmqttpacket* Packet, const xmqttproperty* Expiry)
{
	xnetworker* Worker = xrtNetEngineCurrent(Server->Engine);
	if (!Worker || xrtAtomic32Load(&Server->RetainedFailed, XMEMORY_ACQUIRE)) return false;
	xmqttserverretainedticket* Ticket = xmqttServerNewRetainedTicket(Server, Will->Version, Client, Packet, Expiry);
	if (!Ticket) return false;
	Ticket->Identity = Will->Identity;
	xrtMutexLock(&Server->Gate);
	++Server->RetainedWillWork;
	xrtMutexUnlock(&Server->Gate);
	xmqttservershard* Shard = Ticket->Shard;
	xrtRWLockWrite(&Shard->RetainedGate);
	if (Shard->WillTail) Shard->WillTail->Next = Ticket;
	else Shard->WillHead = Ticket;
	Shard->WillTail = Ticket;
	xrtRWLockWriteUnlock(&Shard->RetainedGate);
	xmqttServerDrainRetainedWills(Shard, Worker);
	return true;
}

static bool xmqttServerSubmitRetained(xmqttserverpeer* Peer, xmqttserverack* Ack,
	const xmqttpacket* Packet, const xmqttproperty* Expiry, double Now)
{
	xmqttserverretainedticket* Ticket = xmqttServerNewRetainedTicket(Peer->Server, Peer->Version,
		Peer->Session->ClientName, Packet, Expiry);
	if (!Ticket) return false;
	xrtAtomic32FetchAdd(&Peer->References, 1, XMEMORY_RELAXED);
	Ticket->Peer = Peer;
	Ticket->Worker = xrtNetStreamWorker(Peer->Stream);
	Ticket->Ack = Ack;
	Ticket->Identity = Peer->Session->Identity;
	Ticket->Received = Now;
	if (!xmqttServerPrepareRetainedTicket(Ticket)) {
		xmqttServerDropRetainedTicket(Ticket);
		return false;
	}
	Peer->Retaining = Ticket;
	if (!xmqttServerHandOffRetained(Ticket)) {
		Peer->Retaining = NULL;
		xmqttServerResolveRetainedTicket(Ticket, false);
		if (xmqttServerReleaseRetainedShard(Ticket->Shard))
			xmqttServerDrainRetainedWills(Ticket->Shard, Ticket->Worker);
		xmqttServerDropRetainedTicket(Ticket);
		return false;
	}
	xmqttServerPauseInput(Peer);
	return true;
}
#endif

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
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	} else if (Retain && Server->RetainedSubmit) {
		if (xmqttServerSubmitRetained(Peer, Ack, Packet, Expiry, Now)) return;
		xrtAtomic64FetchAdd(&Server->RejectedPublications, 1, XMEMORY_RELAXED);
		xrtAtomic64FetchAdd(&Server->DroppedRetained, 1, XMEMORY_RELAXED);
		Reason = 0x97;
#endif
	} else Reason = xmqttServerDistribute(Server, Peer->Version, Peer->Session->Identity,
		Peer->Session->ClientName, Packet, Expiry, Now);
	xmqttServerAcknowledgePublish(Peer, Ack, Qos, Reason);
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
		xmqttServerReceiveRel(Peer, Packet->Identifier, Now);
		break;
	case XMQTT_PACKET_DISCONNECT:
		for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
			if (Properties[Index].Id != XMQTT_PROPERTY_SESSION_EXPIRY) continue;
			uint32 Expiry = Properties[Index].Value.Number;
			if (!xrtAtomic32Load(&Peer->SessionExpiry, XMEMORY_RELAXED) && Expiry) {
				xmqttServerProtocolClose(Peer, 0x82, Now);
				return;
			}
			xrtAtomic32Store(&Peer->SessionExpiry, Expiry, XMEMORY_RELAXED);
		}
		if (!Packet->Reason) {
			xmqttServerDropWill(Peer->Server, Peer->Will);
			Peer->Will = NULL;
		}
		xmqttServerFinishPeer(Peer, Now);
		break;
	default:
		xmqttServerProtocolClose(Peer, 0x82, Now);
		break;
	}
}
static bool xmqttServerCanRead(xmqttserverpeer* Peer)
{
	if (Peer->Closing || Peer->AwaitingSession ||
		xrtAtomic32Load(&Peer->TakenOver, XMEMORY_ACQUIRE)) return false;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	if (Peer->SessionCommit) return false;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	if (Peer->Retaining) return false;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	if (Peer->Auth && Peer->Auth->Timer && xrtTimer() >= Peer->Auth->Deadline) {
		xmqttServerTimedOut(Peer);
		return false;
	}
#endif
	if (Peer->Session &&
		xrtAtomic32Load(&Peer->Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_SEALED) {
		xmqttServerAbortPeer(Peer);
		return false;
	}
	return true;
}

static xmqttwirestatus xmqttServerReadHeader(xmqttserverpeer* Peer, xbytesview Input, xmqttfixedheader* Header)
{
	size_t Limit = Peer->Server->Config.Codec.MaxPacketBytes;
	if (!Peer->Version) {
		xmqttwirestatus Status = xmqttConnectVersionRead(Input, Limit, &Peer->Version);
		if (Status != XMQTT_WIRE_DONE) return Status;
	}
	const xmqttwireconfig Wire = {sizeof(Wire), Peer->Version, Limit};
	return xmqttHeaderRead(Input, &Wire, Header);
}

static bool xmqttServerReadPacket(xmqttserverpeer* Peer, const xmqttfixedheader* Header, cbytes Data)
{
	if (!Peer->Online && Peer->Version == XMQTT_VERSION_311 && !Peer->Server->Config.Allow311) {
		xmqttServerRefuse(Peer, 0, 1, XMQTT_PACKET_MAX_BYTES, xrtTimer());
		return false;
	}
	if (Peer->Online && Header->Type == XMQTT_PACKET_PINGREQ && !Header->RemainingLength) {
		static const unsigned char Pong[] = {0xd0, 0x00};
		if (Peer->KeepAliveMs) {
			double Now = xrtTimer();
			if (!isfinite(Now) || Now >= Peer->Deadline) {
				xmqttServerAbortPeer(Peer);
				return false;
			}
			Peer->Deadline = Now + (double)Peer->KeepAliveMs * 0.001;
		}
		if (xmqttServerSend(Peer, Pong, sizeof(Pong)) != XNET_RESULT_OK) {
			xmqttServerAbortPeer(Peer);
			return false;
		}
		return true;
	}
	xbytesview Input = {Data, Header->PacketBytes};
	const xmqttcodecconfig* Codec = &Peer->Server->Config.Codec;
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
		return false;
	}
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	if (Packet.Type == XMQTT_PACKET_AUTH && (Peer->Auth || Peer->Online)) {
		if (Peer->Online && Peer->KeepAliveMs) Peer->Deadline = Now + (double)Peer->KeepAliveMs * 0.001;
		xmqttServerReadAuth(Peer, &Packet, Header->PacketBytes, Now);
		return true;
	}
	if (!Peer->Online && Peer->Auth) {
		if (Packet.Type == XMQTT_PACKET_DISCONNECT) xmqttServerFinishPeer(Peer, Now);
		else xmqttServerAuthReject(Peer, 0x82, Now);
		return true;
	}
#endif
	if (!Peer->Online && Packet.Type == XMQTT_PACKET_CONNECT)
		xmqttServerConnect(Peer, &Packet, Header->PacketBytes, Now);
	else if (Peer->Online)
		xmqttServerOnline(Peer, &Packet, Properties, Now);
	else
		xmqttServerAbortPeer(Peer);
	return true;
}

/* True means the remaining prefix needs more input. */
static bool xmqttServerReadPackets(xmqttserverpeer* Peer, const xnetbuf* Buffer, xnetbuf* Writable)
{
	size_t Available;
	while ((Available = xrtNetBufSize(Buffer)) != 0 && xmqttServerCanRead(Peer)) {
		xnetspan Front;
		if (!xrtNetBufFront(Buffer, &Front)) {
			xmqttServerAbortPeer(Peer);
			return false;
		}
		unsigned char Prefix[12];
		size_t Need = Peer->Version ? 5u : sizeof(Prefix);
		if (Need > Available) Need = Available;
		if (Front.Size < Need) Front = (xnetspan){Prefix, xrtNetBufPeek(Buffer, 0, Prefix, Need)};
		xmqttfixedheader Header;
		xmqttwirestatus Status = xmqttServerReadHeader(Peer, (xbytesview){Front.Data, Front.Size}, &Header);
		if (Status == XMQTT_WIRE_MORE) return true;
		if (Status == XMQTT_WIRE_ERROR) {
			xmqttServerAbortPeer(Peer);
			return false;
		}
		if (Available < Header.PacketBytes) return true;

		unsigned char* Copy = NULL;
		if (Front.Size < Header.PacketBytes) {
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
			if (Peer->Tls) {
				if (!xrtTlsStreamPullup(Peer->Tls, Header.PacketBytes, &Front)) {
					xmqttServerAbortPeer(Peer);
					return false;
				}
			} else
#endif
			if (Writable) {
				if (!xrtNetBufPullup(Writable, Header.PacketBytes, &Front)) {
					xmqttServerAbortPeer(Peer);
					return false;
				}
			} else {
				Copy = xrtMalloc(Header.PacketBytes);
				if (!Copy || xrtNetBufPeek(Buffer, 0, Copy, Header.PacketBytes) != Header.PacketBytes) {
					xrtFree(Copy);
					xmqttServerAbortPeer(Peer);
					return false;
				}
				Front.Data = Copy;
			}
		}
		bool Parsed = xmqttServerReadPacket(Peer, &Header, Front.Data);
		if (Copy) xrtFree(Copy);
		if (!Parsed) return false;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
		if (Peer->Tls) {
			if (Peer->Output->Closed) return false;
			if (!xrtTlsStreamConsume(Peer->Tls, Header.PacketBytes)) {
				xmqttServerAbortPeer(Peer);
				return false;
			}
		} else
#endif
		if (Writable) xrtNetBufConsume(Writable, Header.PacketBytes);
		else if (xrtNetStreamConsume(Peer->Stream, Header.PacketBytes) != Header.PacketBytes) return false;
	}
	return false;
}

static void xmqttServerRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	(void)Stream;
	(void)xmqttServerReadPackets(Data, Buffer, Buffer);
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

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
#define XMQTT_SERVER_MAX_LIFETIME_MS (UINT64_C(1000) * UINT32_MAX)

static uint64 xmqttServerRemainingMilliseconds(double Deadline, double Now)
{
	if (isinf(Deadline)) return UINT64_MAX;
	return Deadline > Now ? (uint64)((Deadline - Now) * 1000.0) : 0;
}

static uint32 xmqttServerStoredU32(cbytes Bytes)
{
	return (uint32)Bytes[0] << 24 | (uint32)Bytes[1] << 16 | (uint32)Bytes[2] << 8 | Bytes[3];
}

static bool xmqttServerVisitDelivery(const xmqttserverdelivery* Item, bool Sent, double Now,
	xmqttserversessionvisitor Visit, ptr User)
{
	const xmqttserverpublication* Publication = Item->Publication;
	xmqttserversessionrecord Record = {
		.Kind = Sent ? XMQTT_SERVER_RECORD_SENT : XMQTT_SERVER_RECORD_QUEUED,
		.Data = {Publication->Wire, Publication->Size}
	};
	if (Sent) {
		Record.Identifier = Item->Identifier;
		Record.Code = Item->ExpectedAck;
		if (Publication->ExpiryOffset)
			Record.Value = xmqttServerStoredU32(Item->Prefix + Publication->ExpiryOffset);
	} else {
		Record.Milliseconds = Publication->Expires ?
			xmqttServerRemainingMilliseconds(Publication->Expires, Now) : UINT64_MAX;
		if (!Record.Milliseconds) return true;
	}
	return Visit(&Record, User);
}

static bool xmqttServerVisitSession(const xmqttserversession* Session, double Now,
	xmqttserversessionvisitor Visit, ptr User)
{
	uint64 Remaining = xmqttServerRemainingMilliseconds(Session->Expires, Now);
	if (!Remaining && !Session->Will) return true;
	xmqttserversessionrecord Record = {
		.Kind = XMQTT_SERVER_RECORD_SESSION,
		.Data = {(cbytes)Session->ClientName.Data, Session->ClientName.Size},
		.Code = (uint8)Session->Version, .Milliseconds = Remaining, .Identifier = Session->LastIdentifier
	};
	if (!Visit(&Record, User)) return false;
	if (Session->Will) {
		Record = (xmqttserversessionrecord){
			.Kind = XMQTT_SERVER_RECORD_WILL,
			.Data = {Session->Will->Wire, Session->Will->Bytes},
			.Milliseconds = xmqttServerRemainingMilliseconds(Session->Will->Due, Now)
		};
		if (!Visit(&Record, User)) return false;
	}
	if (!Remaining) return true;
	for (const xmqttserverlink* Link = Session->Links; Link; Link = Link->SessionNext) {
		Record = (xmqttserversessionrecord){
			.Kind = XMQTT_SERVER_RECORD_SUBSCRIPTION, .Data = Link->Topic->Key.Text,
			.Group = Link->Share ? Link->Share->Name : (xbytesview){0}, .Code = Link->Options
		};
		if (!Visit(&Record, User)) return false;
	}
	xavliter Iterator;
	xrtAVLIterBegin(&Session->Incoming, &Iterator);
	const xmqttserverack* Ack;
	while ((Ack = (const xmqttserverack*)xrtAVLIterNext(&Iterator)) != NULL) {
		if (Ack->Peer || Ack->Phase != XMQTT_SERVER_ACK_AWAIT_PUBREL) abort();
		Record = (xmqttserversessionrecord){
			.Kind = XMQTT_SERVER_RECORD_INCOMING_QOS2, .Identifier = Ack->Identifier, .Code = Ack->Reason
		};
		if (!Visit(&Record, User)) return false;
	}
	for (const xmqttserverdelivery* Item = Session->SentHead; Item; Item = Item->SentNext)
		if (!xmqttServerVisitDelivery(Item, true, Now, Visit, User)) return false;
	for (const xmqttserverdelivery* Item = Session->Waiting; Item; Item = Item->Next)
		if (!xmqttServerVisitDelivery(Item, false, Now, Visit, User)) return false;
	for (const xmqttserverdelivery* Item = Session->OfflineQueue; Item; Item = Item->Next)
		if (!xmqttServerVisitDelivery(Item, false, Now, Visit, User)) return false;
	return true;
}

XMQTT_API bool xmqttServerVisitSessions(xmqttserver* Server, xmqttserversessionvisitor Visit, ptr User)
{
	if (!Server || !Visit) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT session checkpoint visit");
	if (xmqttServerWaitFor(Server, 0) != XWAIT_OK || !Server->Checkpoint)
		return xmqttServerError(XERR_STATE, "close for checkpoint and drain MQTT broker before visiting sessions");
	if (Server->SessionStoreFailed) return xmqttServerError(XERR_STATE, "MQTT session commit outcome unknown");
	double Now = xrtTimer();
	if (!isfinite(Now)) return xmqttServerError(XERR_STATE, "invalid MQTT session checkpoint clock");
	for (const xmqttserversession* Session = Server->Offline; Session; Session = Session->OfflineNext)
		if (xrtAtomic32Load(&Session->State, XMEMORY_ACQUIRE) == XMQTT_SERVER_OFFLINE &&
			!xmqttServerVisitSession(Session, Now, Visit, User)) return false;
	return true;
}

static double xmqttServerRestoreDeadline(uint64 Milliseconds, uint64 Elapsed, double Now)
{
	if (Milliseconds == UINT64_MAX) return INFINITY;
	return Milliseconds > Elapsed ? Now + (double)(Milliseconds - Elapsed) * 0.001 : Now;
}

static xmqttserversession* xmqttServerRestoreSession(xmqttserver* Server,
	const xmqttserversessionrecord* Record, uint64 Elapsed, double Now)
{
	xstrview Id = {(cstr)Record->Data.Data, Record->Data.Size};
	xmqttversion Version = (xmqttversion)Record->Code;
	if (Record->Group.Size || Record->Value ||
		(Record->Milliseconds != UINT64_MAX && Record->Milliseconds > XMQTT_SERVER_MAX_LIFETIME_MS) ||
		(Version != XMQTT_VERSION_5 && (Version != XMQTT_VERSION_311 || !Server->Config.Allow311)) ||
		(Version == XMQTT_VERSION_311 && Record->Milliseconds != UINT64_MAX) ||
		!Id.Size || Id.Size > Server->Config.MaxClientIdBytes ||
		!xmqttUtf8StringCheck(Id, Server->Config.Codec.Utf8Flags, NULL) ||
		xrtMapGet(&Server->Identities, Record->Data)) {
		xmqttServerError(XERR_PROTOCOL, "invalid MQTT checkpoint session identity or lifetime");
		return NULL;
	}
	if (Server->Info.PersistentSessions >= Server->Config.MaxPersistentSessions) {
		xmqttServerError(XERR_RANGE, "MQTT checkpoint exceeds persistent session limit");
		return NULL;
	}
	uint64 Identity = xrtNetWorkerOperationId(xrtNetEngineWorker(Server->Engine, 0));
	if (!Identity) return NULL;
	xmqttserversession* Session = xmqttServerNewSession(Server, NULL, Version, Id, true);
	if (!Session) return NULL;
	xmqttserversession** Entry = xrtMapGetOrAdd(&Server->Identities, Record->Data, NULL);
	if (!Entry) {
		xmqttServerFreeSession(Session);
		return NULL;
	}
	if (!xrtMapStoredKey(&Server->Identities, Record->Data, &Session->RegisteredKey)) abort();
	*Entry = Session;
	Session->Identity = Identity;
	Session->LastIdentifier = Record->Identifier;
	Session->Expires = xmqttServerRestoreDeadline(Record->Milliseconds, Elapsed, Now);
	xrtAtomic32Store(&Session->State, XMQTT_SERVER_OFFLINE, XMEMORY_RELEASE);
	++Server->Info.PersistentSessions;
	xmqttServerParkSession(Server, Session);
	return Session;
}

static bool xmqttServerRestoreSubscription(xmqttserversession* Session, const xmqttserversessionrecord* Record)
{
	const xmqttserverconfig* Config = &Session->Server->Config;
	xbytesview Filter = Record->Data, Group = Record->Group;
	if (!Filter.Size || Filter.Size > Config->MaxTopicBytes || Group.Size > Config->MaxTopicBytes ||
		(Group.Size && (Session->Version != XMQTT_VERSION_5 ||
			Filter.Size + Group.Size + 8 > Config->MaxTopicBytes ||
			!xrtMemRangeValid(Group.Data, Group.Size) || memchr(Group.Data, '/', Group.Size))) ||
		*xmqttServerFindLink(Session, Filter, Group))
		return xmqttServerError(XERR_PROTOCOL, "invalid or duplicate MQTT checkpoint subscription");
	xmqttsubscription Sub = {{(cstr)Filter.Data, Filter.Size}, Record->Code};
	char* Shared = NULL;
	if (Group.Size) {
		size_t Size = Filter.Size + Group.Size + 8;
		Shared = xrtMalloc(Size);
		if (!Shared) return false;
		memcpy(Shared, "$share/", 7);
		memcpy(Shared + 7, Group.Data, Group.Size);
		Shared[7 + Group.Size] = '/';
		memcpy(Shared + 8 + Group.Size, Filter.Data, Filter.Size);
		Sub.Filter = (xstrview){Shared, Size};
	}
	xmqttpacket Packet;
	xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE, &Packet);
	Packet.Identifier = 1;
	Packet.Body.Topics = (xmqttsubscriptionlist){&Sub, 1};
	size_t Size;
	bool Valid;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Session->Version == XMQTT_VERSION_311)
		Valid = xmqttPacketSize311(&Packet, XMQTT_CLIENT_TO_SERVER, &Config->Codec, &Size);
	else
#endif
		Valid = xmqttPacketSize5(&Packet, XMQTT_CLIENT_TO_SERVER, &Config->Codec, &Size);
	if (Valid) {
		xbytesview Key = {(cbytes)Sub.Filter.Data, Sub.Filter.Size};
		xbytesview Name = xmqttServerShareName(&Key, Session->Version == XMQTT_VERSION_5);
		Valid = xmqttServerIdEqual(Key, Filter, NULL) &&
			(Name.Size ? xmqttServerIdEqual(Name, Group, NULL) : !Group.Size);
	}
	xrtFree(Shared);
	if (!Valid) return xmqttServerError(XERR_PROTOCOL, "invalid MQTT checkpoint filter or options");
	uint8 Result = xmqttServerSubscribeSession(Session, Filter, Group, Record->Code, NULL);
	return Result < 0x80 || xmqttServerError(XERR_AGAIN, "cannot admit MQTT checkpoint subscription");
}

static bool xmqttServerRestoreIncoming(xmqttserversession* Session, const xmqttserversessionrecord* Record)
{
	if (!Record->Identifier || (Record->Code != 0 && Record->Code != 0x10) ||
		xrtAVLFind(&Session->Incoming, &Record->Identifier, xmqttServerCompareAck, NULL))
		return xmqttServerError(XERR_PROTOCOL, "invalid or duplicate MQTT checkpoint incoming QoS 2 ID");
	if (Session->Incoming.Count >= Session->Server->Config.ReceiveMaximum)
		return xmqttServerError(XERR_RANGE, "MQTT session checkpoint exceeds incoming window");
	xmqttserverack* Ack = xmqttServerNewAck(Session, NULL, Record->Identifier);
	if (!Ack) return xmqttServerError(XERR_AGAIN, "cannot admit MQTT checkpoint incoming QoS 2 ID");
	Ack->Phase = XMQTT_SERVER_ACK_AWAIT_PUBREL;
	Ack->Durable = true;
	Ack->Reason = Record->Code;
	return true;
}

static bool xmqttServerStoredPublish(xmqttserversession* Session, xbytesview Wire, const xmqttcodecconfig* Codec,
	const xmqttcodecbuffers* Scratch, xmqttpacket* Packet)
{
	xmqttcodecstatus Status;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	if (Session->Version == XMQTT_VERSION_311)
		Status = xmqttPacketRead311(Wire, XMQTT_SERVER_TO_CLIENT, Codec, Scratch, Packet, NULL);
	else
#else
	(void)Session;
#endif
		Status = xmqttPacketRead5(Wire, XMQTT_SERVER_TO_CLIENT, Codec, Scratch, Packet, NULL);
	if (Status != XMQTT_CODEC_DONE || Packet->Type != XMQTT_PACKET_PUBLISH || Packet->Wire.Size != Wire.Size ||
		(Packet->Flags & XMQTT_PUBLISH_DUP) || Packet->Identifier != !!(Packet->Flags & XMQTT_PUBLISH_QOS_MASK) ||
		!Packet->Body.Publish.Topic.Size)
		return xmqttServerError(XERR_PROTOCOL, "invalid MQTT session checkpoint PUBLISH");
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		xmqttpropertyid Id = Packet->Properties.Data[Index].Id;
		if (Id == XMQTT_PROPERTY_TOPIC_ALIAS || Id == XMQTT_PROPERTY_SUBSCRIPTION_ID)
			return xmqttServerError(XERR_PROTOCOL, "unsupported MQTT checkpoint PUBLISH property");
	}
	return true;
}

static bool xmqttServerRestoreDelivery(xmqttserversession* Session, const xmqttserversessionrecord* Record,
	uint64 Elapsed, double Now)
{
	const xmqttserverconfig* Config = &Session->Server->Config;
	xmqttcodecconfig Codec = Config->Codec;
	Codec.MaxPacketBytes = XMQTT_PACKET_MAX_BYTES;
	if (Record->Data.Size > Config->MaxOutgoingPublishBytes)
		return xmqttServerError(XERR_RANGE, "MQTT checkpoint PUBLISH exceeds outgoing bytes");
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
	xmqttpacket Packet;
	if (!xmqttServerStoredPublish(Session, Record->Data, &Codec, &Scratch, &Packet)) return false;
	const xmqttproperty* Expiry = NULL;
	for (size_t Index = 0; Index < Packet.Properties.Count; ++Index)
		if (Properties[Index].Id == XMQTT_PROPERTY_MESSAGE_EXPIRY) Expiry = Properties + Index;
	uint8 Qos = (Packet.Flags & XMQTT_PUBLISH_QOS_MASK) >> 1;
	bool Sent = Record->Kind == XMQTT_SERVER_RECORD_SENT;
	bool Finite = Record->Milliseconds != UINT64_MAX;
	if (!Qos || (Sent && (!Record->Identifier || Record->Milliseconds ||
		(Record->Code != (Qos == 1 ? XMQTT_PACKET_PUBACK : XMQTT_PACKET_PUBREC) &&
			(Qos != 2 || Record->Code != XMQTT_PACKET_PUBCOMP)) ||
		Record->Value > (Expiry ? Expiry->Value.Number : 0) ||
		xrtAVLFind(&Session->Outgoing, &Record->Identifier, xmqttServerCompareDelivery, NULL))) ||
		(!Sent && (Record->Identifier || Record->Code || Record->Value ||
			(Session->Version == XMQTT_VERSION_5 && Finite != !!Expiry) ||
			(Finite && Record->Milliseconds >
				(Expiry ? UINT64_C(1000) * Expiry->Value.Number : XMQTT_SERVER_MAX_LIFETIME_MS)))))
		return xmqttServerError(XERR_PROTOCOL, "invalid MQTT checkpoint outgoing phase or expiry");
	if (!Sent && Finite && Elapsed >= Record->Milliseconds) return true;
	double Expires = Sent ? (Expiry ? 1 : 0) :
		(Finite ? xmqttServerRestoreDeadline(Record->Milliseconds, Elapsed, Now) : 0);
	xmqttserverpublication* Publication = xmqttServerEncodePublish(&Packet, Session->Version,
		Session->Version, Packet.Flags, true, Expires, &Codec);
	if (!Publication) return false;
	xmqttserverdelivery* Item = xmqttServerNewDelivery(Session, Publication);
	xmqttServerFreePublication(Publication, NULL, 0);
	if (!Item) return xmqttServerError(XERR_AGAIN, "cannot admit MQTT checkpoint outgoing PUBLISH");
	if (Sent) {
		uint16 Last = Session->LastIdentifier;
		xmqttServerRememberDelivery(Session, Item, Record->Identifier);
		Session->LastIdentifier = Last;
		Item->ExpectedAck = Record->Code;
		bytes Prefix = Item->Prefix + Publication->IdOffset;
		Prefix[0] = (uint8)(Record->Identifier >> 8);
		Prefix[1] = (uint8)Record->Identifier;
		if (Publication->ExpiryOffset) {
			Prefix = Item->Prefix + Publication->ExpiryOffset;
			for (unsigned Byte = 0; Byte < 4; ++Byte) Prefix[Byte] = (uint8)(Record->Value >> ((3u - Byte) * 8u));
		}
		Publication->Expires = 0;
	} else {
		*Session->OfflineTail = Item;
		Session->OfflineTail = &Item->Next;
	}
	return true;
}

static bool xmqttServerRestoreWill(xmqttserversession* Session, const xmqttserversessionrecord* Record,
	uint64 Elapsed, double Now)
{
	xmqttserver* Server = Session->Server;
	if (Session->Will || Session->Version != XMQTT_VERSION_5 || Record->Milliseconds > XMQTT_SERVER_MAX_LIFETIME_MS)
		return xmqttServerError(XERR_PROTOCOL, "invalid or duplicate MQTT checkpoint Will");
	xmqttproperty Properties[XMQTT_SERVER_MAX_PROPERTIES];
	const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
	xmqttpacket Packet;
	if (!xmqttServerStoredPublish(Session, Record->Data, &Server->Config.Codec, &Scratch, &Packet)) return false;
	if (Packet.Body.Publish.Topic.Size > Server->Config.MaxTopicBytes ||
		((Packet.Flags & XMQTT_PUBLISH_RETAIN) && !Server->Config.MaxRetainedTopics))
		return xmqttServerError(XERR_RANGE, "MQTT checkpoint Will exceeds broker limits");
	size_t Bytes = sizeof(xmqttserverwill) + Record->Data.Size + Session->ClientName.Size;
	if (!xmqttServerReserveBytes(&Server->WillBytes, Server->Config.MaxWillBytes, Bytes))
		return xmqttServerError(XERR_RANGE, "MQTT checkpoint exceeds broker Will bytes");
	xmqttserverwill* Will = xrtMalloc(Bytes);
	if (!Will) {
		xrtAtomic64FetchSub(&Server->WillBytes, Bytes, XMEMORY_RELAXED);
		return false;
	}
	*Will = (xmqttserverwill){
		.Due = fmin(Session->Expires, xmqttServerRestoreDeadline(Record->Milliseconds, Elapsed, Now)),
		.Identity = Session->Identity, .Version = Session->Version,
		.Bytes = Record->Data.Size, .ClientBytes = Session->ClientName.Size
	};
	memcpy(Will->Wire, Record->Data.Data, Will->Bytes);
	memcpy(Will->Wire + Will->Bytes, Session->ClientName.Data, Will->ClientBytes);
	Session->Will = Will;
	return true;
}

static bool xmqttServerRestoreSessionRecord(xmqttserversession* Session, const xmqttserversessionrecord* Record,
	uint64 Elapsed, double Now)
{
	if (!Session || !xrtMemRangeValid(Record->Data.Data, Record->Data.Size) ||
		(Record->Kind != XMQTT_SERVER_RECORD_SUBSCRIPTION && Record->Group.Size) ||
		(Record->Kind != XMQTT_SERVER_RECORD_SENT && Record->Value))
		return xmqttServerError(XERR_PROTOCOL, "invalid MQTT session checkpoint record fields");
	switch (Record->Kind) {
	case XMQTT_SERVER_RECORD_SUBSCRIPTION:
		if (!Record->Identifier && !Record->Milliseconds)
			return xmqttServerRestoreSubscription(Session, Record);
		break;
	case XMQTT_SERVER_RECORD_INCOMING_QOS2:
		if (!Record->Data.Size && !Record->Milliseconds) return xmqttServerRestoreIncoming(Session, Record);
		break;
	case XMQTT_SERVER_RECORD_QUEUED:
	case XMQTT_SERVER_RECORD_SENT:
		return xmqttServerRestoreDelivery(Session, Record, Elapsed, Now);
	case XMQTT_SERVER_RECORD_WILL:
		if (!Record->Code && !Record->Identifier) return xmqttServerRestoreWill(Session, Record, Elapsed, Now);
		break;
	default:
		break;
	}
	return xmqttServerError(XERR_PROTOCOL, "invalid MQTT session checkpoint record kind or fields");
}

XMQTT_API bool xmqttServerRestoreSessions(xmqttserver* Server, const xmqttserversessionrecord* Records,
	size_t Count, uint64 ElapsedMilliseconds)
{
	if (!Server || Count > SIZE_MAX / sizeof(*Records) ||
		(Count && !xrtMemRangeValid(Records, Count * sizeof(*Records))))
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT session checkpoint records");
	const xmqttserverconfig* Config = &Server->Config;
	uint64 Max = UINT64_C(2) * Config->MaxPersistentSessions + Config->MaxSubscriptions +
		Config->MaxIncomingPublishes + Config->MaxOutgoingPublishes;
	if (Count > Max) return xmqttServerError(XERR_RANGE, "MQTT session checkpoint exceeds record limits");
	double Now = xrtTimer();
	if (xrtNetEngineCurrent(Server->Engine) || !isfinite(Now))
		return xmqttServerError(XERR_STATE, "restore MQTT session checkpoint on broker owner thread");
	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing || Server->Info.Accepted || Server->Offline || Server->ExpiryTimer) {
		xrtMutexUnlock(&Server->Gate);
		return xmqttServerError(XERR_STATE, "restore MQTT sessions before the first broker attachment");
	}
	xmqttserversession* Session = NULL;
	bool Restored = true;
	for (size_t Index = 0; Restored && Index < Count; ++Index) {
		const xmqttserversessionrecord* Record = Records + Index;
		if (Record->Kind == XMQTT_SERVER_RECORD_SESSION) {
			Session = xmqttServerRestoreSession(Server, Record, ElapsedMilliseconds, Now);
			Restored = Session != NULL;
		} else Restored = xmqttServerRestoreSessionRecord(Session, Record, ElapsedMilliseconds, Now);
	}
	if (Restored) {
		for (Session = Server->Offline; Session;) {
			xmqttserversession* Next = Session->OfflineNext;
			if (!Session->Will && Session->Expires <= Now) {
				xmqttServerForgetSession(Server, Session);
				xmqttServerFreeSession(Session);
			}
			Session = Next;
		}
		if (Server->Offline) {
			Server->ExpiryTimer = xrtNetEngineEvery(Server->Engine, 0, 1000, xmqttServerSessionTimer, Server);
			Restored = Server->ExpiryTimer != 0;
		}
	}
	if (!Restored) xmqttServerFreeOffline(xmqttServerDetachOffline(Server));
	xrtMutexUnlock(&Server->Gate);
	return Restored;
}
#endif

static void xmqttServerPeerClosed(xmqttserverpeer* Peer)
{
	Peer->Closing = true;
	xrtAtomic32Store(&Peer->OutgoingStopped, 1, XMEMORY_RELAXED);
	xmqttServerCancelTimer(Peer);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
	xmqttServerAuthCancelTimer(Peer);
#endif
	xmqttserversession* Session = Peer->Session;
	if (!Session) return;
	Peer->ClosedAt = xrtTimer();
	bool Replaced = xrtAtomic32Load(&Peer->TakenOver, XMEMORY_ACQUIRE) != 0;
	uint32 Expiry = xrtAtomic32Load(&Peer->SessionExpiry, XMEMORY_RELAXED);
	if (Session->Persistent && (((Peer->Online || Replaced) && Expiry) || (!Peer->Online && Peer->Resumed))) {
		uint32 State = XMQTT_SERVER_ONLINE;
		(void)xrtAtomic32CompareExchange(&Session->State, &State, XMQTT_SERVER_OFFLINE, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
	} else xrtAtomic32Store(&Session->State, XMQTT_SERVER_SEALED, XMEMORY_RELEASE);
}

static void xmqttServerFreeSession(xmqttserversession* Session)
{
	/* Wait for routing readers before freeing any queued or sent record. TCP
	 * has retired native buffers and all owner tasks have released their pins. */
	while (Session->Links) {
		xmqttserverlink* Link = Session->Links;
		Session->Links = Link->SessionNext;
		xmqttServerUnlinkTopic(Link);
	}
	while (Session->Incoming.Count) {
		xmqttserverack* Ack = (xmqttserverack*)xrtAVLFirst(&Session->Incoming);
		if (Ack->Peer) abort();
		xmqttServerReleaseAck(Ack, NULL, 0);
	}
	while (Session->Waiting) xmqttServerDiscardDelivery(xmqttServerPopWaiting(Session));
	while (Session->OfflineQueue) {
		xmqttserverdelivery* Delivery = Session->OfflineQueue;
		Session->OfflineQueue = Delivery->Next;
		xmqttServerDiscardDelivery(Delivery);
	}
	while (Session->SentHead) {
		xmqttserverdelivery* Delivery = Session->SentHead;
		Session->SentHead = Delivery->SentNext;
		xmqttServerDropDelivery(Delivery, NULL, 0);
	}
	if (xrtAtomic32Load(&Session->OutgoingCount, XMEMORY_RELAXED)) abort();
	xmqttServerDropWill(Session->Server, Session->Will);
	if (Session->Persistent) xrtMutexUnit(&Session->Gate);
	xrtFree(Session);
}

#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
static void xmqttServerOutputClosed(xmqttserverpeer* Peer)
{
	xmqttserveroutput* Out = Peer->Output;
	xrtMutexLock(&Out->Gate);
	Out->Stopped = Out->Closed = true;
	xrtMutexUnlock(&Out->Gate);
	xmqttServerPeerClosed(Peer);
	if (!Out->Driving) xmqttServerDiscardWrites(Out);
	xmqttServerPeerDrop(Peer);
}

static bool xmqttServerInitOutput(xmqttserverpeer* Peer, size_t Limit)
{
	xmqttserveroutput* Out = xrtCalloc(1, sizeof(*Out));
	if (!Out) return false;
	if (!xrtNetPostInit(&Out->Write) || !xrtMutexInit(&Out->Gate)) {
		xrtFree(Out);
		return false;
	}
	Out->Tail = &Out->Head;
	Out->Limit = Limit;
	Peer->Output = Out;
	return true;
}
#endif

static void xmqttServerClosed(xnetstream* Stream, xnetresult Result, const xerror* Cause, ptr Data)
{
	(void)Stream;
	(void)Result;
	(void)Cause;
	xmqttServerPeerClosed(Data);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
static void xmqttServerTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{
	xmqttserverpeer* Peer = Data;
	if (Peer->Output->Reading) return;
	Peer->Output->Reading = true;
	if (xmqttServerReadPackets(Peer, Buffer, NULL) && !xrtTlsStreamReadMore(Stream))
		xmqttServerAbortPeer(Peer);
	Peer->Output->Reading = false;
}

static void xmqttServerTlsWritable(xtlsstream* Stream, ptr Data)
{
	(void)Stream;
	xmqttServerDriveOutput(Data);
}

static void xmqttServerTlsEnd(xtlsstream* Stream, ptr Data)
{
	(void)Stream;
	xmqttserverpeer* Peer = Data;
	if (!Peer->Closing) xmqttServerFinishPeer(Peer, xrtTimer());
}

static void xmqttServerTlsClosed(xtlsstream* Stream, xnetresult Result, const xerror* Cause, ptr Data)
{
	(void)Stream;
	(void)Result;
	(void)Cause;
	xmqttServerOutputClosed(Data);
}

static const xtlsstreamevents xmqttServerTlsEvents = {
	.Read = xmqttServerTlsRead, .End = xmqttServerTlsEnd,
	.Writable = xmqttServerTlsWritable, .Drain = xmqttServerTlsWritable, .Close = xmqttServerTlsClosed
};

static bool xmqttServerBindTls(xmqttserverpeer* Peer, const xtlsserverconfig* Config,
	const xtlsstreamconfig* StreamConfig)
{
	if (!xmqttServerInitOutput(Peer, xrtNetStreamWriteLimit(Peer->Stream))) return false;
	return xrtTlsStreamAccept(Peer->Stream, Config, StreamConfig, &xmqttServerTlsEvents, Peer, &Peer->Tls);
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
static bool xmqttServerWsAppend(xmqttserverpeer* Peer, cbytes Data, size_t Bytes)
{
	if (!Bytes) return true;
	size_t Limit = Peer->Server->Config.Codec.MaxPacketBytes;
	if (Bytes > Limit - Peer->WsBytes) return false;
	size_t Need = Peer->WsBytes + Bytes;
	if (Need > Peer->WsCapacity - Peer->WsFirst) {
		if (Peer->WsFirst) {
			memmove(Peer->WsInput, Peer->WsInput + Peer->WsFirst, Peer->WsBytes);
			Peer->WsFirst = 0;
		}
		if (Need > Peer->WsCapacity) {
			size_t Capacity = Peer->WsCapacity ? Peer->WsCapacity : (Limit < 64 ? Limit : 64);
			while (Capacity < Need) Capacity = Capacity > Limit / 2 ? Limit : Capacity * 2;
			unsigned char* Input = xrtRealloc(Peer->WsInput, Capacity);
			if (!Input) return false;
			Peer->WsInput = Input;
			Peer->WsCapacity = Capacity;
		}
	}
	memcpy(Peer->WsInput + Peer->WsFirst + Peer->WsBytes, Data, Bytes);
	Peer->WsBytes = Need;
	return true;
}

static void xmqttServerWsRead(xwsstream* Stream, xbytesview Data, ptr User)
{
	(void)Stream;
	xmqttserverpeer* Peer = User;
	while ((Data.Size || Peer->WsBytes) && xmqttServerCanRead(Peer)) {
		bool Staged = Peer->WsBytes != 0;
		xbytesview Input = Staged ? (xbytesview){Peer->WsInput + Peer->WsFirst, Peer->WsBytes} : Data;
		xmqttfixedheader Header;
		xmqttwirestatus Status = xmqttServerReadHeader(Peer, Input, &Header);
		if (Status == XMQTT_WIRE_ERROR) {
			xmqttServerAbortPeer(Peer);
			return;
		}
		if (Status == XMQTT_WIRE_MORE || Input.Size < Header.PacketBytes) {
			if (!Staged) {
				if (!xmqttServerWsAppend(Peer, Data.Data, Data.Size)) xmqttServerAbortPeer(Peer);
				return;
			}
			if (!Data.Size) return;
			size_t Need = Status == XMQTT_WIRE_MORE ? 1 : Header.PacketBytes - Input.Size;
			size_t Bytes = Need < Data.Size ? Need : Data.Size;
			if (!xmqttServerWsAppend(Peer, Data.Data, Bytes)) {
				xmqttServerAbortPeer(Peer);
				return;
			}
			Data.Data += Bytes;
			Data.Size -= Bytes;
			continue;
		}
		if (!xmqttServerReadPacket(Peer, &Header, Input.Data)) return;
		if (Staged) {
			Peer->WsFirst += Header.PacketBytes;
			Peer->WsBytes -= Header.PacketBytes;
			if (!Peer->WsBytes) Peer->WsFirst = 0;
		} else {
			Data.Data += Header.PacketBytes;
			Data.Size -= Header.PacketBytes;
		}
	}
	/* Pause takes effect at the end of this native callback. Preserve its
	 * unconsumed suffix until the pending operation finishes. */
	bool Paused = Peer->AwaitingSession != NULL;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	Paused = Paused || Peer->SessionCommit != NULL;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	Paused = Paused || Peer->Retaining != NULL;
#endif
	if (!Peer->Closing && Paused && !xmqttServerWsAppend(Peer, Data.Data, Data.Size))
		xmqttServerAbortPeer(Peer);
}

static void xmqttServerWsBegin(xwsstream* Stream, const xwsmessageinfo* Info, ptr Data)
{
	xmqttserverpeer* Peer = Data;
	if (Info->Opcode == XWS_OPCODE_BINARY || Peer->Closing) return;
	Peer->Closing = true;
	if (xrtWsStreamClose(Stream, 1003, (xstrview){0}) != XNET_RESULT_OK)
		xmqttServerAbortTransport(Peer);
}

static void xmqttServerWsDrain(xwsstream* Stream, ptr Data)
{
	(void)Stream;
	xmqttServerDriveOutput(Data);
}

static void xmqttServerWsWritable(xwsstream* Stream, size_t Pending, ptr Data)
{
	(void)Pending;
	xmqttServerWsDrain(Stream, Data);
}

static void xmqttServerWsClosed(xwsstream* Stream, const xwsstreamclose* Close, ptr Data)
{
	(void)Stream;
	(void)Close;
	xmqttServerOutputClosed(Data);
}

static const xwsstreamevents xmqttServerWsEvents = {
	.MessageBegin = xmqttServerWsBegin, .MessageData = xmqttServerWsRead,
	.Writable = xmqttServerWsWritable, .Drain = xmqttServerWsDrain, .Close = xmqttServerWsClosed
};

typedef struct xmqttserverwsattach {
	const xwsstreamconfig* Config;
	size_t Prefix;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS_TLS)
	xtlsstream* Tls;
#endif
} xmqttserverwsattach;

static bool xmqttServerBindWs(xmqttserverpeer* Peer, const xmqttserverwsattach* Plan)
{
	size_t Limit = xrtNetStreamWriteLimit(Peer->Stream);
	if (Plan->Config->SendLimit < Limit) Limit = Plan->Config->SendLimit;
	if (!xmqttServerInitOutput(Peer, Limit)) return false;
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS_TLS)
	if (Plan->Tls)
		Peer->Ws = xrtWsStreamAttachTls(Plan->Tls, Plan->Prefix, Plan->Config, &xmqttServerWsEvents, Peer);
	else
#endif
		Peer->Ws = xrtWsStreamAttach(Peer->Stream, Plan->Prefix, Plan->Config, &xmqttServerWsEvents, Peer);
	return Peer->Ws != NULL;
}
#endif

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
		.MaxPersistentSessions = 1024, .MaxSubscriptions = 65536, .MaxSubscriptionsPerConnection = 64,
		.MaxSubscribersPerTopic = 1024, .MaxTopicBytes = 1024, .MaxWildcardNodes = 65536,
		.MaxRetainedTopics = 4096, .MaxRetainedBytes = 16u * 1024u * 1024u,
		.MaxWillBytes = 16u * 1024u * 1024u,
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
		!Config->MaxClientIdBytes || Config->MaxClientIdBytes > UINT16_MAX || Config->MaxPersistentSessions > UINT32_MAX ||
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
	xrtAtomic64Init(&Server->WillBytes, 0);
	xrtAtomic64Init(&Server->DroppedRetained, 0);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
	xrtAtomic64Init(&Server->RetainedPendingBytes, 0);
	xrtAtomic32Init(&Server->RetainedFailed, 0);
#endif
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

#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
XMQTT_API bool xmqttServerAuthConfigInit(xmqttserverauthconfig* Config)
{
	if (!xrtMemRangeValid(Config, sizeof(*Config)))
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT enhanced authentication config");
	*Config = (xmqttserverauthconfig){
		.Size = sizeof(*Config), .MaxRounds = 32, .MaxExchangeBytes = 1024u * 1024u, .ReauthTimeout = 5000
	};
	return true;
}

XMQTT_API bool xmqttServerCreateAuth(xnetengine* Engine, const xmqttserverconfig* Config,
	const xmqttserverauthconfig* Auth, xmqttserver** Out)
{
	if (!xrtMemRangeValid(Auth, sizeof(*Auth)) || !xrtMemRangeValid(Out, sizeof(*Out)) ||
		xrtMemRangesOverlap(Auth, sizeof(*Auth), Out, sizeof(*Out)) ||
		xrtMemRangesOverlap(Config, sizeof(*Config), Out, sizeof(*Out)) ||
		Auth->Size != sizeof(*Auth) || !Auth->MaxRounds || Auth->MaxRounds > 65536u ||
		!Auth->MaxExchangeBytes || Auth->ReauthTimeout < 1 || Auth->ReauthTimeout > INT32_MAX ||
		!Auth->Step || !Auth->Release)
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT enhanced authentication setup");
	xmqttserver* Server;
	if (!xmqttServerCreate(Engine, Config, &Server)) return false;
	Server->Auth = *Auth;
	*Out = Server;
	return true;
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
XMQTT_API bool xmqttServerSetRetainedCommit(xmqttserver* Server, size_t PendingBytes,
	xmqttserverretainedsubmit Submit, ptr User)
{
	if (!Server || !Server->Config.MaxRetainedTopics || !PendingBytes || !Submit)
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT retained commit setup");
	xrtMutexLock(&Server->Gate);
	/* Install before restoring sessions so neither pending Wills nor the
	 * expiry worker can bypass this backend. Retained bootstrap is independent. */
	if (Server->Info.Closing || Server->Info.Accepted || Server->ExpiryTimer || Server->RetainedSubmit) {
		xrtMutexUnlock(&Server->Gate);
		return xmqttServerError(XERR_STATE, "configure MQTT retained commit before attachment and session restore");
	}
	Server->RetainedSubmit = Submit;
	Server->RetainedUser = User;
	Server->RetainedPendingLimit = PendingBytes;
	xrtMutexUnlock(&Server->Gate);
	return true;
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
static bool xmqttServerConfigureSessionCommit(xmqttserver* Server,
	xmqttserversessionresetsubmit Reset, xmqttserversessionsubmit Submit, ptr User)
{
	if (!Server || (!Reset && !Submit)) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT session commit setup");
	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing || Server->Info.Accepted || Server->ExpiryTimer ||
		Server->ResetSubmit || Server->SessionSubmit) {
		xrtMutexUnlock(&Server->Gate);
		return xmqttServerError(XERR_STATE, "configure MQTT session commit before attachment and restore");
	}
	Server->ResetSubmit = Reset;
	Server->SessionSubmit = Submit;
	Server->SessionStoreUser = User;
	xrtMutexUnlock(&Server->Gate);
	return true;
}

XMQTT_API bool xmqttServerSetSessionReset(xmqttserver* Server,
	xmqttserversessionresetsubmit Submit, ptr User)
{
	return xmqttServerConfigureSessionCommit(Server, Submit, NULL, User);
}

XMQTT_API bool xmqttServerSetSessionCommit(xmqttserver* Server,
	xmqttserversessionsubmit Submit, ptr User)
{
	return xmqttServerConfigureSessionCommit(Server, NULL, Submit, User);
}
#endif

static bool xmqttServerAttachNative(xmqttserver* Server, xnetstream* Stream
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
	, const xmqttserverwsattach* Ws
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	, const xtlsserverconfig* Tls, const xtlsstreamconfig* TlsStream
#endif
)
{
	if (!Server || !Stream) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT server attachment");
	xnetworker* Worker = xrtNetStreamWorker(Stream);
	if (!Worker || xrtNetWorkerEngine(Worker) != Server->Engine || !xrtNetWorkerIsCurrent(Worker))
		return xmqttServerError(XERR_STATE, "MQTT attach requires the accepted stream worker");

	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing || Server->Info.Connections == Server->Config.MaxConnections
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
		|| Server->SessionStoreFailed
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
		|| xrtAtomic32Load(&Server->RetainedFailed, XMEMORY_ACQUIRE)
#endif
	) {
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
	xrtAtomic32Init(&Peer->TakenOver, 0);
	xrtAtomic32Init(&Peer->SessionExpiry, 0);
	Peer->Server = Server;
	Peer->Stream = Stream;
	Peer->Worker = xrtNetWorkerIndex(Worker);
	double Now = xrtTimer();
	if (!xrtNetPostInit(&Peer->Retirement) || !xrtNetPostInit(&Peer->OutgoingOverload) || !xrtNetPostInit(&Peer->Takeover) ||
		!xmqttServerDeadline(Peer, Now, (unsigned)Server->Config.ConnectTimeout) ||
		!(
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
			Ws ? xmqttServerBindWs(Peer, Ws) :
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
			Tls ? xmqttServerBindTls(Peer, Tls, TlsStream) :
#endif
			xrtNetStreamSetEventsOwnedV1(Stream, Peer, &xmqttServerEvents))) {
		xmqttServerCancelTimer(Peer);
#if defined(XMQTT_SERVER_QUEUED_OUTPUT)
		xmqttServerFreeOutput(Peer->Output);
#endif
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

XMQTT_API bool xmqttServerAttach(xmqttserver* Server, xnetstream* Stream)
{
	return xmqttServerAttachNative(Server, Stream
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
		, NULL
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
		, NULL, NULL
#endif
	);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
XMQTT_API bool xmqttServerAttachTls(xmqttserver* Server, xnetstream* Stream,
	const xtlsserverconfig* Tls, const xtlsstreamconfig* TlsStream)
{
	if (!Server || !Stream || !Tls || !Tls->Context)
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT TLS attachment");
	const xtlslimits* Limits = xrtTlsContextLimits(Tls->Context);
	if (!Limits || Limits->PlainLimit < XTLS_RECORD_PLAINTEXT_MAX ||
		Server->Config.Codec.MaxPacketBytes > Limits->PlainLimit - XTLS_RECORD_PLAINTEXT_MAX)
		return xmqttServerError(XERR_ARGUMENT, "MQTT TLS plaintext limit must allow an extra record");
	return xmqttServerAttachNative(Server, Stream,
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
		NULL,
#endif
		Tls, TlsStream);
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
static bool xmqttServerAttachWebSocket(xmqttserver* Server, xnetstream* Stream,
	const xmqttserverwsattach* Plan)
{
	const xwsstreamconfig* Config = Plan->Config;
	if (!Server || !Stream || !Config || !xrtWsStreamConfigValid(Config) ||
		Config->Role != XWS_ROLE_SERVER || Config->Protocol.Size != 4 ||
		memcmp(Config->Protocol.Data, "mqtt", 4) || Config->DeflateEnabled ||
		Config->FrameLimit < Server->Config.Codec.MaxPacketBytes ||
		Config->MessageLimit < Server->Config.Codec.MaxPacketBytes)
		return xmqttServerError(XERR_ARGUMENT, "invalid MQTT WebSocket attachment");
	return xmqttServerAttachNative(Server, Stream, Plan
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
		, NULL, NULL
#endif
	);
}

XMQTT_API bool xmqttServerAttachWs(xmqttserver* Server, xnetstream* Stream,
	size_t Prefix, const xwsstreamconfig* Config)
{
	const xmqttserverwsattach Plan = {.Config = Config, .Prefix = Prefix};
	return xmqttServerAttachWebSocket(Server, Stream, &Plan);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS_TLS)
XMQTT_API bool xmqttServerAttachWss(xmqttserver* Server, xtlsstream* Stream,
	size_t Prefix, const xwsstreamconfig* Config)
{
	const xmqttserverwsattach Plan = {.Config = Config, .Prefix = Prefix, .Tls = Stream};
	return xmqttServerAttachWebSocket(Server, Stream ? xrtTlsStreamTransport(Stream) : NULL, &Plan);
}
#endif
#endif

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
	Info->WillBytes = (size_t)xrtAtomic64Load(&Server->WillBytes, XMEMORY_RELAXED);
	Info->DroppedRetained = xrtAtomic64Load(&Server->DroppedRetained, XMEMORY_RELAXED);
	Info->Forwarded = xrtAtomic64Load(&Server->Forwarded, XMEMORY_RELAXED);
	Info->DroppedDeliveries = xrtAtomic64Load(&Server->DroppedDeliveries, XMEMORY_RELAXED);
	Info->RejectedPublications = xrtAtomic64Load(&Server->RejectedPublications, XMEMORY_RELAXED);
	return true;
}

static bool xmqttServerCloseMode(xmqttserver* Server, bool Checkpoint)
{
	if (!Server) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT server");
	xmqttserversession* Garbage = NULL;
	xrtMutexLock(&Server->Gate);
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	if (Checkpoint && Server->Info.Closing && !Server->Checkpoint) {
		xrtMutexUnlock(&Server->Gate);
		return xmqttServerError(XERR_STATE, "MQTT broker already closed without a session checkpoint");
	}
#endif
	if (!Server->Info.Closing) {
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
		Server->Checkpoint = Checkpoint;
#endif
		Server->Info.Closing = true;
		if (!Checkpoint) Garbage = xmqttServerDetachOffline(Server);
		if (Garbage) ++Server->SessionCleanup;
		for (xmqttserverpeer* Peer = Server->Peers; Peer; Peer = Peer->Next)
			xmqttServerAbortTransport(Peer);
		if (Server->ExpiryTimer) (void)xrtNetEngineTimerCancel(Server->Engine, Server->ExpiryTimer);
	}
	xrtMutexUnlock(&Server->Gate);
	if (Garbage) {
		xmqttServerFreeOffline(Garbage);
		xrtMutexLock(&Server->Gate);
		--Server->SessionCleanup;
		xmqttServerSignalDrained(Server);
		xrtMutexUnlock(&Server->Gate);
	}
	return true;
}

XMQTT_API bool xmqttServerClose(xmqttserver* Server)
{
	return xmqttServerCloseMode(Server, false);
}

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
XMQTT_API bool xmqttServerCloseForCheckpoint(xmqttserver* Server)
{
	if (!Server) return xmqttServerError(XERR_ARGUMENT, "invalid MQTT broker checkpoint close");
	if (xrtNetEngineCurrent(Server->Engine))
		return xmqttServerError(XERR_STATE, "close for checkpoint outside MQTT broker workers");
	return xmqttServerCloseMode(Server, true);
}
#endif

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
	while (Result == XWAIT_OK && !xmqttServerDrained(Server)) {
		int64 Remaining = __xrtWaitRemaining(Deadline);
		if (Remaining < XRT_WAIT_FOREVER) Result = XWAIT_ERROR;
		else if (!Remaining) Result = XWAIT_TIMEOUT;
		else Result = xrtCondWaitFor(&Server->Drained, &Server->Gate, Remaining);
	}
	if (Result == XWAIT_TIMEOUT && xmqttServerDrained(Server))
		Result = XWAIT_OK;
	xrtMutexUnlock(&Server->Gate);
	return Result;
}

XMQTT_API bool xmqttServerDestroy(xmqttserver* Server)
{
	if (!Server) return true;
	if (xmqttServerWaitFor(Server, 0) != XWAIT_OK)
		return xmqttServerError(XERR_STATE, "MQTT server still owns connections or is open");
#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
	if (Server->Offline) xmqttServerFreeOffline(xmqttServerDetachOffline(Server));
#endif
	xmqttServerUnitTopics(Server);
	xrtMapUnit(&Server->Identities);
	xrtCondUnit(&Server->Drained);
	xrtMutexUnit(&Server->Gate);
	xrtNetEngineUnpin(Server->Engine);
	xrtFree(Server);
	return true;
}

#endif
