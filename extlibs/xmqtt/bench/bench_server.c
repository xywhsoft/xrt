#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif
#include "../examples/server/app_listener.h"
#include <xrt/memory_stats.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool benchReceive(xnetsocket Socket, unsigned char* Data, size_t Size)
{
	while (Size) {
		size_t Read;
		if (xrtNetSocketRecv(Socket, Data, Size, &Read) != XNET_RESULT_OK || !Read) return false;
		Data += Read;
		Size -= Read;
	}
	return true;
}

static bool benchConnectFrame(xnetsocket Socket, const xnetaddr* Address,
	const unsigned char* Frame, size_t Size, bool Resume, uint16 RequiredWindow)
{
	unsigned char Reply[48];
	if (!xrtNetSocketSet(Socket, XNET_OPTION_NO_DELAY, 1) ||
		xrtNetSocketConnect(Socket, Address) != XNET_RESULT_OK ||
		!benchSend(Socket, Frame, Size) || !benchReceive(Socket, Reply, 2) ||
		Reply[0] != 0x20 || Reply[1] < 3 || Reply[1] > sizeof(Reply) - 2 ||
		!benchReceive(Socket, Reply + 2, Reply[1]) || Reply[2] != Resume || Reply[3]) return false;
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

static bool benchConnectLimits(xnetsocket Socket, const xnetaddr* Address, size_t Index, uint16 Window,
	bool Persistent, bool Resume, uint16 RequiredWindow)
{
	/* Eight ASCII digits keep each identity distinct and valid in MQTT. */
	unsigned char Frame[31] = {0x10,0,0,4,'M','Q','T','T',5,Resume ? 0 : 2,0,0};
	size_t Size = 13;
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
	return benchConnectFrame(Socket, Address, Frame, Size, Resume, RequiredWindow);
}

static bool benchConnect(xnetsocket Socket, const xnetaddr* Address, size_t Index, bool Window,
	bool Persistent, bool Resume)
{
	return benchConnectLimits(Socket, Address, Index, Window, Persistent, Resume, 0);
}

static const unsigned char ExactSubscribe[] = {0x82,10,0,1,0,0,4,'l','o','a','d',0};
static const unsigned char SubscribeAck[] = {0x90,4,0,1,0,0};
static const unsigned char RetainedFrame[138] = {0x31,0x87,1,0,4,'l','o','a','d',0};

static bool benchSubscribe(xnetsocket Socket, bool Overlap, bool Shared, uint8 Qos)
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
	return benchSend(Socket, Wire, WireSize) && benchReceive(Socket, Reply, Bytes) && !memcmp(Reply, Expected, Bytes);
}

static bool benchReceiveFlowReply(xnetsocket Socket, unsigned char Header, uint16 Id)
{
	const unsigned char Expected[] = {Header, 2, (uint8)(Id >> 8), (uint8)Id};
	unsigned char Reply[sizeof(Expected)];
	return benchReceive(Socket, Reply, sizeof(Reply)) && !memcmp(Reply, Expected, sizeof(Reply));
}

static bool benchSendFlowReply(xnetsocket Socket, unsigned char Header, uint16 Id)
{
	const unsigned char Wire[] = {Header, 2, (uint8)(Id >> 8), (uint8)Id};
	return benchSend(Socket, Wire, sizeof(Wire));
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

typedef struct benchRemotePeer {
	xnetsocket Socket;
	double Start, Rate;
	size_t Warmup, Rounds;
	double* Samples;
	double Last;
	uint8 Qos;
	uint16 Window;
	bool Complete;
} benchRemotePeer;

static bool benchPing(xnetsocket Socket)
{
	const unsigned char Wire[] = {0xc0,0};
	unsigned char Reply[2];
	return benchSend(Socket, Wire, sizeof(Wire)) && benchReceive(Socket, Reply, sizeof(Reply)) &&
		Reply[0] == 0xd0 && !Reply[1];
}

typedef struct benchRemoteReader {
	xnetsocket Socket;
	unsigned char Bytes[4096];
	size_t Begin, End;
} benchRemoteReader;

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
		size_t Read = 0;
		if (xrtNetSocketRecv(Reader->Socket, Reader->Bytes + Pending, sizeof(Reader->Bytes) - Pending,
			&Read) != XNET_RESULT_OK || !Read) return false;
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
	}
	Peer->Last = Arrived;
	return true;
}

static bool benchRemoteReadReliable(benchRemotePeer* Peer)
{
	typedef struct { uint16 Id; size_t Round; } pending;
	pending Pending[64] = {0};
	benchRemoteReader Reader = {.Socket = Peer->Socket};
	size_t Received = 0, Finished = 0, Total = Peer->Warmup + Peer->Rounds;
	while (Finished < Total) {
		xmqttframe Frame;
		if (!benchRemoteReadFrame(&Reader, &Frame)) return false;
		if (Frame.Header.Type == XMQTT_PACKET_PUBLISH && Received < Total && Frame.Wire.Size == 140) {
			uint16 Id = benchRemotePacketId(Frame.Wire.Data + 9);
			unsigned char Expected[140];
			size_t Size = benchRemoteFrame(Expected, Received, Peer->Qos, Id);
			if (!Id || memcmp(Frame.Wire.Data, Expected, Size)) return false;
			if (Peer->Qos == 2) {
				pending* Free = NULL;
				for (size_t I = 0; I < Peer->Window; ++I) {
					if (Pending[I].Id == Id) return false;
					if (!Pending[I].Id && !Free) Free = Pending + I;
				}
				if (!Free || !benchSendFlowReply(Peer->Socket, 0x50, Id)) return false;
				*Free = (pending){Id, Received};
			} else {
				if (!benchSendFlowReply(Peer->Socket, 0x40, Id) ||
					!benchRemoteDelivery(Peer, Received, xrtTimer())) return false;
				++Finished;
			}
			++Received;
		} else if (Peer->Qos == 2 && Frame.Header.Type == XMQTT_PACKET_PUBREL) {
			uint16 Id = benchRemoteAckId(&Frame);
			size_t I = 0;
			if (!Id) return false;
			while (I < Peer->Window && Pending[I].Id != Id) ++I;
			if (I == Peer->Window || !benchSendFlowReply(Peer->Socket, 0x70, Id) ||
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
	if (Peer->Qos) {
		Peer->Complete = benchRemoteReadReliable(Peer);
		return Peer->Complete ? 0 : 1;
	}
	for (size_t Round = 0; Round < Peer->Warmup + Peer->Rounds; ++Round) {
		unsigned char Wire[138], Expected[138];
		if (!benchReceive(Peer->Socket, Wire, sizeof(Wire))) return 1;
		double Arrived = xrtTimer();
		benchRemoteFrame(Expected, Round, 0, 0);
		if (memcmp(Wire, Expected, sizeof(Wire)) || !benchRemoteDelivery(Peer, Round, Arrived)) return 1;
	}
	Peer->Complete = true;
	return 0;
}

typedef enum { BENCH_PUBLISH_FREE, BENCH_PUBLISH_WAIT_RECEIPT, BENCH_PUBLISH_WAIT_COMPLETE } benchPublishPhase;
typedef struct benchRemotePublisher {
	xnetsocket Socket;
	xmutex Gate;
	xcond Changed;
	struct { benchPublishPhase Phase; size_t Round; } Slots[64];
	uint8 Qos;
	size_t Window, Warmup, Rounds;
	double Start, Rate, Last;
	double* Samples;
	bool Complete, Failed;
} benchRemotePublisher;

/* Publisher and ACK reader share only one short send and slot transition
 * at a time. Packet identifiers are recycled only once fully confirmed. */
static bool benchRemoteOffer(benchRemotePublisher* P, size_t Round, double Scheduled, double* Lag)
{
	if (!P->Qos) {
		unsigned char Frame[138];
		benchRemoteFrame(Frame, Round, 0, 0);
		if (Lag) *Lag = xrtTimer() - Scheduled;
		return benchSend(P->Socket, Frame, sizeof(Frame));
	}
	size_t Slot = Round % P->Window;
	if (!xrtMutexLock(&P->Gate)) return false;
	while (!P->Failed && P->Slots[Slot].Phase != BENCH_PUBLISH_FREE) {
		if (xrtCondWaitFor(&P->Changed, &P->Gate, 5000) != XWAIT_OK) {
			(void)xrtMutexUnlock(&P->Gate);
			return false;
		}
	}
	bool Sent = false;
	if (!P->Failed) {
		unsigned char Frame[140];
		size_t Size = benchRemoteFrame(Frame, Round, P->Qos, (uint16)(Slot + 1));
		P->Slots[Slot].Round = Round;
		P->Slots[Slot].Phase = BENCH_PUBLISH_WAIT_RECEIPT;
		if (Lag) *Lag = xrtTimer() - Scheduled;
		Sent = benchSend(P->Socket, Frame, Size);
	}
	return xrtMutexUnlock(&P->Gate) && Sent;
}

static int32 benchRemoteConfirm(ptr Context)
{
	benchRemotePublisher* P = Context;
	benchRemoteReader Reader = {.Socket = P->Socket};
	size_t Finished = 0, Total = P->Warmup + P->Rounds;
	while (Finished < Total) {
		xmqttframe Frame;
		if (!benchRemoteReadFrame(&Reader, &Frame)) break;
		double Confirmed = xrtTimer();
		uint16 Id = benchRemoteAckId(&Frame);
		if (!Id || Id > P->Window || !xrtMutexLock(&P->Gate)) break;
		size_t Slot = Id - 1u;
		benchPublishPhase Phase = P->Slots[Slot].Phase;
		xmqttpackettype Type = Frame.Header.Type;
		bool Terminal = (P->Qos == 1 && Phase == BENCH_PUBLISH_WAIT_RECEIPT && Type == XMQTT_PACKET_PUBACK) ||
			(P->Qos == 2 && Phase == BENCH_PUBLISH_WAIT_COMPLETE && Type == XMQTT_PACKET_PUBCOMP);
		bool OK = true;
		if (Terminal) {
			size_t Round = P->Slots[Slot].Round;
			if (Round >= P->Warmup) P->Samples[Round - P->Warmup] = Confirmed - (P->Start + (double)Round / P->Rate);
			P->Last = Confirmed;
			P->Slots[Slot].Phase = BENCH_PUBLISH_FREE;
			OK = xrtCondSignal(&P->Changed);
		} else if (P->Qos == 2 && Phase == BENCH_PUBLISH_WAIT_RECEIPT && Type == XMQTT_PACKET_PUBREC) {
			P->Slots[Slot].Phase = BENCH_PUBLISH_WAIT_COMPLETE;
			OK = benchSendFlowReply(P->Socket, 0x62, Id);
		} else OK = false;
		if (!xrtMutexUnlock(&P->Gate) || !OK) break;
		if (Terminal) ++Finished;
	}
	P->Complete = Finished == Total && Reader.Begin == Reader.End;
	if (!P->Complete && xrtMutexLock(&P->Gate)) {
		P->Failed = true;
		(void)xrtCondSignal(&P->Changed);
		(void)xrtMutexUnlock(&P->Gate);
	}
	return P->Complete ? 0 : 1;
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

static bool benchRemoteNumber(const char* Text, unsigned long Min, unsigned long Max, unsigned long* Out)
{
	char* End;
	if (*Text < '0' || *Text > '9') return false;
	unsigned long Value = strtoul(Text, &End, 10);
	if (*End || Value < Min || Value > Max) return false;
	*Out = Value;
	return true;
}

/* Independent wire observer. The external broker and the load driver run
 * in separate processes. Every scheduled arrival gets an observed sample
 * from every subscriber or the entire repeat is recorded as a failure. */
static int benchRemote(int Argc, char** Argv)
{
	unsigned long Port, Peers, Rounds, Rate, Warmup, Qos = 0, Window = 1;
	if ((Argc != 8 && Argc != 10) || !benchRemoteNumber(Argv[2], 1, 65535, &Port) ||
		!benchRemoteNumber(Argv[3], 1, 64, &Peers) ||
		!benchRemoteNumber(Argv[4], 1, 100000, &Rounds) ||
		!benchRemoteNumber(Argv[5], 1, 100000, &Rate) ||
		!benchRemoteNumber(Argv[6], 0, 100000, &Warmup) ||
		(strcmp(Argv[7], "exact") && strcmp(Argv[7], "wildcard")) ||
		(Argc == 10 && (!benchRemoteNumber(Argv[8], 1, 2, &Qos) ||
			!benchRemoteNumber(Argv[9], 1, 64, &Window)))) {
		fprintf(stderr, "Usage: bench_server --remote PORT SUBSCRIBERS ROUNDS RATE WARMUP exact|wildcard [QOS WINDOW]\n");
		return 2;
	}
	benchRemotePeer State[64] = {0};
	xthread* Readers[64] = {0};
	double* Samples = malloc(Peers * Rounds * sizeof(*Samples));
	double* Lag = malloc(Rounds * sizeof(*Lag));
	benchRemotePublisher Publisher = {.Qos = (uint8)Qos, .Window = Window, .Warmup = Warmup,
		.Rounds = Rounds, .Rate = (double)Rate};
	xthread* Confirmations = NULL;
	bool Gate = false, Changed = false;
	bool OK = false;
	const char* Stage = "allocate measurement buffers";
	if (!Samples || !Lag) goto Done;
	if (Qos) {
		Publisher.Samples = malloc(Rounds * sizeof(*Publisher.Samples));
		if (!Publisher.Samples || !(Gate = xrtMutexInit(&Publisher.Gate)) ||
			!(Changed = xrtCondInit(&Publisher.Changed))) goto Done;
	}
	xnetaddr Address;
	xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, (uint16)Port);
	Stage = "connect and subscribe";
	bool Wild = !strcmp(Argv[7], "wildcard");
	const unsigned char Wildcard[] = {0x82,7,0,1,0,0,1,'+',(uint8)Qos};
	const unsigned char ExpectedAck[] = {0x90,4,0,1,0,(uint8)Qos};
	for (size_t I = 0; I < Peers; ++I) {
		State[I].Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		if (!State[I].Socket || !benchRemoteTimeouts(State[I].Socket) ||
			!benchConnectLimits(State[I].Socket, &Address, I,
			Qos ? (uint16)Window : 0, false, false, 0)) goto Done;
		if (Wild) {
			unsigned char Reply[sizeof(ExpectedAck)];
			if (!benchSend(State[I].Socket, Wildcard, sizeof(Wildcard)) ||
				!benchReceive(State[I].Socket, Reply, sizeof(Reply)) ||
				memcmp(Reply, ExpectedAck, sizeof(Reply))) goto Done;
		} else if (!benchSubscribe(State[I].Socket, false, false, (uint8)Qos)) goto Done;
	}
	Publisher.Socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
	if (!Publisher.Socket || !benchRemoteTimeouts(Publisher.Socket) ||
		!benchConnectLimits(Publisher.Socket, &Address, Peers, 0, false, false, Qos ? (uint16)Window : 0))
		goto Done;

	Stage = "start subscribers";
	double Start = xrtTimer() + 0.05;
	Publisher.Start = Start;
	if (Qos) {
		Confirmations = xrtThreadCreate(benchRemoteConfirm, &Publisher, 0);
		if (!Confirmations) goto Done;
	}
	for (size_t I = 0; I < Peers; ++I) {
		State[I].Start = Start;
		State[I].Rate = (double)Rate;
		State[I].Warmup = Warmup;
		State[I].Rounds = Rounds;
		State[I].Qos = (uint8)Qos;
		State[I].Window = (uint16)Window;
		State[I].Samples = Samples + I * Rounds;
		Readers[I] = xrtThreadCreate(benchRemoteRead, State + I, 0);
		if (!Readers[I]) goto Done;
	}
	Stage = "offer scheduled publications";
	for (size_t Round = 0; Round < Warmup + Rounds; ++Round) {
		double Scheduled = Start + (double)Round / (double)Rate;
		double Now;
		while ((Now = xrtTimer()) < Scheduled) {
			if (Scheduled - Now > 0.003) xrtSleep(1);
		}
		if (!benchRemoteOffer(&Publisher, Round, Scheduled, Round >= Warmup ? Lag + Round - Warmup : NULL))
			goto Done;
	}
	Stage = "check subscriber delivery counts and order";
	double Last = 0;
	for (size_t I = 0; I < Peers; ++I) {
		if (xrtThreadWaitFor(Readers[I], 5000) != XWAIT_OK || !State[I].Complete) goto Done;
		if (State[I].Last > Last) Last = State[I].Last;
	}
	if (Confirmations) {
		Stage = "check publisher confirmation counts";
		if (xrtThreadWaitFor(Confirmations, 5000) != XWAIT_OK || !Publisher.Complete) goto Done;
		if (Publisher.Last > Last) Last = Publisher.Last;
	}
	Stage = "check for excess packets";
	if (Qos && !benchPing(Publisher.Socket)) goto Done;
	for (size_t I = 0; I < Peers; ++I) if (!benchPing(State[I].Socket)) goto Done;
	double Seconds = Last - (Start + (double)Warmup / (double)Rate);
	if (Seconds <= 0) goto Done;
	size_t Count = Peers * Rounds;
	qsort(Samples, Count, sizeof(*Samples), benchOrder);
	qsort(Lag, Rounds, sizeof(*Lag), benchOrder);
	printf("offered: %lu\ndeliveries: %zu\nmeasurement_seconds: %.9f\n"
		"ingress_per_sec: %.3f\ndeliveries_per_sec: %.3f\n"
		"delivery_p50_us: %.3f\ndelivery_p95_us: %.3f\ndelivery_p99_us: %.3f\n"
		"delivery_p999_us: %.3f\noffer_lag_p99_us: %.3f\n",
		Rounds, Count, Seconds, (double)Rounds / Seconds, (double)Count / Seconds,
		benchPercentile(Samples, Count, 0.5), benchPercentile(Samples, Count, 0.95),
		benchPercentile(Samples, Count, 0.99), benchPercentile(Samples, Count, 0.999),
		benchPercentile(Lag, Rounds, 0.99));
	if (Qos) {
		qsort(Publisher.Samples, Rounds, sizeof(*Publisher.Samples), benchOrder);
		printf("publish_confirmed: %lu\npublisher_p99_us: %.3f\n", Rounds,
			benchPercentile(Publisher.Samples, Rounds, 0.99));
	}
	OK = true;

Done:
	if (!OK) fprintf(stderr, "Remote benchmark failed during %s\n", Stage);
	for (size_t I = 0; I < Peers; ++I)
		if (Readers[I]) (void)xrtNetSocketShutdown(State[I].Socket, XNET_SHUTDOWN_BOTH);
	for (size_t I = 0; I < Peers; ++I) {
		if (Readers[I]) {
			if (xrtThreadWait(Readers[I]) != XWAIT_OK) abort();
			xrtThreadDestroy(Readers[I]);
		}
		if (State[I].Socket) xrtNetSocketClose(State[I].Socket);
	}
	if (Confirmations) {
		(void)xrtNetSocketShutdown(Publisher.Socket, XNET_SHUTDOWN_BOTH);
		if (xrtThreadWait(Confirmations) != XWAIT_OK) abort();
		xrtThreadDestroy(Confirmations);
	}
	if (Publisher.Socket) xrtNetSocketClose(Publisher.Socket);
	if (Changed) (void)xrtCondUnit(&Publisher.Changed);
	if (Gate) (void)xrtMutexUnit(&Publisher.Gate);
	free(Publisher.Samples);
	free(Lag);
	free(Samples);
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
