#ifndef XREDIS_BENCH_OPENLOOP_SCHEDULE_H
#define XREDIS_BENCH_OPENLOOP_SCHEDULE_H
#include <xredis.h>
#include <limits.h>

/* Benchmark-only accounting. No library ABI or production dependency. Times
 * are absolute monotonic nanoseconds, independent of actual issue/completion. */
#define REDIS_OPEN_NS 1000000000ull
#define REDIS_OPEN_MAX_COUNT 1000000u
#define REDIS_OPEN_MAX_SLOTS 4096u
#define REDIS_OPEN_MAX_PAYLOAD (256u * 1024u)
typedef enum redisopenoutcome {
	REDIS_OPEN_PENDING = 0, REDIS_OPEN_ACCEPTED, REDIS_OPEN_CAPACITY,
	REDIS_OPEN_OVERLOADED, REDIS_OPEN_CLOSED, REDIS_OPEN_ERROR, REDIS_OPEN_EXPIRED,
	REDIS_OPEN_OUTCOMES
} redisopenoutcome;
typedef struct redisopenconfig {
	size_t Count, Slots, Payload;
	uint64 Rate;
	int64 Timeout;
	unsigned Mode; /* 0 normal, 1 initial generator stall, 2 fixture holds ECHO. */
	uint32 Stall, Port;
	bool Stats;
} redisopenconfig;
typedef struct redisopenrecord {
	uint64 Due, Submitted, Finished;
	redisopenoutcome Outcome;
	xredisrequeststatus Status;
	bool Valid;
} redisopenrecord;
typedef struct redisopencounts {
	size_t Total, Accepted, Successful, Corrupt, Missed;
	size_t Outcomes[REDIS_OPEN_OUTCOMES];
	size_t Status[XREDIS_REQUEST_INITIALIZATION + 1u];
} redisopencounts;

static inline redisopenconfig redisOpenDefaults(void)
{ return (redisopenconfig){100000, 256, 32, 100000, 1000, 0, 0, 0, false}; }

static inline bool redisOpenConfigValid(const redisopenconfig* Config)
{
	return Config != NULL && Config->Count != 0 && Config->Count <= REDIS_OPEN_MAX_COUNT &&
		Config->Rate != 0 && Config->Rate <= REDIS_OPEN_NS && Config->Slots != 0 && Config->Slots <= REDIS_OPEN_MAX_SLOTS &&
		Config->Payload >= 8 && Config->Payload <= REDIS_OPEN_MAX_PAYLOAD && Config->Payload <= (64u * 1024u * 1024u) / Config->Slots &&
		Config->Timeout > 0 && Config->Timeout <= 10000 && Config->Mode <= 2 && Config->Stall <= 5000 && Config->Port <= UINT16_MAX &&
		(Config->Mode == 1 || Config->Stall == 0) && (Config->Mode != 2 || (Config->Port == 0 &&
		 Config->Payload + 64u <= 65536u / Config->Slots)) &&
		(Config->Count - 1u) / Config->Rate <= 30 &&
		((Config->Count - 1u) / Config->Rate != 30 || (Config->Count - 1u) % Config->Rate == 0);
}

static inline bool redisOpenDue(uint64 Start, size_t Index, uint64 Rate, uint64* Due)
{
	if (Rate == 0 || Rate > REDIS_OPEN_NS || Index >= REDIS_OPEN_MAX_COUNT || Due == NULL) return false;
	uint64 Delta = ((uint64)Index / Rate) * REDIS_OPEN_NS + (((uint64)Index % Rate) * REDIS_OPEN_NS) / Rate;
	if (Start > UINT64_MAX - Delta) return false;
	*Due = Start + Delta; return true;
}

static inline size_t redisOpenQuantile(size_t Count, unsigned Thousandths)
{
	if (Count == 0 || Thousandths == 0 || Thousandths > 1000) return SIZE_MAX;
	/* ceil(N * p / 1000) - 1 without overflowing a size_t product. */
	return (Count / 1000u) * Thousandths + ((Count % 1000u) * Thousandths + 999u) / 1000u - 1u;
}

static inline bool redisOpenFinish(redisopenrecord* Record, uint64 Finished,
	redisopenoutcome Outcome, xredisrequeststatus Status, bool Valid, uint64 Timeout,
	redisopencounts* Counts)
{
	if (Record == NULL || Counts == NULL || Record->Finished != 0 || Finished == 0 ||
		Finished < Record->Due || Record->Submitted < Record->Due || Record->Submitted > Finished ||
		Outcome <= REDIS_OPEN_PENDING || Outcome >= REDIS_OPEN_OUTCOMES ||
		(Outcome == REDIS_OPEN_ACCEPTED && (unsigned)Status > XREDIS_REQUEST_INITIALIZATION)) return false;
	Record->Finished = Finished; Record->Outcome = Outcome; Record->Status = Status; Record->Valid = Valid;
	++Counts->Total; ++Counts->Outcomes[Outcome];
	if (Finished - Record->Due >= Timeout) ++Counts->Missed;
	if (Outcome == REDIS_OPEN_ACCEPTED) {
		++Counts->Accepted; ++Counts->Status[Status];
		if (!Valid) ++Counts->Corrupt;
		else if (Status == XREDIS_REQUEST_OK) ++Counts->Successful;
	}
	return true;
}

static inline unsigned char redisOpenByte(size_t Id, size_t Offset)
{
	if (Offset < 8) return (unsigned char)((uint64)Id >> (Offset * 8u));
	return (unsigned char)((uint64)Id * 17u + (Id >> 8u) + Offset * 131u);
}
#endif
