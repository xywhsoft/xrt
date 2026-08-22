#include "../test.h"



/* 验证 FIFO、满队列、空回复与 ring 回绕。 */
static void testSshReplyQueueRing(void)
{
	uint64 arrTokens[3];
	xsshreplyqueue Queue;
	uint64 iToken = 99u;

	testRequire(xrtSshReplyQueueInit(&Queue, arrTokens, 3u) &&
		(xrtSshReplyQueueCount(&Queue) == 0u) &&
		(xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_ERROR_PROTOCOL) &&
		(iToken == 99u), "ssh empty reply queue mismatch");
	testRequire((xrtSshReplyQueuePush(&Queue, 10u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 20u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 30u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 40u) == XSSH_ERROR_SPACE) &&
		(xrtSshReplyQueueCount(&Queue) == 3u), "ssh full reply queue mismatch");
	testRequire((xrtSshReplyQueueFront(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 10u) && (xrtSshReplyQueueCount(&Queue) == 3u) &&
		(xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 10u) && (xrtSshReplyQueuePush(&Queue, 40u) == XSSH_OK) &&
		(xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 20u) && (xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 30u) && (xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 40u) && (xrtSshReplyQueueCount(&Queue) == 0u),
		"ssh reply queue FIFO or wrap mismatch");
}



/* 验证未完成请求迁移到更大调用方存储。 */
static void testSshReplyQueueRebind(void)
{
	uint64 arrSmall[3];
	uint64 arrLarge[257];
	xsshreplyqueue Queue;
	uint64 iToken;
	size_t i;

	testRequire(xrtSshReplyQueueInit(&Queue, arrSmall, 3u) &&
		(xrtSshReplyQueuePush(&Queue, 1u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 2u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 3u) == XSSH_OK) &&
		(xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 4u) == XSSH_OK) &&
		(xrtSshReplyQueueRebind(&Queue, arrLarge, 257u) == XSSH_OK) &&
		(Queue.Head == 0u) && (Queue.Count == 3u),
		"ssh reply queue rebind failed");
	for ( i = 5u; i <= 258u; ++i ) {
		testRequire(xrtSshReplyQueuePush(&Queue, (uint64)i) == XSSH_OK,
			"ssh expanded reply queue rejected token");
	}
	testRequire((xrtSshReplyQueueCount(&Queue) == 257u) &&
		(xrtSshReplyQueuePush(&Queue, 259u) == XSSH_ERROR_SPACE),
		"ssh expanded reply queue capacity mismatch");
	for ( i = 2u; i <= 258u; ++i ) {
		testRequire((xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
			(iToken == (uint64)i), "ssh expanded reply queue order mismatch");
	}
}



/* 验证长时间交错 push/pop 不破坏 FIFO 顺序或 ring 下标。 */
static void testSshReplyQueueStress(void)
{
	uint64 arrTokens[1024];
	xsshreplyqueue Queue;
	uint64 iToken;
	uint64 iWrite = 0u;
	uint64 iRead = 0u;
	size_t i;

	testRequire(xrtSshReplyQueueInit(
		&Queue,
		arrTokens,
		sizeof(arrTokens) / sizeof(arrTokens[0])
	), "ssh reply queue stress init failed");
	for ( i = 0u; i < 65536u; ++i ) {
		while ( xrtSshReplyQueueCount(&Queue) < 769u ) {
			testRequire(xrtSshReplyQueuePush(
				&Queue,
				iWrite
			) == XSSH_OK, "ssh reply queue stress push failed");
			iWrite++;
		}
		testRequire((xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
			(iToken == iRead), "ssh reply queue stress order mismatch");
		iRead++;
	}
	while ( xrtSshReplyQueueCount(&Queue) != 0u ) {
		testRequire((xrtSshReplyQueuePop(&Queue, &iToken) == XSSH_OK) &&
			(iToken == iRead), "ssh reply queue stress drain mismatch");
		iRead++;
	}
	testRequire(iRead == iWrite, "ssh reply queue stress count mismatch");
}



/* 验证零容量、对象重叠和缩容失败原子性。 */
static void testSshReplyQueueBoundaries(void)
{
	uint64 arrTokens[2];
	xsshreplyqueue Queue;
	uint64 iToken = 7u;

	testRequire(xrtSshReplyQueueInit(&Queue, NULL, 0u) &&
		(xrtSshReplyQueuePush(&Queue, 1u) == XSSH_ERROR_SPACE),
		"ssh zero-capacity reply queue mismatch");
	testRequire(xrtSshReplyQueueInit(&Queue, arrTokens, 2u) &&
		(xrtSshReplyQueuePush(&Queue, 1u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&Queue, 2u) == XSSH_OK) &&
		(xrtSshReplyQueueRebind(&Queue, NULL, 0u) ==
			XSSH_ERROR_ARGUMENT) && (Queue.Count == 2u) &&
		(xrtSshReplyQueueFront(&Queue, arrTokens) == XSSH_ERROR_ARGUMENT) &&
		(xrtSshReplyQueuePop(&Queue, arrTokens) == XSSH_ERROR_ARGUMENT) &&
		(Queue.Count == 2u) &&
		(xrtSshReplyQueueFront(&Queue, &iToken) == XSSH_OK) &&
		(iToken == 1u), "ssh reply queue boundary mismatch");
	testRequire(!xrtSshReplyQueueInit(
		(xsshreplyqueue*)arrTokens,
		arrTokens,
		2u
	), "ssh reply queue accepted overlapping object and storage");
}



/* 运行无固定容量 reply FIFO 测试。 */
int main(void)
{
	testSshReplyQueueRing();
	testSshReplyQueueRebind();
	testSshReplyQueueStress();
	testSshReplyQueueBoundaries();
	return 0;
}
