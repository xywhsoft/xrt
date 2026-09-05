/*
 * 范例：hash/variants —— 哈希补遗：64Seed / SipHash 一次性
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHash64Seed   显式种子的 64 位哈希（对应 32 位版 hash32 范例）
 *   xrtSipHash      SipHash-2-4 一次性（流式版见 keyed 范例）
 * 模块宏：XRT_MODULE_HASH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/hash/variants/main.c -lws2_32 -liphlpapi
 * 预期输出（值由算法决定，跨平台稳定）：
 *   seed64: 1E3C0D5C77AC4C64
 *   sip:    21D9F0C356D09D0A
 *
 * SipHash 一次性与流式等价：同样的密钥与数据，
 *   两种姿势必须得到同一结果（本例打印验证）。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	xsipkey Key = xrtSipKey(UINT64_C(0x0123456789ABCDEF),
		UINT64_C(0xFEDCBA9876543210));
	uint64 iStreamed = 0;

	/* 显式种子：跨进程一致分片、防预计算。 */
	printf("seed64: %016llX\n",
		(unsigned long long)xrtHash64Seed("user:42", 7u, UINT64_C(0x1234)));

	/* 一次性 SipHash；与 keyed 范例的 Init/Update/Final 对拍。 */
	printf("sip:    %016llX\n",
		(unsigned long long)xrtSipHash("request:user-input", 18u, Key));

	/* 对拍：流式喂同样数据（分两块）结果一致。 */
	{
		xsiphash State;

		xrtSipHashInit(&State, Key);
		(void)xrtSipHashUpdate(&State, "request:", 8u);
		(void)xrtSipHashUpdate(&State, "user-input", 10u);
		iStreamed = xrtSipHashFinal(&State);
	}
	printf("stream-equal=%s\n",
		iStreamed == xrtSipHash("request:user-input", 18u, Key) ? "yes" : "no");
	return 0;
}
