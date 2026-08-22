#include <stdio.h>
#include <xrt.h>



/* 展示原子计数和无歧义的比较交换。 */
int main(void)
{
	xatomic64 Counter = XRT_ATOMIC64_INIT(0u);
	uint64 iExpected = 1u;

	(void)xrtAtomic64FetchAdd(&Counter, 1u, XMEMORY_RELAXED);
	if (
		xrtAtomic64CompareExchange(
			&Counter,
			&iExpected,
			10u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		)
	) {
		printf("counter=%llu\n", (unsigned long long)xrtAtomic64Load(&Counter, XMEMORY_RELAXED));
	}
	return 0;
}
