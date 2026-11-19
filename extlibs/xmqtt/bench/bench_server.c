#include "../examples/server/app_listener.h"
#include <xrt/memory_stats.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
	const size_t Count = Peers * Rounds;
	xnetsocket* Sockets = calloc(Peers, sizeof(*Sockets));
	double* ConnectSamples = malloc(Peers * sizeof(*ConnectSamples));
	double* SentAt = malloc(Peers * sizeof(*SentAt));
	double* PingSamples = malloc(Count * sizeof(*PingSamples));
	serverexample Example = {0};
	bool OK = false;
	const char* Stage = "allocate samples";
	if (!Sockets || !ConnectSamples || !SentAt || !PingSamples) goto Done;

	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.MaxConnections = Peers;
	Stage = "start listener";
	if (!exampleServerStart(&Example, &Config, Workers)) goto Done;

	Stage = "connect and receive CONNACK";
	double Start = xrtTimer();
	for (size_t Index = 0; Index < Peers; ++Index) {
		double Attempt = xrtTimer();
		Sockets[Index] = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0);
		if (!Sockets[Index] || !benchConnect(Sockets[Index], &Example.Address, Index)) goto Done;
		ConnectSamples[Index] = xrtTimer() - Attempt;
	}
	double ConnectSeconds = xrtTimer() - Start;

	Stage = "send PING and receive PINGRESP";
	xmemstats Before, After;
	xrtMemStatsEnable(true);
	xrtMemStatsGet(&Before);
	Start = xrtTimer();
	for (size_t Round = 0; Round < Rounds; ++Round) {
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
	qsort(ConnectSamples, Peers, sizeof(*ConnectSamples), benchOrder);
	qsort(PingSamples, Count, sizeof(*PingSamples), benchOrder);
	printf("connect_per_sec: %.3f\nconnect_p99_us: %.3f\npings_per_sec: %.3f\n"
		"ping_p50_us: %.3f\nping_p99_us: %.3f\nping_p999_us: %.3f\n"
		"xrt_requested_blocks_per_ping: %.6f\n",
		(double)Peers / ConnectSeconds, benchPercentile(ConnectSamples, Peers, 0.99),
		(double)Count / PingSeconds, benchPercentile(PingSamples, Count, 0.5),
		benchPercentile(PingSamples, Count, 0.99), benchPercentile(PingSamples, Count, 0.999),
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
		for (size_t Index = 0; Index < Peers; ++Index) if (Sockets[Index]) xrtNetSocketClose(Sockets[Index]);
	}
	free(PingSamples);
	free(SentAt);
	free(ConnectSamples);
	free(Sockets);
	return OK ? 0 : 1;
}
