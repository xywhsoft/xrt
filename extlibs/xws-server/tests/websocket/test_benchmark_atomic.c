#include "../../../../dev/bench/bench_common.h"
#include "../test.h"

#define COUNTER_THREADS 4u
#define COUNTER_ROUNDS 25000u
#define PUBLICATION_ROUNDS 5000u

typedef struct benchmark_atomic_state {
    volatile long Go, Count, Maximum, Winners, Published, Acknowledged;
    volatile uint64_t Once;
    uint64_t Payload;
    uint64_t Deadline;
} benchmark_atomic_state;

typedef struct benchmark_atomic_worker {
    benchmark_atomic_state* State;
    unsigned Index;
} benchmark_atomic_worker;

static void wait_value(volatile long* value, long expected, uint64_t deadline)
{
    while (xbenchAtomicLoad(value) != expected) {
        testRequire(!xbenchDeadlineReached(deadline), "benchmark atomic publication timed out");
        xrtThreadYield();
    }
}

static int32 count_worker(ptr data)
{
    benchmark_atomic_worker* worker = (benchmark_atomic_worker*)data;
    benchmark_atomic_state* state = worker->State;
    wait_value(&state->Go, 1, state->Deadline);
    if (xbenchAtomicSetOnceU64(&state->Once, ((uint64_t)worker->Index << 32) | 1u)) {
        (void)xbenchAtomicInc(&state->Winners);
    }
    for (unsigned i = 0; i < COUNTER_ROUNDS; i++) {
        (void)xbenchAtomicAdd(&state->Count, 1);
        (void)xbenchAtomicMax(&state->Maximum, (long)(i * COUNTER_THREADS + worker->Index));
    }
    return 0;
}

static int32 publish_worker(ptr data)
{
    benchmark_atomic_state* state = (benchmark_atomic_state*)data;
    for (long i = 1; i <= (long)PUBLICATION_ROUNDS; i++) {
        wait_value(&state->Acknowledged, i - 1, state->Deadline);
        state->Payload = (uint64_t)i * UINT64_C(0x100000001);
        xbenchAtomicStore(&state->Published, i);
    }
    return 0;
}

static int32 consume_worker(ptr data)
{
    benchmark_atomic_state* state = (benchmark_atomic_state*)data;
    for (long i = 1; i <= (long)PUBLICATION_ROUNDS; i++) {
        wait_value(&state->Published, i, state->Deadline);
        testRequire(state->Payload == (uint64_t)i * UINT64_C(0x100000001),
            "benchmark atomic publication exposed stale payload");
        xbenchAtomicStore(&state->Acknowledged, i);
    }
    return 0;
}

static void join_worker(xthread* thread)
{
    testRequire(thread != NULL && xrtThreadWaitFor(thread, 10000) == XWAIT_OK &&
        xrtThreadExitCode(thread) == 0, "benchmark atomic Worker did not finish");
    xrtThreadDestroy(thread);
}

int main(void)
{
    benchmark_atomic_state state;
    benchmark_atomic_worker workers[COUNTER_THREADS];
    xthread* threads[COUNTER_THREADS];
    xthread* producer;
    xthread* consumer;
    volatile long signed_value = -4;
    volatile uint64_t wide = 0;

    testRequire(xbenchAtomicAdd(&signed_value, 2) == -2 &&
        xbenchAtomicMax(&signed_value, -3) == -2 &&
        xbenchAtomicMax(&signed_value, 7) == 7, "benchmark signed atomic mismatch");
    xbenchAtomicStore(&signed_value, -9);
    testRequire(xbenchAtomicLoad(&signed_value) == -9 &&
        !xbenchAtomicSetOnceU64(NULL, 1) && !xbenchAtomicSetOnceU64(&wide, 0) &&
        xbenchAtomicSetOnceU64(&wide, UINT64_C(0x8000000000000001)) &&
        !xbenchAtomicSetOnceU64(&wide, 2) &&
        xbenchAtomicLoadU64(&wide) == UINT64_C(0x8000000000000001),
        "benchmark 64-bit atomic mismatch");

    memset(&state, 0, sizeof(state));
    state.Maximum = -1;
    /* Initialize the Windows QPC frequency before publishing to threads. */
    state.Deadline = xbenchDeadlineAfterMs(10000);
    for (unsigned i = 0; i < COUNTER_THREADS; i++) {
        workers[i].State = &state;
        workers[i].Index = i;
        threads[i] = xrtThreadCreate(count_worker, &workers[i], 0);
        testRequire(threads[i] != NULL, "benchmark atomic Worker creation failed");
    }
    xbenchAtomicStore(&state.Go, 1);
    for (unsigned i = 0; i < COUNTER_THREADS; i++) join_worker(threads[i]);
    testRequire(xbenchAtomicLoad(&state.Count) == (long)(COUNTER_THREADS * COUNTER_ROUNDS) &&
        xbenchAtomicLoad(&state.Maximum) == (long)(COUNTER_THREADS * COUNTER_ROUNDS) - 1 &&
        xbenchAtomicLoad(&state.Winners) == 1 &&
        (xbenchAtomicLoadU64(&state.Once) & UINT64_C(0xffffffff)) == 1 &&
        (xbenchAtomicLoadU64(&state.Once) >> 32) < COUNTER_THREADS,
        "benchmark atomics lost updates or selected multiple winners");

    state.Deadline = xbenchDeadlineAfterMs(10000);
    producer = xrtThreadCreate(publish_worker, &state, 0);
    consumer = xrtThreadCreate(consume_worker, &state, 0);
    join_worker(producer);
    join_worker(consumer);
    xrtClearError();
    testMemoryDebugDrain("benchmark atomic Workers leaked resources");
    printf("[PASS] benchmark atomic contention and payload publication\n");
    return 0;
}
