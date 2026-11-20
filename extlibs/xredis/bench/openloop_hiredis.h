#ifndef XREDIS_BENCH_OPENLOOP_HIREDIS_H
#define XREDIS_BENCH_OPENLOOP_HIREDIS_H
#include <hiredis.h>
#if !defined(_WIN32)
#include <fcntl.h>
#endif

#if HIREDIS_MAJOR != 1 || HIREDIS_MINOR != 3 || HIREDIS_PATCH != 0
#error This comparison uses Hiredis 1.3.0.
#endif

static redisContext* redisOpenHiredisConnect(const redisopenconfig* Config, unsigned Index, unsigned char* Payload)
{
	struct timeval Timeout = {5, 0};
	redisOptions Options = {0};
	REDIS_OPTIONS_SET_TCP(&Options, "127.0.0.1", (int)Config->Port);
	Options.connect_timeout = &Timeout;
	Options.command_timeout = &Timeout;
	redisContext* Client = redisConnectWithOptions(&Options);
	redisOpenRequire(Client != NULL && Client->err == 0, "Hiredis connection");

	redisReply* Hello = redisCommand(Client, "HELLO 3");
	redisOpenRequire(Hello != NULL && Hello->type == REDIS_REPLY_MAP, "Hiredis HELLO map");
	bool Resp3 = false;
	for (size_t Index = 0; Index + 1 < Hello->elements; Index += 2) {
		const redisReply* Key = Hello->element[Index];
		const redisReply* Value = Hello->element[Index + 1];
		if ((Key->type == REDIS_REPLY_STRING || Key->type == REDIS_REPLY_STATUS) &&
			Key->len == 5 && memcmp(Key->str, "proto", 5) == 0)
			Resp3 = Value->type == REDIS_REPLY_INTEGER && Value->integer == 3;
	}
	freeReplyObject(Hello);
	redisOpenRequire(Resp3, "Hiredis HELLO protocol");
	redisReply* Ping = redisCommand(Client, "PING");
	redisOpenRequire(Ping != NULL && Ping->type == REDIS_REPLY_STATUS &&
		Ping->len == 4 && memcmp(Ping->str, "PONG", 4) == 0, "Hiredis warmup");
	freeReplyObject(Ping);
	if (Config->Mixed && Index == 0) {
		redisOpenPayload(Payload, Config->Payload, 0);
		const char* Args[] = {"SET", (const char*)RedisOpenReadKey.Data, (const char*)Payload};
		const size_t Sizes[] = {3, RedisOpenReadKey.Size, Config->Payload};
		redisReply* Seed = redisCommandArgv(Client, 3, Args, Sizes);
		redisOpenRequire(Seed && Seed->type == REDIS_REPLY_STATUS && Seed->len == 2 &&
			memcmp(Seed->str, "OK", 2) == 0, "Hiredis seed SET");
		freeReplyObject(Seed);
	}

	/* Complete the connection and TCP_NODELAY setup via the bounded blocking
	 * connect first. The measured driver advances the nonblocking public
	 * Hiredis buffers itself, so writes and reads cannot move arrival times. */
#if defined(_WIN32)
	u_long Nonblocking = 1;
	redisOpenRequire(ioctlsocket(Client->fd, FIONBIO, &Nonblocking) == 0, "Hiredis nonblocking socket");
#else
	int Flags = fcntl(Client->fd, F_GETFL, 0);
	redisOpenRequire(Flags >= 0 && fcntl(Client->fd, F_SETFL, Flags | O_NONBLOCK) == 0,
		"Hiredis nonblocking socket");
#endif
	Client->flags &= ~REDIS_BLOCK;
	return Client;
}

typedef struct redisopenconnection {
	redisContext* Client;
	size_t Pending[128];
	size_t Read, Written;
	int Flushed;
} redisopenconnection;

static bool redisOpenRun(const redisopenconfig* Config, redisopenresult* Output)
{
	memset(Output, 0, sizeof(*Output));
	snprintf(Output->Backend, sizeof(Output->Backend), "hiredis_nonblocking");
	Output->HandlesBefore = redisOpenHandles();
	const size_t Window = Config->Slots < 128u ? Config->Slots : 128u;
	const unsigned Connections = redisOpenConnections(Config);
	redisopenconnection Peers[REDIS_OPEN_MAX_CONNECTIONS] = {0};
	redisopenrecord* Records = calloc(Config->Count, sizeof(*Records));
	uint64* Samples = malloc(Config->Count * sizeof(*Samples));
	unsigned char* Payload = malloc(Config->Payload);
	redisOpenRequire(Records != NULL && Samples != NULL && Payload != NULL, "Hiredis observer allocation");
	Output->HarnessBytes = Config->Count * (sizeof(*Records) + sizeof(*Samples)) + Config->Payload + sizeof(Peers);
	for (unsigned Index = 0; Index < Connections; ++Index) {
		Peers[Index].Client = redisOpenHiredisConnect(Config, Index, Payload);
		Peers[Index].Flushed = 1;
	}

	size_t Active = 0, Next = 0;
	const uint64 Timeout = (uint64)Config->Timeout * 1000000u;
	double CpuStart = redisOpenCpu(false), GeneratorStart = redisOpenCpu(true);
	uint64 Start = redisOpenNow();
	if (Config->Mode == 1) xrtSleep(Config->Stall);

	while (Next < Config->Count || Active) {
		for (unsigned Index = 0; Index < Connections; ++Index) {
			redisopenconnection* Peer = &Peers[Index];
			if (Peer->Read == Peer->Written) continue;
			redisContext* Client = Peer->Client;
			redisOpenRequire(Peer->Flushed || redisBufferWrite(Client, &Peer->Flushed) == REDIS_OK, "Hiredis write");
			redisOpenRequire(redisBufferRead(Client) == REDIS_OK, "Hiredis read");
			for (;;) {
				void* Value = NULL;
				redisOpenRequire(redisGetReplyFromReader(Client, &Value) == REDIS_OK, "Hiredis parse");
				if (Value == NULL) break;
				uint64 Finished = redisOpenNow();
				redisOpenRequire(Peer->Read != Peer->Written && Active, "unsolicited Hiredis reply");
				size_t Id = Peer->Pending[Peer->Read++ % Window];
				--Active;
				const redisReply* Reply = Value;
				bool Valid;
				if (redisOpenWrite(Config, Id))
					Valid = Reply->type == REDIS_REPLY_STATUS && Reply->len == 2 && memcmp(Reply->str, "OK", 2) == 0;
				else
					Valid = Reply->type == REDIS_REPLY_STRING && redisOpenBinary(
						(xbytesview){(cbytes)Reply->str, Reply->len}, Config->Payload, Config->Mixed ? 0 : Id);
				freeReplyObject(Value);
				redisOpenRequire(redisOpenFinish(&Records[Id], Finished, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK,
						Valid, Timeout, &Output->Counts), "Hiredis accepted ledger");
			}
			/* A timed-out command still occupies its connection's FIFO. */
			redisOpenRequire(Peer->Read == Peer->Written || redisOpenNow() - Records[Peer->Pending[Peer->Read % Window]].Due < Timeout,
				"Hiredis outstanding reply deadline");
		}

		for (size_t Work = 0; Next < Config->Count && Work < 128; ++Work, ++Next) {
			redisopenrecord* Record = &Records[Next];
			redisOpenRequire(redisOpenDue(Start, Next, Config->Rate, &Record->Due), "Hiredis fixed arrival timestamp");
			uint64 Now = redisOpenNow();
			if (Now < Record->Due) break;
			Record->Submitted = Now;
			redisopenoutcome Outcome = REDIS_OPEN_PENDING;
			if (Now - Record->Due >= Timeout) Outcome = REDIS_OPEN_EXPIRED;
			else if (Active == Window) Outcome = REDIS_OPEN_CAPACITY;
			else {
				xbytesview Parts[3];
				const size_t ArgCount = redisOpenCommand(Config, Next, Payload, Parts);
				const char* Args[3];
				size_t Sizes[3];
				for (size_t Part = 0; Part < ArgCount; ++Part) {
					Args[Part] = (const char*)Parts[Part].Data;
					Sizes[Part] = Parts[Part].Size;
				}
				Record->Submitted = redisOpenNow();
				if (Record->Submitted - Record->Due >= Timeout) Outcome = REDIS_OPEN_EXPIRED;
				else {
					redisopenconnection* Peer = &Peers[Next % Connections];
					redisOpenRequire(redisAppendCommandArgv(Peer->Client, (int)ArgCount, Args, Sizes) == REDIS_OK, "Hiredis append");
					Peer->Pending[Peer->Written++ % Window] = Next;
					Peer->Flushed = 0;
					if (++Active > Output->WindowPeak) Output->WindowPeak = Active;
					size_t Buffered = 0;
					for (unsigned Index = 0; Index < Connections; ++Index)
						Buffered += sdslen(Peers[Index].Client->obuf);
					if (Buffered > Output->CommandBytesPeak) Output->CommandBytesPeak = Buffered;
				}
			}
			if (Outcome != REDIS_OPEN_PENDING)
				redisOpenRequire(redisOpenFinish(Record, redisOpenNow(), Outcome, XREDIS_REQUEST_OK, true,
						Timeout, &Output->Counts), "Hiredis rejected ledger");
		}
		xrtThreadYield();
	}
	uint64 LogicalEnd = redisOpenNow();
	Output->Elapsed = (double)(LogicalEnd - Start) / (double)REDIS_OPEN_NS;
	Output->RequestsPeak = Output->WindowPeak;
	size_t Read = 0;
	for (unsigned Index = 0; Index < Connections; ++Index) {
		const redisopenconnection* Peer = &Peers[Index];
		redisOpenRequire(Peer->Flushed && Peer->Read == Peer->Written, "Hiredis connection drain");
		Read += Peer->Read;
		redisFree(Peer->Client);
	}
	redisOpenRequire(Read == Output->Counts.Accepted, "Hiredis drain accounting");
	Output->Retire = (double)(redisOpenNow() - LogicalEnd) / (double)REDIS_OPEN_NS;
	Output->GeneratorCpu = redisOpenCpu(true) - GeneratorStart;
	Output->Cpu = redisOpenCpu(false) - CpuStart;
	redisOpenSummarize(Config, Output, Records, Samples);
	free(Payload);
	free(Samples);
	free(Records);
	Output->HandlesAfter = redisOpenHandles();
	return Output->Counts.Corrupt == 0 && (Config->Mode != 0 || Output->Counts.Successful != 0);
}
#endif
