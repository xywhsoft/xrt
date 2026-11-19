#include <xmqtt/server.h>
#include <xmqtt/codec311.h>
#include <xrt/atomic.h>
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

typedef struct xmqttserverpeer xmqttserverpeer;
struct xmqttserver {
	xnetengine* Engine;
	xmqttserverconfig Config;
	xmutex Gate;
	xcond Drained;
	xsipkey HashKey;
	xmap Identities;
	xmqttserverpeer* Peers;
	xmqttserverinfo Info;
};

struct xmqttserverpeer {
	xatomic32 References;
	xmqttserver* Server;
	xnetstream* Stream;
	xnetpost Retirement;
	xmqttserverpeer *Previous, *Next;
	xbytesview ClientId; /* Borrows the map key while registered, under Gate. */
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

static void xmqttServerPeerRetire(xnetworker* Worker, ptr Data)
{
	(void)Worker;
	xmqttserverpeer* Peer = Data;
	xmqttserver* Server = Peer->Server;
	xrtMutexLock(&Server->Gate);
	if (Peer->ClientId.Size) {
		xrtMapRemove(&Server->Identities, Peer->ClientId);
		--Server->Info.Established;
	}
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
			{XMQTT_PROPERTY_MAXIMUM_QOS, XMQTT_PROPERTY_BYTE, {.Number = 0}},
			{XMQTT_PROPERTY_RETAIN_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
			{XMQTT_PROPERTY_WILDCARD_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
			{XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
			{XMQTT_PROPERTY_SHARED_AVAILABLE, XMQTT_PROPERTY_BYTE, {.Number = 0}},
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

static bool xmqttServerClaimId(xmqttserverpeer* Peer, xstrview Id)
{
	xmqttserver* Server = Peer->Server;
	xbytesview Key = {(cbytes)Id.Data, Id.Size};
	xrtMutexLock(&Server->Gate);
	if (Server->Info.Closing) {
		xrtMutexUnlock(&Server->Gate);
		return false;
	}
	bool New;
	xmqttserverpeer** Entry = xrtMapGetOrAdd(&Server->Identities, Key, &New);
	if (!Entry) {
		xrtMutexUnlock(&Server->Gate);
		return false;
	}
	if (!New) {
		xmqttserverpeer* Old = *Entry;
		Old->ClientId = (xbytesview){0};
		xrtNetStreamAbort(Old->Stream);
	} else {
		++Server->Info.Established;
	}
	if (!xrtMapStoredKey(&Server->Identities, Key, &Peer->ClientId)) abort();
	*Entry = Peer;
	xrtMutexUnlock(&Server->Gate);
	return true;
}

static void xmqttServerConnect(xmqttserverpeer* Peer, const xmqttpacket* Packet, double Now)
{
	xmqttserver* Server = Peer->Server;
	const xmqttconnectpacket* Connect = &Packet->Body.Connect;
	uint32 SessionExpiry = 0;
	size_t PacketLimit = XMQTT_PACKET_MAX_BYTES;
	bool AuthMethod = false;
	for (size_t Index = 0; Index < Packet->Properties.Count; ++Index) {
		const xmqttproperty* Property = &Packet->Properties.Data[Index];
		if (Property->Id == XMQTT_PROPERTY_SESSION_EXPIRY) SessionExpiry = Property->Value.Number;
		else if (Property->Id == XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE) PacketLimit = Property->Value.Number;
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
	if (!xmqttServerClaimId(Peer, Connect->ClientId)) {
		xmqttServerRefuse(Peer, 0x88, 3, PacketLimit, Now);
		return;
	}
	Peer->KeepAliveMs = (unsigned)Connect->KeepAlive * 1500u;
	if (!xmqttServerDeadline(Peer, Now, Peer->KeepAliveMs) ||
		!xmqttServerWriteConnack(Peer, 0, 0, PacketLimit)) {
		xmqttServerAbortPeer(Peer);
		return;
	}
	Peer->Online = true;
}

static void xmqttServerRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	xmqttserverpeer* Peer = Data;
	const xmqttcodecconfig* Codec = &Peer->Server->Config.Codec;
	size_t Available;
	while (!Peer->Closing && (Available = xrtNetBufSize(Buffer)) != 0) {
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
			const xmqttcodecbuffers Scratch = {Properties, XMQTT_SERVER_MAX_PROPERTIES, NULL, 0};
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
			else if (Peer->Online && Packet.Type == XMQTT_PACKET_DISCONNECT)
				xmqttServerFinishPeer(Peer, Now);
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

static void xmqttServerClosed(xnetstream* Stream, xnetresult Result, const xerror* Cause, ptr Data)
{
	(void)Stream;
	(void)Result;
	(void)Cause;
	xmqttServerCancelTimer(Data);
}

static const xnetstreameventsownershipv1 xmqttServerEvents = {
	sizeof(xnetstreameventsownershipv1),
	{.Read = xmqttServerRead, .End = xmqttServerEnd, .Close = xmqttServerClosed},
	xmqttServerPeerDrop, &xmqttServerPeerOps
};

XMQTT_API bool xmqttServerConfigInit(xmqttserverconfig* Config)
{
	if (!xrtMemRangeValid(Config, sizeof(*Config))) return xmqttServerError(XERR_ARGUMENT, "invalid server config");
	*Config = (xmqttserverconfig){
		.Size = sizeof(*Config), .MaxConnections = 1024, .MaxClientIdBytes = 256, .ConnectTimeout = 5000
	};
	xmqttCodecConfigInit(&Config->Codec);
	Config->Codec.MaxPacketBytes = 16384;
	Config->Codec.MaxProperties = XMQTT_SERVER_MAX_PROPERTIES;
	return true;
}

XMQTT_API bool xmqttServerCreate(xnetengine* Engine, const xmqttserverconfig* Config, xmqttserver** Out)
{
	if (!Engine || !xrtMemRangeValid(Config, sizeof(*Config)) || !xrtMemRangeValid(Out, sizeof(*Out)) ||
		xrtMemRangesOverlap(Config, sizeof(*Config), Out, sizeof(*Out)) ||
		Config->Size != sizeof(*Config) || !Config->MaxConnections || Config->MaxConnections > UINT32_MAX ||
		!Config->MaxClientIdBytes || Config->MaxClientIdBytes > UINT16_MAX ||
		Config->ConnectTimeout < 1 || Config->ConnectTimeout > INT32_MAX ||
		Config->Codec.Size != sizeof(Config->Codec) || Config->Codec.MaxPacketBytes < 2 ||
		Config->Codec.MaxPacketBytes > XMQTT_PACKET_MAX_BYTES ||
		Config->Codec.MaxProperties > XMQTT_SERVER_MAX_PROPERTIES ||
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
	bool Gate = xrtMutexInit(&Server->Gate);
	bool Cond = Gate && xrtCondInit(&Server->Drained);
	bool Map = Cond && xrtMapInit(&Server->Identities, sizeof(xmqttserverpeer*));
	if (Map && xrtSecureRandom(&Server->HashKey, sizeof(Server->HashKey)) &&
		xrtMapSetKeyPolicy(&Server->Identities, xmqttServerIdHash, xmqttServerIdEqual, &Server->HashKey)) {
		*Out = Server;
		return true;
	}
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
	Peer->Server = Server;
	Peer->Stream = Stream;
	Peer->Worker = xrtNetWorkerIndex(Worker);
	double Now = xrtTimer();
	if (!xrtNetPostInit(&Peer->Retirement) || !xmqttServerDeadline(Peer, Now, (unsigned)Server->Config.ConnectTimeout) ||
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
	xrtMapUnit(&Server->Identities);
	xrtCondUnit(&Server->Drained);
	xrtMutexUnit(&Server->Gate);
	xrtNetEngineUnpin(Server->Engine);
	xrtFree(Server);
	return true;
}

#endif
