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

static bool benchConnect(xnetsocket Socket, const xnetaddr* Address, size_t Index)
{
	/* Eight ASCII digits keep each identity distinct and valid in MQTT. */
	unsigned char Frame[] = {0x10,21,0,4,'M','Q','T','T',5,2,0,0,0,0,8,0,0,0,0,0,0,0,0};
	for (size_t Pos = sizeof(Frame); Pos > 15;) {
		Frame[--Pos] = '0' + (unsigned char)(Index % 10u);
		Index /= 10u;
	}
	unsigned char Reply[48];
	return xrtNetSocketSet(Socket, XNET_OPTION_NO_DELAY, 1) &&
		xrtNetSocketConnect(Socket, Address) == XNET_RESULT_OK &&
		benchSend(Socket, Frame, sizeof(Frame)) && benchReceive(Socket, Reply, 2) &&
		Reply[0] == 0x20 && Reply[1] >= 3 && Reply[1] <= sizeof(Reply) - 2 &&
		benchReceive(Socket, Reply + 2, Reply[1]) && !Reply[2] && !Reply[3];
}

static bool benchSubscribe(xnetsocket Socket, bool Overlap, bool Shared)
{
	const unsigned char Request[] = {0x82,10,0,1,0,0,4,'l','o','a','d',0};
	const unsigned char Group[] = {0x82,19,0,1,0,0,13,'$','s','h','a','r','e','/','g','/','l','o','a','d',0};
	const unsigned char Extra[] = {0x82,16,0,1,0,0,4,'l','o','a','d',0,0,3,'+','/','#',0};
	const unsigned char Ack[] = {0x90,4,0,1,0,0}, ExtraAck[] = {0x90,5,0,1,0,0,0};
	unsigned char Reply[sizeof(ExtraAck)];
	size_t Bytes = Overlap ? sizeof(ExtraAck) : sizeof(Ack);
	const unsigned char* Wire = Shared ? Group : (Overlap ? Extra : Request);
	size_t WireSize = Shared ? sizeof(Group) : (Overlap ? sizeof(Extra) : sizeof(Request));
	return benchSend(Socket, Wire, WireSize) &&
		benchReceive(Socket, Reply, Bytes) && !memcmp(Reply, Overlap ? ExtraAck : Ack, Bytes);
}

static bool benchPublishRound(const xnetsocket* Sockets, size_t Peers, size_t Round, size_t Deliveries, bool Shared, double* Samples)
{
	/* MQTT 5 packet with 128 bytes of binary payload and no properties. */
	unsigned char Frame[138] = {0x30,0x87,1,0,4,'l','o','a','d',0};
	Frame[sizeof(Frame)-4] = (uint8)(Round >> 24);
	Frame[sizeof(Frame)-3] = (uint8)(Round >> 16);
	Frame[sizeof(Frame)-2] = (uint8)(Round >> 8);
	Frame[sizeof(Frame)-1] = (uint8)Round;
	double SentAt = xrtTimer();
	if (!benchSend(Sockets[Peers], Frame, sizeof(Frame))) return false;
	size_t First = Round % Peers;
	/* Group membership adds to the head. The first joiner starts the
	 * cursor and later joiners are traversed in reverse join order. */
	if (Shared && First) First = Peers - First;
	for (size_t Offset = 0; Offset < Deliveries; ++Offset) {
		unsigned char Reply[sizeof(Frame)];
		if (!benchReceive(Sockets[(First + Offset) % Peers], Reply, sizeof(Reply)) ||
			memcmp(Reply, Frame, sizeof(Frame))) return false;
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
	bool Routing = Shared || Overlap || (Argc > 4 && !strcmp(Argv[4], "route"));
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
	if (Routing) Config.MaxSubscribersPerTopic = Peers;
	Stage = "start listener";
	if (!exampleServerStart(&Example, &Config, Workers)) goto Done;

	Stage = "connect and receive CONNACK";
	double Start = xrtTimer();
	for (size_t Index = 0; Index < Connections; ++Index) {
		double Attempt = xrtTimer();
		Sockets[Index] = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		if (!Sockets[Index] || !benchConnect(Sockets[Index], &Example.Address, Index)) goto Done;
		ConnectSamples[Index] = xrtTimer() - Attempt;
	}
	double ConnectSeconds = xrtTimer() - Start;

	if (Routing) {
		Stage = "subscribe topic";
		for (size_t Index = 0; Index < Peers; ++Index) {
			if (!benchSubscribe(Sockets[Index], Overlap, Shared)) goto Done;
		}
	}
	Stage = Routing ? "publish and verify fan-out" : "send PING and receive PINGRESP";
	xmemstats Before, After;
	xrtMemStatsEnable(true);
	xrtMemStatsGet(&Before);
	Start = xrtTimer();
	for (size_t Round = 0; Round < Rounds; ++Round) {
		if (Routing) {
			if (!benchPublishRound(Sockets, Peers, Round, Deliveries, Shared, PingSamples + Round * Deliveries)) goto Done;
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
	if (Routing) {
		const unsigned char Ping[] = {0xc0,0};
		unsigned char Pong[2];
		xmqttserverinfo Info;
		if (!benchSend(Sockets[Peers], Ping, sizeof(Ping)) || !benchReceive(Sockets[Peers], Pong, sizeof(Pong)) ||
			Pong[0] != 0xd0 || Pong[1] || !xmqttServerInfo(Example.Server, &Info) || Info.Forwarded != Count || Info.DroppedDeliveries ||
			Info.Subscriptions != Peers * (Overlap ? 2u : 1u)) goto Done;
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
