/*
 * 范例：core/atomic —— 64 位原子计数与无歧义的比较交换
 * ----------------------------------------------------------------
 * 演示 API：
 *   XRT_ATOMIC64_INIT        编译期初始化原子变量
 *   xrtAtomic64FetchAdd      原子加法并返回旧值
 *   xrtAtomic64CompareExchange  比较交换（CAS），失败时回写观察到的当前值
 *   xrtAtomic64Load          原子读取
 * 模块宏：XRT_MODULE_ATOMIC（地基模块，XRT_MODULE_ALL 默认包含）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -I single impl.c examples/core/atomic/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   counter=10
 *
 * 内存序说明：
 *   RELAXED  只保证单变量原子性，不建立跨变量顺序（计数器足够）；
 *   ACQ_REL  读-改-写同时具备获取与释放语义（CAS 常用）；
 *   ACQUIRE  失败路径的加载序（CAS 失败时 iExpected 被回写，读取需获得语义）。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	/* 原子变量必须用 INIT 宏静态初始化，保证所有平台起始状态一致。 */
	xatomic64 Counter = XRT_ATOMIC64_INIT(0u);

	/*
	 * CAS 的"期望值"通过指针传入：
	 *   成功 —— *pExpected 保持不变，NewValue 被写入；
	 *   失败 —— *pExpected 被回写为实际观察到的当前值，
	 *           调用方可直接用它重试，无需再 Load 一次。
	 * 这里期望值设为 1，与 FetchAdd 之后的实际值一致，CAS 将成功。
	 */
	uint64 iExpected = 1u;

	/* 原子自增：返回旧值 0（丢弃），计数器变为 1；纯计数用 RELAXED 即可。 */
	(void)xrtAtomic64FetchAdd(&Counter, 1u, XMEMORY_RELAXED);

	/*
	 * 比较-交换：当前值 == 1 时整体替换为 10。
	 * 成功序 ACQ_REL：本次写入对后续获取者可见；
	 * 失败序 ACQUIRE：失败时 iExpected 被回写，读它需要获得语义。
	 */
	if (
		xrtAtomic64CompareExchange(
			&Counter,
			&iExpected,
			10u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		)
	) {
		/* 读取最终值 10；没有其他生产者在发布数据，RELAXED 足够。 */
		printf("counter=%llu\n", (unsigned long long)xrtAtomic64Load(&Counter, XMEMORY_RELAXED));
	}
	return 0;
}
