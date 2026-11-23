#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <windows.h>
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#endif
#include "../examples/server/app_listener.h"
#include <xrt/memory_stats.h>
#if defined(XMQTT_BENCH_TLS)
#include <xrt/tls_client.h>
#include <xrt/tls_stream.h>
#include <xrt/tls_verify.h>
#include <xrt/x509.h>
#if !defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || !defined(XRT_FEATURE_TLS_RECORD_AES) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	!defined(XRT_FEATURE_TLS_STREAM_FUTURE) || !defined(XRT_FEATURE_TLS_STREAM_RETIRE)
#error "See the verified TLS broker comparison build flags in bench/README.md"
#endif
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double benchObserverCpu(bool Pacer)
{
#if defined(_WIN32) || defined(_WIN64)
	FILETIME Created, Exited, Kernel, User;
	BOOL OK = Pacer ? GetThreadTimes(GetCurrentThread(), &Created, &Exited, &Kernel, &User) :
		GetProcessTimes(GetCurrentProcess(), &Created, &Exited, &Kernel, &User);
	if (!OK) return -1;
	uint64 Work = ((uint64)Kernel.dwHighDateTime << 32) + Kernel.dwLowDateTime;
	Work += ((uint64)User.dwHighDateTime << 32) + User.dwLowDateTime;
	return (double)Work / 10000000.0;
#else
	struct timespec Work;
	if (clock_gettime(Pacer ? CLOCK_THREAD_CPUTIME_ID : CLOCK_PROCESS_CPUTIME_ID, &Work) != 0) return -1;
	return (double)Work.tv_sec + (double)Work.tv_nsec / 1000000000.0;
#endif
}

static bool benchSend(xnetsocket Socket, const unsigned char* Data, size_t Size)
{
	while (Size) {
		size_t Sent;
		if (xrtNetSocketSend(Socket, Data, Size, &Sent) != XNET_RESULT_OK || !Sent) return false;
		Data += Sent;
		Size -= Sent;
	}
	return true;
}

typedef struct benchConnection {
	xnetsocket Socket;
#if defined(XMQTT_BENCH_TLS)
	xtlsstream* Tls;
#endif
} benchConnection;

#if defined(XMQTT_BENCH_TLS)
static bool benchTlsAwait(xfuture* Future, const xatomic32* Stop)
{
	if (!Future) return false;
	xwaitresult Result;
	do {
		Result = xrtFutureWaitFor(Future, Stop ? 200 : 5000);
	} while (Result == XWAIT_TIMEOUT && Stop && !xrtAtomic32Load(Stop, XMEMORY_ACQUIRE));
	if (Result != XWAIT_OK) (void)xrtFutureCancel(Future);
	return Result == XWAIT_OK && xrtFutureState(Future) == XFUTURE_RESOLVED;
}

static bool benchTlsFinish(xfuture* Future)
{
	bool OK = benchTlsAwait(Future, NULL);
	if (Future) xrtFutureDestroy(Future);
	return OK;
}
#endif

static bool benchWireSend(benchConnection Connection, const unsigned char* Data, size_t Size)
{
#if defined(XMQTT_BENCH_TLS)
	if (Connection.Tls) return benchTlsFinish(xrtTlsStreamSendAsync(Connection.Tls, Data, Size));
#endif
	return benchSend(Connection.Socket, Data, Size);
}

static size_t benchWireRead(benchConnection Connection, unsigned char* Data, size_t Size, const xatomic32* Stop)
{
#if defined(XMQTT_BENCH_TLS)
	if (Connection.Tls) {
		xfuture* Future = xrtTlsStreamRecvAsync(Connection.Tls, Size);
		size_t Read = 0;
		if (benchTlsAwait(Future, Stop)) {
			xnetbytes* Bytes = xrtFutureValue(Future);
			xbytesview View = Bytes ? xrtNetBytesView(Bytes) : (xbytesview){0};
			if (View.Size && View.Size <= Size) {
				memcpy(Data, View.Data, View.Size);
				Read = View.Size;
			}
		}
		if (Future) xrtFutureDestroy(Future);
		return Read;
	}
#endif
	(void)Stop;
	size_t Read = 0;
	return xrtNetSocketRecv(Connection.Socket, Data, Size, &Read) == XNET_RESULT_OK ? Read : 0;
}

static bool benchWireReceive(benchConnection Connection, unsigned char* Data, size_t Size)
{
	while (Size) {
		size_t Read = benchWireRead(Connection, Data, Size, NULL);
		if (!Read) return false;
		Data += Read;
		Size -= Read;
	}
	return true;
}

static bool benchReceive(xnetsocket Socket, unsigned char* Data, size_t Size)
{
	return benchWireReceive((benchConnection){.Socket = Socket}, Data, Size);
}

static bool benchMqttConnect(benchConnection Connection, const unsigned char* Frame, size_t Size,
	bool Resume, uint16 RequiredWindow)
{
	unsigned char Reply[48];
	if (!benchWireSend(Connection, Frame, Size) || !benchWireReceive(Connection, Reply, 2) ||
		Reply[0] != 0x20 || Reply[1] < 3 || Reply[1] > sizeof(Reply) - 2 ||
		!benchWireReceive(Connection, Reply + 2, Reply[1]) || Reply[2] != Resume || Reply[3]) return false;
	if (!RequiredWindow) return true;
	xmqttproperty Properties[16];
	const xmqttcodecbuffers Buffers = {.Properties = Properties, .PropertyCapacity = 16};
	xmqttpacket Connack;
	if (xmqttPacketRead5((xbytesview){Reply, Reply[1] + 2u}, XMQTT_SERVER_TO_CLIENT, NULL, &Buffers,
		&Connack, NULL) != XMQTT_CODEC_DONE) return false;
	for (size_t I = 0; I < Connack.Properties.Count; ++I) {
		const xmqttproperty* Property = Connack.Properties.Data + I;
		if (Property->Id == XMQTT_PROPERTY_RECEIVE_MAXIMUM) return Property->Value.Number >= RequiredWindow;
	}
	return true; /* Omitting Receive Maximum advertises 65535. */
}

static bool benchConnectFrame(xnetsocket Socket, const xnetaddr* Address,
	const unsigned char* Frame, size_t Size, bool Resume, uint16 RequiredWindow)
{
	return xrtNetSocketSet(Socket, XNET_OPTION_NO_DELAY, 1) &&
		xrtNetSocketConnect(Socket, Address) == XNET_RESULT_OK &&
		benchMqttConnect((benchConnection){.Socket = Socket}, Frame, Size, Resume, RequiredWindow);
}

static size_t benchConnectWire(unsigned char Frame[31], size_t Index, uint16 Window, bool Persistent, bool Resume)
{
	/* Eight ASCII digits keep each identity distinct and valid in MQTT. */
	const unsigned char Header[] = {0x10,0,0,4,'M','Q','T','T',5,Resume ? 0 : 2,0,0,0};
	memcpy(Frame, Header, sizeof(Header));
	size_t Size = sizeof(Header);
	if (Window) {
		Frame[Size++] = 0x21;
		Frame[Size++] = (uint8)(Window >> 8);
		Frame[Size++] = (uint8)Window;
	}
	if (Persistent) {
		const unsigned char Expiry[] = {0x11,0,0,0,60};
		memcpy(Frame + Size, Expiry, sizeof(Expiry));
		Size += sizeof(Expiry);
	}
	Frame[12] = (uint8)(Size - 13);
	Frame[Size++] = 0;
	Frame[Size++] = 8;
	size_t First = Size;
	Size += 8;
	Frame[1] = (uint8)(Size - 2);
	for (size_t Pos = Size; Pos > First;) {
		Frame[--Pos] = '0' + (unsigned char)(Index % 10u);
		Index /= 10u;
	}
	return Size;
}

static bool benchConnect(xnetsocket Socket, const xnetaddr* Address, size_t Index, bool Window,
	bool Persistent, bool Resume)
{
	unsigned char Frame[31];
	size_t Size = benchConnectWire(Frame, Index, Window, Persistent, Resume);
	return benchConnectFrame(Socket, Address, Frame, Size, Resume, 0);
}

static const unsigned char ExactSubscribe[] = {0x82,10,0,1,0,0,4,'l','o','a','d',0};
static const unsigned char SubscribeAck[] = {0x90,4,0,1,0,0};
static const unsigned char RetainedFrame[138] = {0x31,0x87,1,0,4,'l','o','a','d',0};

static bool benchWireSubscribe(benchConnection Connection, bool Overlap, bool Shared, uint8 Qos)
{
	const unsigned char Group[] = {0x82,19,0,1,0,0,13,'$','s','h','a','r','e','/','g','/','l','o','a','d',0};
	const unsigned char Extra[] = {0x82,16,0,1,0,0,4,'l','o','a','d',0,0,3,'+','/','#',0};
	const unsigned char ExtraAck[] = {0x90,5,0,1,0,0,0};
	unsigned char Reply[sizeof(ExtraAck)];
	size_t Bytes = Overlap ? sizeof(ExtraAck) : sizeof(SubscribeAck);
	const unsigned char QosSubscribe[] = {0x82,10,0,1,0,0,4,'l','o','a','d',Qos};
	const unsigned char QosAck[] = {0x90,4,0,1,0,Qos};
	const unsigned char* Wire = Shared ? Group : (Overlap ? Extra : (Qos ? QosSubscribe : ExactSubscribe));
	size_t WireSize = Shared ? sizeof(Group) : (Overlap ? sizeof(Extra) : sizeof(ExactSubscribe));
	const unsigned char* Expected = Overlap ? ExtraAck : (Qos ? QosAck : SubscribeAck);
	return benchWireSend(Connection, Wire, WireSize) &&
		benchWireReceive(Connection, Reply, Bytes) && !memcmp(Reply, Expected, Bytes);
}

static bool benchSubscribe(xnetsocket Socket, bool Overlap, bool Shared, uint8 Qos)
{
	return benchWireSubscribe((benchConnection){.Socket = Socket}, Overlap, Shared, Qos);
}

static bool benchReceiveFlowReply(xnetsocket Socket, unsigned char Header, uint16 Id)
{
	const unsigned char Expected[] = {Header, 2, (uint8)(Id >> 8), (uint8)Id};
	unsigned char Reply[sizeof(Expected)];
	return benchReceive(Socket, Reply, sizeof(Reply)) && !memcmp(Reply, Expected, sizeof(Reply));
}

static bool benchWireFlowReply(benchConnection Connection, unsigned char Header, uint16 Id)
{
	const unsigned char Wire[] = {Header, 2, (uint8)(Id >> 8), (uint8)Id};
	return benchWireSend(Connection, Wire, sizeof(Wire));
}

static bool benchSendFlowReply(xnetsocket Socket, unsigned char Header, uint16 Id)
{
	return benchWireFlowReply((benchConnection){.Socket = Socket}, Header, Id);
}

static bool benchPublishRound(const xnetsocket* Sockets, size_t Peers, size_t Round, size_t Deliveries,
	bool Shared, uint8 Qos, bool Outgoing, double* Samples)
{
	/* MQTT 5 packet with 128 bytes of binary payload and no properties. */
	unsigned char Frame[138] = {0x30,0x87,1,0,4,'l','o','a','d',0};
	Frame[sizeof(Frame)-4] = (uint8)(Round >> 24);
	Frame[sizeof(Frame)-3] = (uint8)(Round >> 16);
	Frame[sizeof(Frame)-2] = (uint8)(Round >> 8);
	Frame[sizeof(Frame)-1] = (uint8)Round;
	unsigned char Publish[sizeof(Frame) + 2];
	const unsigned char* Input = Frame;
	size_t Bytes = sizeof(Frame);
	uint16 Identifier = (uint16)(Round + 1);
	if (Qos) {
		const size_t Prefix = 9;
		memcpy(Publish, Frame, Prefix);
		Publish[0] = 0x30 | (uint8)(Qos << 1);
		Publish[1] += 2;
		Publish[Prefix] = (uint8)(Identifier >> 8);
		Publish[Prefix + 1] = (uint8)Identifier;
		memcpy(Publish + Prefix + 2, Frame + Prefix, sizeof(Frame) - Prefix);
		Input = Publish;
		Bytes = sizeof(Publish);
	}
	double SentAt = xrtTimer();
	if (!benchSend(Sockets[Peers], Input, Bytes)) return false;
	if (Qos && !benchReceiveFlowReply(Sockets[Peers], Qos == 2 ? 0x50 : 0x40, Identifier)) return false;
	if (Qos == 2 && (!benchSendFlowReply(Sockets[Peers], 0x62, Identifier) ||
		!benchReceiveFlowReply(Sockets[Peers], 0x70, Identifier))) return false;
	size_t First = Round % Peers;
	/* Group membership adds to the head. The first joiner starts the
	 * cursor and later joiners are traversed in reverse join order. */
	if (Shared && First) First = Peers - First;
	for (size_t Offset = 0; Offset < Deliveries; ++Offset) {
		xnetsocket Socket = Sockets[(First + Offset) % Peers];
		unsigned char Reply[sizeof(Publish)];
		const unsigned char* Expected = Outgoing ? Publish : Frame;
		size_t ExpectedBytes = Outgoing ? sizeof(Publish) : sizeof(Frame);
		if (!benchReceive(Socket, Reply, ExpectedBytes) || memcmp(Reply, Expected, ExpectedBytes)) return false;
		if (Outgoing && !benchSendFlowReply(Socket, Qos == 2 ? 0x50 : 0x40, Identifier)) return false;
		if (Qos != 2) Samples[Offset] = xrtTimer() - SentAt;
	}
	if (Qos == 2) {
		/* Send each subscriber's PUBREC before waiting for any PUBREL. */
		for (size_t Offset = 0; Offset < Deliveries; ++Offset) {
			xnetsocket Socket = Sockets[(First + Offset) % Peers];
			if (!benchReceiveFlowReply(Socket, 0x62, Identifier) ||
				!benchSendFlowReply(Socket, 0x70, Identifier)) return false;
			Samples[Offset] = xrtTimer() - SentAt;
		}
	}
	return true;
}

static bool benchReplayRound(const xnetsocket* Sockets, size_t Peers, size_t Round,
	double* SentAt, double* Samples)
{
	size_t First = Round % Peers;
	for (size_t Offset = 0; Offset < Peers; ++Offset) {
		size_t Index = (First + Offset) % Peers;
		SentAt[Index] = xrtTimer();
		if (!benchSend(Sockets[Index], ExactSubscribe, sizeof(ExactSubscribe))) return false;
	}
	for (size_t Offset = 0; Offset < Peers; ++Offset) {
		size_t Index = (First + Offset) % Peers;
		unsigned char Ack[sizeof(SubscribeAck)], Payload[sizeof(RetainedFrame)];
		if (!benchReceive(Sockets[Index], Ack, sizeof(Ack)) || memcmp(Ack, SubscribeAck, sizeof(Ack)) ||
			!benchReceive(Sockets[Index], Payload, sizeof(Payload)) ||
			memcmp(Payload, RetainedFrame, sizeof(Payload))) return false;
		Samples[Offset] = xrtTimer() - SentAt[Index];
	}
	return true;
}

static bool benchPersistentRound(serverexample* Example, xnetsocket* Sockets, size_t Peers, size_t Round, double* Samples)
{
	const unsigned char Disconnect[] = {0xe0, 0};
	for (size_t Index = 0; Index < Peers; ++Index)
		if (!benchSend(Sockets[Index], Disconnect, sizeof(Disconnect))) return false;
	for (size_t Index = 0; Index < Peers; ++Index) {
		unsigned char Extra;
		size_t Read;
		if (xrtNetSocketRecv(Sockets[Index], &Extra, sizeof(Extra), &Read) != XNET_RESULT_CLOSED) return false;
		xrtNetSocketClose(Sockets[Index]);
		Sockets[Index] = NULL;
	}
	double Deadline = xrtTimer() + 5.0;
	for (;;) {
		xmqttserverinfo Info;
		if (!xmqttServerInfo(Example->Server, &Info) || xrtTimer() >= Deadline || Info.OutgoingPublishOverloads) return false;
		if (Info.OfflineSessions == Peers && Info.Connections == 1) break;
		xrtSleep(1);
	}
	uint16 Id = (uint16)(Round + 1);
	unsigned char Wire[140] = {0x32,0x89,1,0,4,'l','o','a','d',(uint8)(Id >> 8),(uint8)Id,0};
	Wire[sizeof(Wire) - 2] = (uint8)(Round >> 8);
	Wire[sizeof(Wire) - 1] = (uint8)Round;
	double SentAt = xrtTimer();
	if (!benchSend(Sockets[Peers], Wire, sizeof(Wire)) || !benchReceiveFlowReply(Sockets[Peers], 0x40, Id)) return false;
	for (size_t Offset = 0; Offset < Peers; ++Offset) {
		size_t Index = (Round + Offset) % Peers;
		Sockets[Index] = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		unsigned char Reply[sizeof(Wire)];
		if (!Sockets[Index] || !benchConnect(Sockets[Index], &Example->Address, Index, true, true, true) ||
			!benchReceive(Sockets[Index], Reply, sizeof(Reply)) || memcmp(Wire, Reply, sizeof(Wire)) ||
			!benchSendFlowReply(Sockets[Index], 0x40, Id)) return false;
		Samples[Offset] = xrtTimer() - SentAt;
	}
	return true;
}

static bool benchLiveTakeoverRound(serverexample* Example, xnetsocket* Sockets, size_t Peers,
	size_t Round, double* Samples)
{
	uint16 Id = (uint16)(Round + 1);
	unsigned char Wire[140] = {0x32,0x89,1,0,4,'l','o','a','d',(uint8)(Id >> 8),(uint8)Id,0};
	Wire[sizeof(Wire) - 2] = (uint8)(Round >> 8);
	Wire[sizeof(Wire) - 1] = (uint8)Round;
	unsigned char Reply[sizeof(Wire)];
	if (!benchSend(Sockets[Peers], Wire, sizeof(Wire)) || !benchReceiveFlowReply(Sockets[Peers], 0x40, Id)) return false;
	for (size_t Index = 0; Index < Peers; ++Index)
		if (!benchReceive(Sockets[Index], Reply, sizeof(Reply)) || memcmp(Reply, Wire, sizeof(Wire))) return false;
	Wire[0] |= XMQTT_PUBLISH_DUP;
	const unsigned char TakenOver[] = {0xe0,1,0x8e};
	for (size_t Offset = 0; Offset < Peers; ++Offset) {
		size_t Index = (Round + Offset) % Peers;
		double Start = xrtTimer();
		xnetsocket Old = Sockets[Index];
		xnetsocket New = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		size_t Read;
		bool Ready = New && benchConnect(New, &Example->Address, Index, true, true, true) &&
			benchReceive(Old, Reply, sizeof(TakenOver)) && !memcmp(Reply, TakenOver, sizeof(TakenOver)) &&
			xrtNetSocketRecv(Old, Reply, 1, &Read) == XNET_RESULT_CLOSED &&
			benchReceive(New, Reply, sizeof(Reply)) && !memcmp(Reply, Wire, sizeof(Wire)) &&
			benchSendFlowReply(New, 0x40, Id);
		if (!Ready) {
			if (New) xrtNetSocketClose(New);
			return false;
		}
		Samples[Offset] = xrtTimer() - Start;
		xrtNetSocketClose(Old);
		Sockets[Index] = New;
	}
	return true;
}

static bool benchWillRound(const serverexample* Example, const xnetsocket* Sockets, size_t Peers,
	size_t Round, double* Samples)
{
	unsigned char Expected[138] = {0x30,0x87,1,0,4,'l','o','a','d',0};
	Expected[sizeof(Expected) - 2] = (uint8)(Round >> 8);
	Expected[sizeof(Expected) - 1] = (uint8)Round;
	unsigned char Connect[157] = {0x10,0x9a,1,0,4,'M','Q','T','T',5,6,0,0,0,
		0,4,'w','i','l','l',0,0,4,'l','o','a','d',0,128};
	memcpy(Connect + sizeof(Connect) - 128, Expected + sizeof(Expected) - 128, 128);
	xnetsocket Publisher = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	if (!Publisher) return false;
	bool Ready = benchConnectFrame(Publisher, &Example->Address, Connect, sizeof(Connect), false, 0);
	const unsigned char Disconnect[] = {0xe0,1,0x04};
	unsigned char Reply[sizeof(Expected)];
	size_t Read;
	double SentAt = xrtTimer();
	Ready = Ready && benchSend(Publisher, Disconnect, sizeof(Disconnect)) &&
		xrtNetSocketRecv(Publisher, Reply, 1, &Read) == XNET_RESULT_CLOSED;
	xrtNetSocketClose(Publisher);
	if (!Ready) return false;
	for (size_t Offset = 0; Offset < Peers; ++Offset) {
		size_t Index = (Round + Offset) % Peers;
		if (!benchReceive(Sockets[Index], Reply, sizeof(Reply)) || memcmp(Reply, Expected, sizeof(Reply))) return false;
		Samples[Offset] = xrtTimer() - SentAt;
	}
	return true;
}

static int benchOrder(const void* Left, const void* Right)
{
	double A = *(const double*)Left, B = *(const double*)Right;
	return (A > B) - (A < B);
}

static double benchPercentile(const double* Values, size_t Count, double Fraction)
{
	size_t Rank = (size_t)ceil(Fraction * (double)Count);
	return Values[Rank - 1] * 1000000.0;
}

enum { BENCH_SHARED_GROUPS = 2, BENCH_MAX_PUBLISHERS = 8 };

typedef struct benchRemoteShared {
	xatomic32 Finished, Failed;
} benchRemoteShared;

typedef struct benchRemotePeer {
	benchConnection Connection;
	double Start, Rate;
	size_t Warmup, Rounds, Publishers;
	double* Samples;
	double Last;
	uint8 Qos;
	uint16 Window;
	benchRemoteShared* Shared;
	xatomic32* Seen;
	xatomic32 ExpectPing;
	size_t Delivered;
	bool Complete;
} benchRemotePeer;

static bool benchPing(benchConnection Connection)
{
	const unsigned char Wire[] = {0xc0,0};
	unsigned char Reply[2];
	return benchWireSend(Connection, Wire, sizeof(Wire)) && benchWireReceive(Connection, Reply, sizeof(Reply)) &&
		Reply[0] == 0xd0 && !Reply[1];
}

typedef struct benchRemoteReader {
	benchConnection Connection;
	benchRemoteShared* Shared;
	unsigned char Bytes[4096];
	size_t Begin, End;
} benchRemoteReader;

static bool benchRemoteSharedReadable(const benchRemoteReader* Reader)
{
	intptr_t Socket = xrtNetSocketNative(Reader->Connection.Socket);
	if (Socket == -1) return false;
	while (!xrtAtomic32Load(&Reader->Shared->Failed, XMEMORY_ACQUIRE)) {
#if defined(_WIN32) || defined(_WIN64)
		fd_set Set; FD_ZERO(&Set); FD_SET((SOCKET)Socket, &Set);
		struct timeval Wait = {.tv_usec = 200000};
		int Ready = select(0, &Set, NULL, NULL, &Wait);
#else
		struct pollfd Poll = {.fd = (int)Socket, .events = POLLIN};
		int Ready = poll(&Poll, 1, 200);
		if (Ready < 0 && errno == EINTR) continue;
#endif
		if (Ready) return Ready > 0;
	}
	return false;
}

static bool benchRemoteReadFrame(benchRemoteReader* Reader, xmqttframe* Frame)
{
	for (;;) {
		size_t Pending = Reader->End - Reader->Begin;
		xmqttwirestatus Status = xmqttFrameRead((xbytesview){Reader->Bytes + Reader->Begin, Pending}, NULL, Frame);
		if (Status == XMQTT_WIRE_DONE) {
			Reader->Begin += Frame->Wire.Size;
			return true;
		}
		if (Status != XMQTT_WIRE_MORE || Pending == sizeof(Reader->Bytes)) return false;
		if (Pending && Reader->Begin) memmove(Reader->Bytes, Reader->Bytes + Reader->Begin, Pending);
		if (Reader->Shared && Reader->Connection.Socket && !benchRemoteSharedReadable(Reader)) return false;
		size_t Read = benchWireRead(Reader->Connection, Reader->Bytes + Pending,
			sizeof(Reader->Bytes) - Pending, Reader->Shared ? &Reader->Shared->Failed : NULL);
		if (!Read) return false;
		Reader->Begin = 0;
		Reader->End = Pending + Read;
	}
}

static uint16 benchRemotePacketId(const unsigned char* P)
{
	return (uint16)((uint16)P[0] << 8 | P[1]);
}

/* Successful MQTT 5 acknowledgement with no properties. Accept all three
 * wire forms, including explicit reason and empty property sections. */
static uint16 benchRemoteAckId(const xmqttframe* Frame)
{
	xbytesview Body = Frame->Body;
	if (Body.Size < 2 || Body.Size > 4 || (Body.Size > 2 && Body.Data[2]) ||
		(Body.Size == 4 && Body.Data[3])) return 0;
	return benchRemotePacketId(Body.Data);
}

static size_t benchRemoteFrame(unsigned char* Frame, size_t Round, uint8 Qos, uint16 Id)
{
	const unsigned char Header[] = {0x30,0x87,1,0,4,'l','o','a','d'};
	memcpy(Frame, Header, sizeof(Header));
	size_t Offset = sizeof(Header);
	if (Qos) {
		Frame[0] |= (uint8)(Qos << 1);
		Frame[1] += 2;
		Frame[Offset++] = (uint8)(Id >> 8);
		Frame[Offset++] = (uint8)Id;
	}
	Frame[Offset++] = 0;
	memset(Frame + Offset, 0x41, 128);
	for (size_t Byte = 0; Byte < 4; ++Byte)
		Frame[Offset + 127 - Byte] = (unsigned char)(Round >> (Byte * 8));
	return Offset + 128;
}

static bool benchRemoteDelivery(benchRemotePeer* Peer, size_t Round, double Arrived)
{
	if (Round >= Peer->Warmup) {
		double Latency = Arrived - (Peer->Start + (double)Round / Peer->Rate);
		if (Latency < 0) return false;
		Peer->Samples[Round - Peer->Warmup] = Latency;
		++Peer->Delivered;
	}
	Peer->Last = Arrived;
	if (Peer->Shared) (void)xrtAtomic32FetchAdd(&Peer->Shared->Finished, 1, XMEMORY_ACQ_REL);
	return true;
}

static size_t benchRemoteSequence(const unsigned char* Tail)
{
	return ((size_t)Tail[0] << 24) | ((size_t)Tail[1] << 16) | ((size_t)Tail[2] << 8) | Tail[3];
}

static bool benchRemotePublication(benchRemotePeer* Peer, const xmqttframe* Frame, size_t* Round, uint16* Id)
{
	size_t Size = 138u + (Peer->Qos ? 2u : 0u);
	if (Frame->Wire.Size != Size) return false;
	*Id = Peer->Qos ? benchRemotePacketId(Frame->Wire.Data + 9) : 0;
	if (Peer->Qos && !*Id) return false;
	const unsigned char* Tail = Frame->Wire.Data + Size - 4;
	if (Peer->Shared || Peer->Publishers > 1) *Round = benchRemoteSequence(Tail);
	unsigned char Expected[140];
	if (*Round >= Peer->Warmup + Peer->Rounds || benchRemoteFrame(Expected, *Round, Peer->Qos, *Id) != Size ||
		memcmp(Frame->Wire.Data, Expected, Size)) {
		if (!Peer->Shared && Peer->Publishers == 1) {
			size_t Observed = benchRemoteSequence(Tail);
			if (Observed != *Round)
				fprintf(stderr, "Subscriber publication sequence mismatch: expected=%zu received=%zu\n", *Round, Observed);
		}
		return false;
	}
	/* Claim before acknowledging, including when QoS 2 is still awaiting PUBREL. */
	uint32 Unseen = 0;
	return !Peer->Shared || xrtAtomic32CompareExchange(Peer->Seen + *Round, &Unseen, 1,
		XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}

static bool benchRemoteReadFrames(benchRemotePeer* Peer)
{
	typedef struct { uint16 Id; size_t Round; } pending;
	pending Pending[64] = {0};
	benchRemoteReader Reader = {.Connection = Peer->Connection, .Shared = Peer->Shared};
	size_t Received = 0, Finished = 0, Total = Peer->Warmup + Peer->Rounds;
	size_t Next[BENCH_MAX_PUBLISHERS];
	for (size_t Lane = 0; Lane < Peer->Publishers; ++Lane) Next[Lane] = Lane;
	while (Peer->Shared || Finished < Total) {
		xmqttframe Frame;
		if (!benchRemoteReadFrame(&Reader, &Frame)) return false;
		if (Peer->Shared && Frame.Header.Type == XMQTT_PACKET_PINGRESP) {
			return !Frame.Body.Size && xrtAtomic32Load(&Peer->ExpectPing, XMEMORY_ACQUIRE) && Reader.Begin == Reader.End;
		}
		if (Frame.Header.Type == XMQTT_PACKET_PUBLISH) {
			uint16 Id;
			size_t Round = Received;
			if (!benchRemotePublication(Peer, &Frame, &Round, &Id)) return false;
			if (!Peer->Shared && Peer->Publishers > 1) {
				size_t Lane = Round % Peer->Publishers;
				if (Round != Next[Lane]) return false;
				Next[Lane] += Peer->Publishers;
			}
			if (Peer->Qos == 2) {
				pending* Free = NULL;
				for (size_t I = 0; I < Peer->Window; ++I) {
					if (Pending[I].Id == Id) return false;
					if (!Pending[I].Id && !Free) Free = Pending + I;
				}
				if (!Free || !benchWireFlowReply(Peer->Connection, 0x50, Id)) return false;
				*Free = (pending){Id, Round};
			} else {
				if ((Peer->Qos && !benchWireFlowReply(Peer->Connection, 0x40, Id)) ||
					!benchRemoteDelivery(Peer, Round, xrtTimer())) return false;
				++Finished;
			}
			++Received;
		} else if (Peer->Qos == 2 && Frame.Header.Type == XMQTT_PACKET_PUBREL) {
			uint16 Id = benchRemoteAckId(&Frame);
			size_t I = 0;
			if (!Id) return false;
			while (I < Peer->Window && Pending[I].Id != Id) ++I;
			if (I == Peer->Window || !benchWireFlowReply(Peer->Connection, 0x70, Id) ||
				!benchRemoteDelivery(Peer, Pending[I].Round, xrtTimer())) return false;
			Pending[I].Id = 0;
			++Finished;
		} else return false;
	}
	return Reader.Begin == Reader.End;
}

static int32 benchRemoteRead(ptr Context)
{
	benchRemotePeer* Peer = Context;
	if (Peer->Qos || Peer->Shared) {
		Peer->Complete = benchRemoteReadFrames(Peer);
		if (Peer->Shared && !Peer->Complete) xrtAtomic32Store(&Peer->Shared->Failed, 1, XMEMORY_RELEASE);
		return Peer->Complete ? 0 : 1;
	}
	for (size_t Round = 0; Round < Peer->Warmup + Peer->Rounds; ++Round) {
		unsigned char Wire[138], Expected[138];
		if (!benchWireReceive(Peer->Connection, Wire, sizeof(Wire))) return 1;
		double Arrived = xrtTimer();
		benchRemoteFrame(Expected, Round, 0, 0);
		if (memcmp(Wire, Expected, sizeof(Wire)) || !benchRemoteDelivery(Peer, Round, Arrived)) return 1;
	}
	Peer->Complete = true;
	return 0;
}

typedef enum { BENCH_PUBLISH_FREE, BENCH_PUBLISH_WAIT_RECEIPT, BENCH_PUBLISH_WAIT_COMPLETE } benchPublishPhase;
typedef enum { BENCH_OFFER_ERROR, BENCH_OFFER_BLOCKED, BENCH_OFFER_SENT } benchOfferResult;

typedef struct benchRemoteCredit {
	xmutex Gate;
	xcond Changed;
	xatomic32 Waiting;
	bool Pending;
} benchRemoteCredit;

typedef struct benchRemotePublisher {
	benchConnection Connection;
	xmutex Gate;
	xcond Changed;
	benchRemoteCredit* Credit;
	struct { benchPublishPhase Phase; size_t Round; double Sent; } Slots[64];
	uint8 Qos;
	size_t Window, Warmup, Rounds, CreditStalls, Confirmed, Sent;
	size_t Index, Stride, Next;
	double Start, Rate, Last, ReadyAt;
	double *Samples, *FromSend;
	bool Complete, Failed, Stalled;
} benchRemotePublisher;

/* Publisher and ACK reader share only one short send and slot transition
 * at a time. Packet identifiers are recycled only once fully confirmed. */
static benchOfferResult benchRemoteOffer(benchRemotePublisher* P, size_t Round, double Scheduled, double Ready,
	double* Lag, double* CreditWait)
{
	if (!P->Qos) {
		unsigned char Frame[138];
		benchRemoteFrame(Frame, Round, 0, 0);
		if (Lag) *Lag = xrtTimer() - Scheduled;
		return benchWireSend(P->Connection, Frame, sizeof(Frame)) ? BENCH_OFFER_SENT : BENCH_OFFER_ERROR;
	}
	size_t LocalRound = P->Credit ? Round / P->Stride : Round;
	size_t Slot = LocalRound % P->Window;
	if (!xrtMutexLock(&P->Gate)) return BENCH_OFFER_ERROR;
	if (P->Credit && !P->Failed && P->Slots[Slot].Phase != BENCH_PUBLISH_FREE)
		return xrtMutexUnlock(&P->Gate) ? BENCH_OFFER_BLOCKED : BENCH_OFFER_ERROR;
	if (CreditWait && P->Slots[Slot].Phase != BENCH_PUBLISH_FREE) ++P->CreditStalls;
	while (!P->Failed && P->Slots[Slot].Phase != BENCH_PUBLISH_FREE) {
		if (xrtCondWaitFor(&P->Changed, &P->Gate, 5000) != XWAIT_OK) {
			(void)xrtMutexUnlock(&P->Gate);
			return BENCH_OFFER_ERROR;
		}
	}
	bool Sent = false;
	if (CreditWait) *CreditWait = xrtTimer() - Ready;
	if (!P->Failed) {
		unsigned char Frame[140];
		size_t Size = benchRemoteFrame(Frame, Round, P->Qos, (uint16)(Slot + 1));
		P->Slots[Slot].Round = Round;
		P->Slots[Slot].Phase = BENCH_PUBLISH_WAIT_RECEIPT;
		P->Slots[Slot].Sent = xrtTimer();
		if (Lag) *Lag = P->Slots[Slot].Sent - Scheduled;
		Sent = benchWireSend(P->Connection, Frame, Size);
	}
	return xrtMutexUnlock(&P->Gate) && Sent ? BENCH_OFFER_SENT : BENCH_OFFER_ERROR;
}

static bool benchRemoteCreditChanged(benchRemotePublisher* P)
{
	benchRemoteCredit* Credit = P->Credit;
	if (!Credit) return xrtCondSignal(&P->Changed);
	if (!xrtAtomic32Load(&Credit->Waiting, XMEMORY_ACQUIRE)) return true;
	if (!xrtMutexLock(&Credit->Gate)) return false;
	Credit->Pending = true;
	bool OK = xrtCondSignal(&Credit->Changed);
	return xrtMutexUnlock(&Credit->Gate) && OK;
}

static xwaitresult benchRemoteWaitCredit(benchRemoteCredit* Credit, int64 Milliseconds)
{
	if (!xrtMutexLock(&Credit->Gate)) return XWAIT_ERROR;
	xwaitresult Result = Credit->Pending ? XWAIT_OK :
		xrtCondWaitFor(&Credit->Changed, &Credit->Gate, Milliseconds);
	if (Credit->Pending) Result = XWAIT_OK;
	Credit->Pending = false;
	return xrtMutexUnlock(&Credit->Gate) ? Result : XWAIT_ERROR;
}

static int32 benchRemoteConfirm(ptr Context)
{
	benchRemotePublisher* P = Context;
	benchRemoteReader Reader = {.Connection = P->Connection};
	size_t Finished = 0, Total = (P->Warmup + P->Rounds + P->Stride - 1u - P->Index) / P->Stride;
	size_t Measured = 0;
	while (Finished < Total) {
		xmqttframe Frame;
		if (!benchRemoteReadFrame(&Reader, &Frame)) break;
		double Confirmed = xrtTimer();
		xmqttpackettype Type = Frame.Header.Type;
		uint16 Id = benchRemoteAckId(&Frame);
		if (Type == XMQTT_PACKET_DISCONNECT) {
			fprintf(stderr, "publisher DISCONNECT reason=0x%02X\n",
				Frame.Body.Size ? (unsigned)Frame.Body.Data[0] : 0u);
			break;
		}
		if (!Id && (Type == XMQTT_PACKET_PUBACK || Type == XMQTT_PACKET_PUBREC ||
			Type == XMQTT_PACKET_PUBCOMP) && Frame.Body.Size > 2) {
			fprintf(stderr, "publisher acknowledgement type=%u reason=0x%02X\n",
				(unsigned)Type, (unsigned)Frame.Body.Data[2]);
		}
		if (!Id || Id > P->Window || !xrtMutexLock(&P->Gate)) break;
		size_t Slot = Id - 1u;
		benchPublishPhase Phase = P->Slots[Slot].Phase;
		bool Terminal = (P->Qos == 1 && Phase == BENCH_PUBLISH_WAIT_RECEIPT && Type == XMQTT_PACKET_PUBACK) ||
			(P->Qos == 2 && Phase == BENCH_PUBLISH_WAIT_COMPLETE && Type == XMQTT_PACKET_PUBCOMP);
		bool OK = true;
		if (Terminal) {
			size_t Round = P->Slots[Slot].Round;
			if (Round >= P->Warmup) {
				size_t Sample = Round - P->Warmup;
				P->Samples[Sample] = Confirmed - (P->Start + (double)Round / P->Rate);
				P->FromSend[Sample] = Confirmed - P->Slots[Slot].Sent;
				++Measured;
			}
			P->Last = Confirmed;
			P->Slots[Slot].Phase = BENCH_PUBLISH_FREE;
			OK = benchRemoteCreditChanged(P);
		} else if (P->Qos == 2 && Phase == BENCH_PUBLISH_WAIT_RECEIPT && Type == XMQTT_PACKET_PUBREC) {
			P->Slots[Slot].Phase = BENCH_PUBLISH_WAIT_COMPLETE;
			OK = benchWireFlowReply(P->Connection, 0x62, Id);
		} else OK = false;
		if (!xrtMutexUnlock(&P->Gate) || !OK) break;
		if (Terminal) ++Finished;
	}
	P->Confirmed = Measured;
	P->Complete = Finished == Total && Reader.Begin == Reader.End;
	if (!P->Complete && xrtMutexLock(&P->Gate)) {
		P->Failed = true;
		(void)benchRemoteCreditChanged(P);
		(void)xrtMutexUnlock(&P->Gate);
	}
	return P->Complete ? 0 : 1;
}


/* One pacer offers each lane its own fixed subset of the arrival schedule.
 * A flow without credit does not delay offers on eligible flows. */
static bool benchRemoteOfferMany(benchRemotePublisher* Publishers, size_t Count, size_t Warmup,
	size_t Rounds, double Start, double Rate, double* Lag, double* PacerLag, double* CreditWait)
{
	benchRemoteCredit* Credit = Publishers[0].Credit;
	size_t Total = Warmup + Rounds, Offered = 0, Cursor = 0;
	bool Waiting = false;
	while (Offered < Total) {
		double Now = xrtTimer(), NextDue = INFINITY;
		bool Sent = false, Blocked = false;
		for (size_t Offset = 0; Offset < Count; ++Offset) {
			size_t Lane = (Cursor + Offset) % Count;
			benchRemotePublisher* P = Publishers + Lane;
			size_t Round = P->Next;
			if (Round >= Total) continue;
			double Scheduled = Start + (double)Round / Rate;
			if (Scheduled > Now) {
				if (Scheduled < NextDue) NextDue = Scheduled;
				continue;
			}
			bool Measured = Round >= Warmup;
			size_t Sample = Measured ? Round - Warmup : 0;
			if (!P->ReadyAt) P->ReadyAt = Now;
			benchOfferResult Result = benchRemoteOffer(P, Round, Scheduled, P->ReadyAt,
				Measured ? Lag + Sample : NULL, Measured ? CreditWait + Sample : NULL);
			if (Result == BENCH_OFFER_ERROR) return false;
			if (Result == BENCH_OFFER_BLOCKED) {
				Blocked = true;
				if (!P->Stalled && Measured) ++P->CreditStalls;
				P->Stalled = true;
				continue;
			}
			if (Measured) {
				PacerLag[Sample] = P->ReadyAt - Scheduled;
				++P->Sent;
			}
			P->ReadyAt = 0;
			P->Stalled = false;
			P->Next += Count;
			Cursor = (Lane + 1) % Count;
			++Offered;
			Sent = true;
			break;
		}
		if (Sent || !Blocked) {
			if (Waiting) {
				xrtAtomic32Store(&Credit->Waiting, 0, XMEMORY_RELEASE);
				Waiting = false;
			}
			if (!Sent && NextDue - Now > 0.003) xrtSleep(1);
		} else if (!Waiting) {
			/* Register before rechecking the credits under the publisher gates. */
			xrtAtomic32Store(&Credit->Waiting, 1, XMEMORY_RELEASE);
			Waiting = true;
		} else {
			double Delay = fmin(5000, (NextDue - xrtTimer()) * 1000);
			if (Delay >= 1) {
				xwaitresult Result = benchRemoteWaitCredit(Credit, (int64)Delay);
				if (Result != XWAIT_OK && (Result != XWAIT_TIMEOUT || !isfinite(NextDue))) return false;
			}
		}
	}
	return true;
}

/* Bound the blocking I/O on both brokers, including the join of a
 * reader whose peer stopped responding. The XRT object owns the handle. */
static bool benchRemoteTimeouts(xnetsocket Socket)
{
	intptr_t Native = xrtNetSocketNative(Socket);
	if (Native == -1) return false;
#if defined(_WIN32) || defined(_WIN64)
	const DWORD Timeout = 5000;
	return setsockopt((SOCKET)Native, SOL_SOCKET, SO_RCVTIMEO, (const char*)&Timeout, sizeof(Timeout)) == 0 &&
		setsockopt((SOCKET)Native, SOL_SOCKET, SO_SNDTIMEO, (const char*)&Timeout, sizeof(Timeout)) == 0;
#else
	const struct timeval Timeout = {.tv_sec = 5};
	return setsockopt((int)Native, SOL_SOCKET, SO_RCVTIMEO, &Timeout, sizeof(Timeout)) == 0 &&
		setsockopt((int)Native, SOL_SOCKET, SO_SNDTIMEO, &Timeout, sizeof(Timeout)) == 0;
#endif
}

typedef struct benchRemoteTransport {
#if defined(XMQTT_BENCH_TLS)
	xnetengine* Engine;
	xtlscontext* Context;
	xtlsverifier* Verifier;
	xtlsclientconfig Client;
#else
	unsigned char Unused;
#endif
} benchRemoteTransport;

#if defined(XMQTT_BENCH_TLS)
static bool benchRemoteTlsStart(benchRemoteTransport* Transport, const char* Ca, const char* Name)
{
	unsigned char Pem[65536];
	FILE* File = fopen(Ca, "rb");
	if (!File) return false;
	size_t Size = fread(Pem, 1, sizeof(Pem), File);
	bool Complete = Size && Size < sizeof(Pem) && feof(File) && !ferror(File);
	if (fclose(File) != 0 || !Complete) return false;
	xx509store* Store = xrtX509StoreCreate();
	if (!Store) return false;
	if (xrtX509StoreAddData(Store, (xbytesview){Pem, Size}, NULL)) {
		xtlsverifierconfig Verify;
		xrtTlsVerifierConfigInit(&Verify);
		Verify.Store = Store;
		xtlsverifylimits Limits;
		xrtTlsVerifyLimitsInit(&Limits);
		Transport->Verifier = xrtTlsVerifierCreateLimited(&Verify, &Limits);
	}
	xrtX509StoreFree(Store);
	if (!Transport->Verifier) return false;
	const xtlsversion Versions[] = {XTLS_VERSION_13};
	const xtlscipher Ciphers[] = {XTLS_AES_128_GCM_SHA256};
	const uint16 Groups[] = {XTLS_GROUP_X25519};
	const xtlssignature Signatures[] = {XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256};
	xtlspolicy Policy;
	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions; Policy.VersionCount = 1;
	Policy.Ciphers = Ciphers; Policy.CipherCount = 1;
	Policy.Groups = Groups; Policy.GroupCount = 1;
	Policy.Signatures = Signatures; Policy.SignatureCount = 1;
	xtlscontextconfig Context;
	xrtTlsContextConfigInit(&Context);
	Context.Policy = &Policy;
	Transport->Context = xrtTlsContextCreate(&Context);
	if (!Transport->Context) return false;
	xrtTlsClientConfigInit(&Transport->Client);
	Transport->Client.Context = Transport->Context;
	Transport->Client.Verifier = Transport->Verifier;
	Transport->Client.ServerName = Transport->Client.VerifyName = (xstrview){Name, strlen(Name)};
	xnetengineconfig Engine;
	xrtNetEngineConfigInit(&Engine);
	Engine.Workers = 1;
	Transport->Engine = xrtNetEngineCreate(&Engine);
	return Transport->Engine && xrtNetEngineStart(Transport->Engine);
}

static bool benchRemoteTlsStop(benchRemoteTransport* Transport)
{
	bool OK = !Transport->Engine || xrtNetEngineDestroy(Transport->Engine);
	if (Transport->Context) xrtTlsContextRelease(Transport->Context);
	if (Transport->Verifier) xrtTlsVerifierRelease(Transport->Verifier);
	return OK;
}
#endif

static bool benchRemoteConnect(benchConnection* Connection, const benchRemoteTransport* Transport,
	const xnetaddr* Address, size_t Index, uint16 Window, uint16 RequiredWindow)
{
	unsigned char Frame[31];
	size_t Size = benchConnectWire(Frame, Index, Window, false, false);
#if defined(XMQTT_BENCH_TLS)
	if (Transport->Engine) {
		xnetstreamconfig Tcp;
		xrtNetStreamConfigInit(&Tcp);
		Tcp.NoDelay = true;
		Connection->Tls = xrtTlsStreamConnect(Transport->Engine, Address, 0, &Tcp,
			&Transport->Client, NULL, NULL, NULL);
		xfuture* Open = Connection->Tls ? xrtTlsStreamWaitAsync(Connection->Tls, XTLS_STREAM_WAIT_OPEN) : NULL;
		bool Ready = benchTlsAwait(Open, NULL);
		if (!Ready) {
			const xerror* Error = Open ? xrtFutureError(Open) : xrtGetError();
			fprintf(stderr, "TLS connection %zu: %s\n", Index, Error ? xrtErrorMessage(Error) : "open timed out");
		}
		if (Open) xrtFutureDestroy(Open);
		return Ready && benchMqttConnect(*Connection, Frame, Size, false, RequiredWindow);
	}
#else
	(void)Transport;
#endif
	Connection->Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	return Connection->Socket && benchRemoteTimeouts(Connection->Socket) &&
		benchConnectFrame(Connection->Socket, Address, Frame, Size, false, RequiredWindow);
}

static void benchRemoteAbort(benchConnection Connection)
{
#if defined(XMQTT_BENCH_TLS)
	if (Connection.Tls) (void)xrtTlsStreamAbort(Connection.Tls);
#endif
	if (Connection.Socket) (void)xrtNetSocketShutdown(Connection.Socket, XNET_SHUTDOWN_BOTH);
}

static bool benchRemoteClose(benchConnection Connection)
{
#if defined(XMQTT_BENCH_TLS)
	if (Connection.Tls) {
		(void)xrtTlsStreamAbort(Connection.Tls);
		bool Retired = xrtTlsStreamWaitRetired(Connection.Tls, 5000) == XWAIT_OK;
		xrtTlsStreamDestroy(Connection.Tls);
		return Retired;
	}
#endif
	if (Connection.Socket) xrtNetSocketClose(Connection.Socket);
	return true;
}

static bool benchRemoteNumber(const char* Text, unsigned long Min, unsigned long Max, unsigned long* Out)
{
	char* End;
	if (*Text < '0' || *Text > '9') return false;
	unsigned long Value = strtoul(Text, &End, 10);
	if (*End || Value < Min || Value > Max) return false;
	*Out = Value;
	return true;
}

static bool benchRemoteSharedDrain(benchRemoteShared* State, benchRemotePeer* Peers, size_t Count, uint32 Deliveries)
{
	double Deadline = xrtTimer() + 5.0;
	while (xrtAtomic32Load(&State->Finished, XMEMORY_ACQUIRE) < Deliveries) {
		if (xrtAtomic32Load(&State->Failed, XMEMORY_ACQUIRE) || xrtTimer() >= Deadline) return false;
		xrtSleep(1);
	}
	/* All legitimate ACK writes have returned. Each reader still owns its
	 * receive side and checks the PINGRESP plus any buffered excess. */
	const unsigned char Ping[] = {0xc0, 0};
	for (size_t I = 0; I < Count; ++I) {
		xrtAtomic32Store(&Peers[I].ExpectPing, 1, XMEMORY_RELEASE);
		if (!benchWireSend(Peers[I].Connection, Ping, sizeof(Ping))) return false;
	}
	return true;
}

/* Independent wire observer. The external broker and the load driver run
 * in separate processes. Every arrival must reach each ordinary subscriber
 * or exactly one member of each shared group. */
static int benchRemote(int Argc, char** Argv)
{
#if defined(XMQTT_BENCH_TLS)
	const char* Ca = NULL;
	const char* Name = NULL;
	if (Argc >= 4 && !strcmp(Argv[Argc - 3], "--tls")) {
		Ca = Argv[Argc - 2];
		Name = Argv[Argc - 1];
		Argc -= 3;
		if (!*Ca || !*Name) return 2;
	}
#endif
	unsigned long PublisherCount = 1;
	if (Argc >= 4 && !strcmp(Argv[Argc - 2], "--publishers")) {
		if (!benchRemoteNumber(Argv[Argc - 1], 1, BENCH_MAX_PUBLISHERS, &PublisherCount)) return 2;
		Argc -= 2;
	}
	unsigned long Port, Peers, Rounds, Rate, Warmup, Qos = 0, Window = 1, SubscriberWindow = 0;
	if ((Argc != 8 && Argc != 10 && Argc != 11) || !benchRemoteNumber(Argv[2], 1, 65535, &Port) ||
		!benchRemoteNumber(Argv[3], 1, 64, &Peers) ||
		!benchRemoteNumber(Argv[4], 1, 100000, &Rounds) ||
		!benchRemoteNumber(Argv[5], 1, 100000, &Rate) ||
		!benchRemoteNumber(Argv[6], 0, 100000, &Warmup) ||
		(strcmp(Argv[7], "exact") && strcmp(Argv[7], "wildcard") &&
			(strcmp(Argv[7], "shared") || Peers < BENCH_SHARED_GROUPS)) ||
		(Argc >= 10 && (!benchRemoteNumber(Argv[8], 1, 2, &Qos) ||
			!benchRemoteNumber(Argv[9], 1, 64, &Window))) ||
		(Argc == 11 && !benchRemoteNumber(Argv[10], 1, 64, &SubscriberWindow)) || (PublisherCount > 1 && !Qos)) {
		fprintf(stderr, "Usage: bench_server --remote PORT SUBSCRIBERS ROUNDS RATE WARMUP exact|wildcard|shared "
			"[QOS PUBLISHER_WINDOW [SUBSCRIBER_WINDOW]] [--publishers 1..8]"
#if defined(XMQTT_BENCH_TLS)
			" [--tls CA_PEM DNS_NAME]"
#endif
			"\n");
		return 2;
	}
	if (Qos && !SubscriberWindow) SubscriberWindow = Window;
	benchRemotePeer State[64] = {0};
	benchRemoteTransport Transport = {0};
	xthread* Readers[64] = {0};
	bool Shared = !strcmp(Argv[7], "shared");
	size_t Destinations = Shared ? BENCH_SHARED_GROUPS : Peers, Total = Warmup + Rounds;
	benchRemoteShared Group = {0};
	xrtAtomic32Init(&Group.Finished, 0); xrtAtomic32Init(&Group.Failed, 0);
	xatomic32* Seen = Shared ? malloc(Destinations * Total * sizeof(*Seen)) : NULL;
	double* Samples = malloc(Destinations * Rounds * sizeof(*Samples));
	double* Lag = malloc((Qos ? 3 : 2) * Rounds * sizeof(*Lag));
	benchRemotePublisher Publishers[BENCH_MAX_PUBLISHERS] = {0};
	xthread* Confirmations[BENCH_MAX_PUBLISHERS] = {0};
	bool Gates[BENCH_MAX_PUBLISHERS] = {0}, Changed = false, CreditGate = false, CreditReady = false;
	benchRemoteCredit Credit = {0};
	double* ConfirmationSamples = NULL;
	bool OK = false;
	const char* Stage = "allocate measurement buffers";
	if (!Samples || !Lag || (Shared && !Seen)) goto Done;
	if (Shared) for (size_t I = 0; I < Destinations * Total; ++I) xrtAtomic32Init(Seen + I, 0);
	double* PacerLag = Lag + Rounds;
	double* CreditWait = Qos ? PacerLag + Rounds : NULL;
	if (Qos) {
		ConfirmationSamples = malloc(2 * Rounds * sizeof(*ConfirmationSamples));
		if (!ConfirmationSamples) goto Done;
		if (PublisherCount > 1) {
			xrtAtomic32Init(&Credit.Waiting, 0);
			if (!(CreditGate = xrtMutexInit(&Credit.Gate)) ||
				!(CreditReady = xrtCondInit(&Credit.Changed))) goto Done;
		} else if (!(Changed = xrtCondInit(&Publishers[0].Changed))) goto Done;
	}
	for (size_t I = 0; I < PublisherCount; ++I) {
		benchRemotePublisher* P = Publishers + I;
		P->Qos = (uint8)Qos;
		P->Window = Window;
		P->Warmup = Warmup;
		P->Rounds = Rounds;
		P->Rate = (double)Rate;
		P->Index = P->Next = I;
		P->Stride = PublisherCount;
		if (Qos) {
			if (!(Gates[I] = xrtMutexInit(&P->Gate))) goto Done;
			P->Credit = CreditReady ? &Credit : NULL;
			P->Samples = ConfirmationSamples;
			P->FromSend = ConfirmationSamples + Rounds;
		}
	}
	xnetaddr Address;
	xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port);
#if defined(XMQTT_BENCH_TLS)
	Stage = "initialize authenticated TLS";
	if (Ca && !benchRemoteTlsStart(&Transport, Ca, Name)) goto Done;
#endif
	Stage = "connect and subscribe";
	bool Wild = !strcmp(Argv[7], "wildcard");
	const unsigned char Wildcard[] = {0x82,7,0,1,0,0,1,'+',(uint8)Qos};
	const unsigned char ExpectedAck[] = {0x90,4,0,1,0,(uint8)Qos};
	for (size_t I = 0; I < Peers; ++I) {
		if (!benchRemoteConnect(&State[I].Connection, &Transport, &Address, I,
			(uint16)SubscriberWindow, 0)) goto Done;
		if (Shared || Wild) {
			const unsigned char Filter[] = {0x82,16,0,1,0,0,10,'$','s','h','a','r','e','/',
				(unsigned char)('a' + I % BENCH_SHARED_GROUPS),'/', '+',(uint8)Qos};
			unsigned char Reply[sizeof(ExpectedAck)];
			if (!benchWireSend(State[I].Connection, Shared ? Filter : Wildcard,
					Shared ? sizeof(Filter) : sizeof(Wildcard)) ||
				!benchWireReceive(State[I].Connection, Reply, sizeof(Reply)) ||
				memcmp(Reply, ExpectedAck, sizeof(Reply))) goto Done;
		} else if (!benchWireSubscribe(State[I].Connection, false, false, (uint8)Qos)) goto Done;
	}
	Stage = "connect publisher";
	for (size_t I = 0; I < PublisherCount; ++I)
		if (!benchRemoteConnect(&Publishers[I].Connection, &Transport, &Address, Peers + I, 0, Qos ? (uint16)Window : 0))
			goto Done;

	Stage = "start subscribers";
	double Start = xrtTimer() + 0.05;
	if (Qos) for (size_t I = 0; I < PublisherCount; ++I) {
		Publishers[I].Start = Start;
		Confirmations[I] = xrtThreadCreate(benchRemoteConfirm, Publishers + I, 0);
		if (!Confirmations[I]) goto Done;
	}
	for (size_t I = 0; I < Peers; ++I) {
		State[I].Start = Start;
		State[I].Rate = (double)Rate;
		State[I].Warmup = Warmup;
		State[I].Rounds = Rounds;
		State[I].Publishers = PublisherCount;
		State[I].Qos = (uint8)Qos;
		State[I].Window = (uint16)SubscriberWindow;
		size_t Destination = Shared ? I % Destinations : I;
		State[I].Samples = Samples + Destination * Rounds;
		if (Shared) {
			State[I].Shared = &Group;
			State[I].Seen = Seen + Destination * Total;
			xrtAtomic32Init(&State[I].ExpectPing, 0);
		}
		Readers[I] = xrtThreadCreate(benchRemoteRead, State + I, 0);
		if (!Readers[I]) goto Done;
	}
	Stage = "sample observer CPU clocks";
	double CpuStart = benchObserverCpu(false), PacerStart = benchObserverCpu(true);
	if (CpuStart < 0 || PacerStart < 0) goto Done;
	Stage = "offer scheduled publications";
	if (PublisherCount > 1) {
		if (!benchRemoteOfferMany(Publishers, PublisherCount, Warmup, Rounds, Start, (double)Rate,
			Lag, PacerLag, CreditWait)) goto Done;
	} else for (size_t Round = 0; Round < Warmup + Rounds; ++Round) {
		double Scheduled = Start + (double)Round / (double)Rate;
		double Now;
		while ((Now = xrtTimer()) < Scheduled) {
			if (Scheduled - Now > 0.003) xrtSleep(1);
		}
		bool Measured = Round >= Warmup;
		size_t Sample = Measured ? Round - Warmup : 0;
		if (Measured) PacerLag[Sample] = Now - Scheduled;
		if (benchRemoteOffer(Publishers, Round, Scheduled, Now, Measured ? Lag + Sample : NULL,
			Measured && CreditWait ? CreditWait + Sample : NULL) != BENCH_OFFER_SENT) goto Done;
		if (Measured) ++Publishers[0].Sent;
	}
	double PacerCpu = benchObserverCpu(true) - PacerStart;
	Stage = "check subscriber delivery counts and order";
	if (Shared && !benchRemoteSharedDrain(&Group, State, Peers, (uint32)(Destinations * Total))) goto Done;
	double Last = 0;
	for (size_t I = 0; I < Peers; ++I) {
		if (xrtThreadWaitFor(Readers[I], 5000) != XWAIT_OK || !State[I].Complete) goto Done;
		if (State[I].Last > Last) Last = State[I].Last;
	}
	size_t CreditStalls = 0;
	if (Qos) for (size_t I = 0; I < PublisherCount; ++I) {
		Stage = "check publisher confirmation counts";
		if (xrtThreadWaitFor(Confirmations[I], 5000) != XWAIT_OK || !Publishers[I].Complete) goto Done;
		if (Publishers[I].Last > Last) Last = Publishers[I].Last;
		CreditStalls += Publishers[I].CreditStalls;
	}
	Stage = "sample observer CPU clocks";
	double ObserverCpu = benchObserverCpu(false) - CpuStart;
	if (ObserverCpu < 0 || PacerCpu < 0) goto Done;
	Stage = "check for excess packets";
	if (Qos) for (size_t I = 0; I < PublisherCount; ++I)
		if (!benchPing(Publishers[I].Connection)) goto Done;
	if (!Shared) for (size_t I = 0; I < Peers; ++I) if (!benchPing(State[I].Connection)) goto Done;
	double Seconds = Last - (Start + (double)Warmup / (double)Rate);
	if (Seconds <= 0) goto Done;
	size_t Count = Destinations * Rounds;
	qsort(Samples, Count, sizeof(*Samples), benchOrder);
	qsort(Lag, Rounds, sizeof(*Lag), benchOrder);
	qsort(PacerLag, Rounds, sizeof(*PacerLag), benchOrder);
	printf("offered: %lu\ndeliveries: %zu\nmeasurement_seconds: %.9f\n"
		"ingress_per_sec: %.3f\ndeliveries_per_sec: %.3f\n"
		"delivery_p50_us: %.3f\ndelivery_p95_us: %.3f\ndelivery_p99_us: %.3f\n"
		"delivery_p999_us: %.3f\noffer_lag_p99_us: %.3f\npacer_lag_p99_us: %.3f\n",
		Rounds, Count, Seconds, (double)Rounds / Seconds, (double)Count / Seconds,
		benchPercentile(Samples, Count, 0.5), benchPercentile(Samples, Count, 0.95),
		benchPercentile(Samples, Count, 0.99), benchPercentile(Samples, Count, 0.999),
		benchPercentile(Lag, Rounds, 0.99), benchPercentile(PacerLag, Rounds, 0.99));
	printf("observer_cpu_seconds: %.9f\npacer_cpu_seconds: %.9f\n", ObserverCpu, PacerCpu);
#if defined(XMQTT_BENCH_TLS)
	if (Ca) printf("tls_connections: %lu\n", Peers + PublisherCount);
#endif
	if (Shared) {
		size_t Min = Rounds, Max = 0;
		for (size_t I = 0; I < Peers; ++I) {
			if (State[I].Delivered < Min) Min = State[I].Delivered;
			if (State[I].Delivered > Max) Max = State[I].Delivered;
		}
		printf("share_min_deliveries: %zu\nshare_max_deliveries: %zu\n", Min, Max);
	}
	if (Qos) {
		double* FromSend = ConfirmationSamples + Rounds;
		qsort(ConfirmationSamples, Rounds, sizeof(*ConfirmationSamples), benchOrder);
		qsort(CreditWait, Rounds, sizeof(*CreditWait), benchOrder);
		qsort(FromSend, Rounds, sizeof(*FromSend), benchOrder);
		printf("publish_confirmed: %lu\npublisher_p99_us: %.3f\n"
			"publisher_credit_stalls: %zu\npublisher_credit_wait_p99_us: %.3f\npublisher_from_send_p99_us: %.3f\n",
			Rounds, benchPercentile(ConfirmationSamples, Rounds, 0.99), CreditStalls,
			benchPercentile(CreditWait, Rounds, 0.99), benchPercentile(FromSend, Rounds, 0.99));
	}
	OK = true;

Done:
	if (OK) Stage = "retire observer connections";
	if (Shared) xrtAtomic32Store(&Group.Failed, 1, XMEMORY_RELEASE);
	for (size_t I = 0; I < Peers; ++I)
		if (Readers[I]) benchRemoteAbort(State[I].Connection);
	for (size_t I = 0; I < PublisherCount; ++I)
		if (Confirmations[I]) benchRemoteAbort(Publishers[I].Connection);
	for (size_t I = 0; I < Peers; ++I) {
		if (Readers[I]) {
			if (xrtThreadWait(Readers[I]) != XWAIT_OK) abort();
			xrtThreadDestroy(Readers[I]);
		}
		if (!benchRemoteClose(State[I].Connection)) OK = false;
	}
	size_t Sent = 0, Confirmed = 0;
	for (size_t I = 0; I < PublisherCount; ++I) {
		if (Confirmations[I]) {
			if (xrtThreadWait(Confirmations[I]) != XWAIT_OK) abort();
			xrtThreadDestroy(Confirmations[I]);
		}
		if (!benchRemoteClose(Publishers[I].Connection)) OK = false;
		if (Gates[I]) (void)xrtMutexUnit(&Publishers[I].Gate);
		Sent += Publishers[I].Sent;
		Confirmed += Publishers[I].Confirmed;
	}
#if defined(XMQTT_BENCH_TLS)
	if (!benchRemoteTlsStop(&Transport)) OK = false;
#endif
	if (Changed) (void)xrtCondUnit(&Publishers[0].Changed);
	if (CreditReady) (void)xrtCondUnit(&Credit.Changed);
	if (CreditGate) (void)xrtMutexUnit(&Credit.Gate);
	free(ConfirmationSamples);
	free(Lag);
	free(Samples);
	free(Seen);
	if (!OK) {
		size_t Delivered = 0;
		for (size_t I = 0; I < Peers; ++I) Delivered += State[I].Delivered;
		fprintf(stderr, "Remote benchmark failed during %s\n"
			"Partial measurement: sent=%zu confirmed=%zu deliveries=%zu\n",
			Stage, Sent, Confirmed, Delivered);
	}
	return OK ? 0 : 1;
}

int main(int Argc, char** Argv)
{
	if (Argc > 1 && !strcmp(Argv[1], "--remote")) return benchRemote(Argc, Argv);
	size_t Peers = Argc > 1 ? strtoul(Argv[1], NULL, 10) : 64;
	size_t Rounds = Argc > 2 ? strtoul(Argv[2], NULL, 10) : 1000;
	unsigned Workers = Argc > 3 ? strtoul(Argv[3], NULL, 10) : 2;
	if (Peers < 1 || Peers > 4096 || Rounds < 1 || Rounds > 10000 || Workers < 1 || Workers > 32)
		return 1;
	bool Overlap = Argc > 4 && !strcmp(Argv[4], "overlap");
	bool Shared = Argc > 4 && !strcmp(Argv[4], "shared");
	bool Replay = Argc > 4 && !strcmp(Argv[4], "retained");
	bool Takeover = Argc > 4 && !strcmp(Argv[4], "takeover");
	bool Will = Argc > 4 && !strcmp(Argv[4], "will");
	bool Persistent = Takeover || (Argc > 4 && !strcmp(Argv[4], "resume"));
	bool Qos2 = Argc > 4 && !strcmp(Argv[4], "outbound2");
	bool Outgoing = Persistent || Qos2 || (Argc > 4 && !strcmp(Argv[4], "outbound1"));
	uint8 Qos = Qos2 ? 2 : (Outgoing || (Argc > 4 && !strcmp(Argv[4], "inbound1")));
	bool Routing = Qos || Replay || Shared || Overlap || Will || (Argc > 4 && !strcmp(Argv[4], "route"));
	const size_t Connections = Peers + (Routing && !Will), Deliveries = Shared ? 1 : Peers;
	const size_t Count = Deliveries * Rounds;
	xnetsocket* Sockets = calloc(Connections, sizeof(*Sockets));
	double* ConnectSamples = malloc(Connections * sizeof(*ConnectSamples));
	double* SentAt = malloc(Peers * sizeof(*SentAt));
	double* PingSamples = malloc(Count * sizeof(*PingSamples));
	serverexample Example = {0};
	bool OK = false;
	const char* Stage = "allocate samples";
	if (!Sockets || !ConnectSamples || !SentAt || !PingSamples) goto Done;

	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.MaxConnections = Connections + (Will ? 2u : (size_t)Takeover);
	if (Persistent) Config.MaxPersistentSessions = Peers;
	if (Routing) Config.MaxSubscribersPerTopic = Peers;
	Stage = "start listener";
	if (!exampleServerStart(&Example, &Config, Workers)) goto Done;

	Stage = "connect and receive CONNACK";
	double Start = xrtTimer();
	for (size_t Index = 0; Index < Connections; ++Index) {
		double Attempt = xrtTimer();
		Sockets[Index] = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		if (!Sockets[Index] || !benchConnect(Sockets[Index], &Example.Address, Index,
			Outgoing && Index < Peers, Persistent && Index < Peers, false)) goto Done;
		ConnectSamples[Index] = xrtTimer() - Attempt;
	}
	double ConnectSeconds = xrtTimer() - Start;

	if (Replay) {
		Stage = "store retained message";
		const unsigned char Ping[] = {0xc0,0};
		unsigned char Pong[2];
		if (!benchSend(Sockets[Peers], RetainedFrame, sizeof(RetainedFrame)) ||
			!benchSend(Sockets[Peers], Ping, sizeof(Ping)) || !benchReceive(Sockets[Peers], Pong, sizeof(Pong)) ||
			Pong[0] != 0xd0 || Pong[1]) goto Done;
	} else if (Routing) {
		Stage = "subscribe topic";
		for (size_t Index = 0; Index < Peers; ++Index) {
			if (!benchSubscribe(Sockets[Index], Overlap, Shared, Outgoing ? Qos : 0)) goto Done;
		}
	}
	Stage = Replay ? "subscribe and verify replay" : (Routing ? "publish and verify fan-out" : "send PING and receive PINGRESP");
	xmemstats Before, After;
	xrtMemStatsEnable(true);
	xrtMemStatsGet(&Before);
	Start = xrtTimer();
	for (size_t Round = 0; Round < Rounds; ++Round) {
		if (Will) {
			Stage = "close publisher and verify its Will";
			if (!benchWillRound(&Example, Sockets, Peers, Round, PingSamples + Round * Peers)) goto Done;
			continue;
		}
		if (Takeover) {
			Stage = "verify the persistent takeover exchange";
			if (!benchLiveTakeoverRound(&Example, Sockets, Peers, Round, PingSamples + Round * Peers)) goto Done;
			continue;
		}
		if (Persistent) {
			Stage = "reconnect and verify offline delivery";
			if (!benchPersistentRound(&Example, Sockets, Peers, Round, PingSamples + Round * Peers)) goto Done;
			continue;
		}
		if (Replay) {
			if (!benchReplayRound(Sockets, Peers, Round, SentAt, PingSamples + Round * Peers)) goto Done;
			continue;
		}
		if (Routing) {
			if (!benchPublishRound(Sockets, Peers, Round, Deliveries, Shared, Qos, Outgoing, PingSamples + Round * Deliveries)) goto Done;
			continue;
		}
		/* Rotate the first connection to spread sequential peer-side work. */
		size_t First = Round % Peers;
		for (size_t Offset = 0; Offset < Peers; ++Offset) {
			size_t Index = (First + Offset) % Peers;
			const unsigned char Ping[] = {0xc0,0};
			SentAt[Index] = xrtTimer();
			if (!benchSend(Sockets[Index], Ping, sizeof(Ping))) goto Done;
		}
		for (size_t Offset = 0; Offset < Peers; ++Offset) {
			size_t Index = (First + Offset) % Peers;
			unsigned char Pong[2];
			if (!benchReceive(Sockets[Index], Pong, sizeof(Pong)) || Pong[0] != 0xd0 || Pong[1] != 0) goto Done;
			PingSamples[Round * Peers + Offset] = xrtTimer() - SentAt[Index];
		}
	}
	double PingSeconds = xrtTimer() - Start;
	xrtMemStatsGet(&After);
	xrtMemStatsEnable(false);
	if (!(ConnectSeconds > 0) || !(PingSeconds > 0)) goto Done;
	qsort(ConnectSamples, Connections, sizeof(*ConnectSamples), benchOrder);
	qsort(PingSamples, Count, sizeof(*PingSamples), benchOrder);
	if (Outgoing) {
		Stage = "retire publish exchanges";
		double Deadline = xrtTimer() + 5.0;
		for (;;) {
			xmqttserverinfo Info;
			if (!xmqttServerInfo(Example.Server, &Info) || Info.OutgoingPublishOverloads || xrtTimer() >= Deadline)
				goto Done;
			if (!Info.IncomingPublishes && !Info.OutgoingPublishes && !Info.OutgoingPublishBytes) break;
			xrtSleep(1);
		}
	}
	if (Routing) {
		const unsigned char Ping[] = {0xc0,0};
		unsigned char Pong[2];
		xmqttserverinfo Info;
		Stage = "verify publisher and delivery totals";
		if (Will) {
			double Deadline = xrtTimer() + 5.0;
			do {
				if (!xmqttServerInfo(Example.Server, &Info) || xrtTimer() >= Deadline) goto Done;
				if (Info.Connections == Peers && !Info.WillBytes) break;
				xrtSleep(1);
			} while (true);
		} else if (!benchSend(Sockets[Peers], Ping, sizeof(Ping)) || !benchReceive(Sockets[Peers], Pong, sizeof(Pong)) ||
			Pong[0] != 0xd0 || Pong[1] || !xmqttServerInfo(Example.Server, &Info)) goto Done;
		if (Info.Forwarded != Count * (Takeover ? 2u : 1u) || Info.DroppedDeliveries ||
			Info.Subscriptions != Peers * (Overlap ? 2u : 1u) || Info.RetainedTopics != (size_t)Replay) goto Done;
	}
	printf("connect_per_sec: %.3f\nconnect_p99_us: %.3f\n",
		(double)Connections / ConnectSeconds, benchPercentile(ConnectSamples, Connections, 0.99));
	printf("%s: %.3f\n%s_p50_us: %.3f\n%s_p99_us: %.3f\n%s_p999_us: %.3f\n"
		"xrt_requested_blocks_per_%s: %.6f\n",
		Routing ? "deliveries_per_sec" : "pings_per_sec", (double)Count / PingSeconds,
		Routing ? "delivery" : "ping", benchPercentile(PingSamples, Count, 0.5),
		Routing ? "delivery" : "ping", benchPercentile(PingSamples, Count, 0.99),
		Routing ? "delivery" : "ping", benchPercentile(PingSamples, Count, 0.999),
		Routing ? "delivery" : "ping",
		(double)(After.BlockAllocCalls - Before.BlockAllocCalls) / (double)Count);
	OK = true;

Done:
	if (!OK) {
		const xerror* Error = xrtGetError();
		fprintf(stderr, "%s: %s (%ld)\n", Stage, Error ? xrtErrorMessage(Error) : "unexpected peer result",
			(long)(Error ? xrtErrorSystemCode(Error) : 0));
	}
	/* Keep the peers open until broker shutdown also verifies bounded joins. */
	if (!exampleServerStop(&Example)) {
		const xerror* Error = xrtGetError();
		fprintf(stderr, "retire listener: %s\n", Error ? xrtErrorMessage(Error) : "incomplete teardown");
		OK = false;
	}
	if (Sockets) {
		for (size_t Index = 0; Index < Connections; ++Index) if (Sockets[Index]) xrtNetSocketClose(Sockets[Index]);
	}
	free(PingSamples);
	free(SentAt);
	free(ConnectSamples);
	free(Sockets);
	return OK ? 0 : 1;
}
