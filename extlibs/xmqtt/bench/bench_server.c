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

static bool benchConnect(xnetsocket Socket, const xnetaddr* Address, size_t Index, bool Window,
	bool Persistent, bool Resume)
{
	/* Eight ASCII digits keep each identity distinct and valid in MQTT. */
	unsigned char Frame[31] = {0x10,0,0,4,'M','Q','T','T',5,Resume ? 0 : 2,0,0};
	size_t Size = 13;
	if (Window) {
		Frame[Size++] = 0x21;
		Frame[Size++] = 0;
		Frame[Size++] = 1;
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
	unsigned char Reply[48];
	return xrtNetSocketSet(Socket, XNET_OPTION_NO_DELAY, 1) &&
		xrtNetSocketConnect(Socket, Address) == XNET_RESULT_OK &&
		benchSend(Socket, Frame, Size) && benchReceive(Socket, Reply, 2) &&
		Reply[0] == 0x20 && Reply[1] >= 3 && Reply[1] <= sizeof(Reply) - 2 &&
		benchReceive(Socket, Reply + 2, Reply[1]) && Reply[2] == Resume && !Reply[3];
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

int main(int Argc, char** Argv)
{
	size_t Peers = Argc > 1 ? strtoul(Argv[1], NULL, 10) : 64;
	size_t Rounds = Argc > 2 ? strtoul(Argv[2], NULL, 10) : 1000;
	unsigned Workers = Argc > 3 ? strtoul(Argv[3], NULL, 10) : 2;
	if (Peers < 1 || Peers > 4096 || Rounds < 1 || Rounds > 10000 || Workers < 1 || Workers > 32)
		return 1;
	bool Overlap = Argc > 4 && !strcmp(Argv[4], "overlap");
	bool Shared = Argc > 4 && !strcmp(Argv[4], "shared");
	bool Replay = Argc > 4 && !strcmp(Argv[4], "retained");
	bool Persistent = Argc > 4 && !strcmp(Argv[4], "resume");
	bool Qos2 = Argc > 4 && !strcmp(Argv[4], "outbound2");
	bool Outgoing = Persistent || Qos2 || (Argc > 4 && !strcmp(Argv[4], "outbound1"));
	uint8 Qos = Qos2 ? 2 : (Outgoing || (Argc > 4 && !strcmp(Argv[4], "inbound1")));
	bool Routing = Qos || Replay || Shared || Overlap || (Argc > 4 && !strcmp(Argv[4], "route"));
	const size_t Connections = Peers + Routing, Deliveries = Shared ? 1 : Peers;
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
	Config.MaxConnections = Connections;
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
		if (!benchSend(Sockets[Peers], Ping, sizeof(Ping)) || !benchReceive(Sockets[Peers], Pong, sizeof(Pong)) ||
			Pong[0] != 0xd0 || Pong[1] || !xmqttServerInfo(Example.Server, &Info) || Info.Forwarded != Count || Info.DroppedDeliveries ||
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
