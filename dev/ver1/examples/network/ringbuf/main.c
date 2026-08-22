/*
 * XRT Example - Ring Buffer Style Receive Buffer
 * XRT 范例 - 环形缓冲式接收缓冲
 *
 * Description / 说明:
 *   EN: Demonstrates the current public buffering primitive xrtNetChain.
 *   CN: 演示当前公开的缓冲原语 xrtNetChain。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_ringbuf.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/network_ringbuf -lm -lpthread
 *
 * Note / 注意:
 *   - The historical plan mentioned xrtNetRingBuf, but current public API uses xrtNetChain.
 */
#include "../../../xrt.c"


int main()
{
	xnetchain tChain;
	char sPeek[32] = { 0 };

	xrtInit();

	xrtNetChainInit(&tChain);
	xrtNetChainAppendCopy(&tChain, "hello", 5);
	xrtNetChainAppendCopy(&tChain, "-world", 6);

	(void)xrtNetChainPeek(&tChain, sPeek, sizeof(sPeek) - 1u);
	printf("bytes_before_consume = %zu\n", xrtNetChainBytes(&tChain));
	printf("span_count = %u\n", (unsigned)xrtNetChainSpanCount(&tChain));
	printf("peek = %s\n", sPeek);

	xrtNetChainConsume(&tChain, 6);
	memset(sPeek, 0, sizeof(sPeek));
	(void)xrtNetChainPeek(&tChain, sPeek, sizeof(sPeek) - 1u);
	printf("bytes_after_consume = %zu\n", xrtNetChainBytes(&tChain));
	printf("peek_after_consume = %s\n", sPeek);

	xrtNetChainClear(&tChain);
	xrtUnit();
	return 0;
}
