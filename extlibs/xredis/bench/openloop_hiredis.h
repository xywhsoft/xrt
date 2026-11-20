#ifndef XREDIS_BENCH_OPENLOOP_HIREDIS_H
#define XREDIS_BENCH_OPENLOOP_HIREDIS_H
#include <hiredis.h>
#if !defined(_WIN32)
#include <fcntl.h>
#endif

#if HIREDIS_MAJOR != 1 || HIREDIS_MINOR != 3 || HIREDIS_PATCH != 0
#error This comparison uses Hiredis 1.3.0.
#endif

static redisContext* redisOpenHiredisConnect(uint32 Port)
{
	struct timeval Timeout = {5, 0};
	redisOptions Options = {0};
	REDIS_OPTIONS_SET_TCP(&Options, "127.0.0.1", (int)Port);
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

static bool redisOpenRun(const redisopenconfig* Config, redisopenresult* Output)
{
	memset(Output, 0, sizeof(*Output));
	snprintf(Output->Backend, sizeof(Output->Backend), "hiredis_nonblocking");
	Output->HandlesBefore = redisOpenHandles();
	size_t Window = Config->Slots < 128u ? Config->Slots : 128u;
	size_t Pending[128];
	redisopenrecord* Records = calloc(Config->Count, sizeof(*Records));
	uint64* Samples = malloc(Config->Count * sizeof(*Samples));
	unsigned char* Payload = malloc(Config->Payload);
	redisOpenRequire(Records != NULL && Samples != NULL && Payload != NULL, "Hiredis observer allocation");
	Output->HarnessBytes = Config->Count * (sizeof(*Records) + sizeof(*Samples)) + Config->Payload + sizeof(Pending);
	redisContext* Client = redisOpenHiredisConnect(Config->Port);

	size_t Read = 0, Written = 0, Next = 0;
	int Flushed = 1;
	const uint64 Timeout = (uint64)Config->Timeout * 1000000u;
	double CpuStart = redisOpenCpu();
	uint64 Start = redisOpenNow();
	if (Config->Mode == 1) xrtSleep(Config->Stall);

	while (Next < Config->Count || Read != Written) {
		if (Read != Written) {
			redisOpenRequire(Flushed || redisBufferWrite(Client, &Flushed) == REDIS_OK, "Hiredis write");
			redisOpenRequire(redisBufferRead(Client) == REDIS_OK, "Hiredis read");
			for (;;) {
				void* Value = NULL;
				redisOpenRequire(redisGetReplyFromReader(Client, &Value) == REDIS_OK, "Hiredis parse");
				if (Value == NULL) break;
				uint64 Finished = redisOpenNow();
				redisOpenRequire(Read != Written, "unsolicited Hiredis reply");
				size_t Id = Pending[Read++ % Window];
				const redisReply* Reply = Value;
				bool Valid = Reply->type == REDIS_REPLY_STRING && Reply->len == Config->Payload;
				for (size_t Byte = 0; Valid && Byte < Config->Payload; ++Byte)
					Valid = (unsigned char)Reply->str[Byte] == redisOpenByte(Id, Byte);
				freeReplyObject(Value);
				redisOpenRequire(redisOpenFinish(&Records[Id], Finished, REDIS_OPEN_ACCEPTED, XREDIS_REQUEST_OK,
					Valid, Timeout, &Output->Counts), "Hiredis accepted ledger");
			}
			/* The reference connection cannot cancel a single outstanding
			 * command without breaking FIFO matching. Fail an undrained run. */
			redisOpenRequire(Read == Written || redisOpenNow() - Records[Pending[Read % Window]].Due < Timeout,
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
			else if (Written - Read == Window) Outcome = REDIS_OPEN_CAPACITY;
			else {
				for (size_t Byte = 0; Byte < Config->Payload; ++Byte) Payload[Byte] = redisOpenByte(Next, Byte);
				Record->Submitted = redisOpenNow();
				if (Record->Submitted - Record->Due >= Timeout) Outcome = REDIS_OPEN_EXPIRED;
				else {
					const char* Args[] = {"ECHO", (const char*)Payload};
					const size_t Sizes[] = {4, Config->Payload};
					redisOpenRequire(redisAppendCommandArgv(Client, 2, Args, Sizes) == REDIS_OK, "Hiredis append");
					Pending[Written++ % Window] = Next;
					Flushed = 0;
					if (Written - Read > Output->RequestsPeak) Output->RequestsPeak = Written - Read;
					size_t Buffered = sdslen(Client->obuf);
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
	redisOpenRequire(Flushed && Read == Output->Counts.Accepted, "Hiredis drain accounting");
	redisFree(Client);
	Output->Retire = (double)(redisOpenNow() - LogicalEnd) / (double)REDIS_OPEN_NS;
	Output->Cpu = redisOpenCpu() - CpuStart;
	redisOpenSummarize(Config, Output, Records, Samples);
	free(Payload);
	free(Samples);
	free(Records);
	Output->HandlesAfter = redisOpenHandles();
	return Output->Counts.Corrupt == 0 && (Config->Mode != 0 || Output->Counts.Successful != 0);
}
#endif
