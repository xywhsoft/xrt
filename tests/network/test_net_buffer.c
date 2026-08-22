#include "../test.h"



/* 外部引用释放过程必须恰好执行一次。 */
static void testNetBufRelease(ptr pContext, cbytes pData, size_t iSize)
{
	size_t* pCount = (size_t*)pContext;

	testRequire(pData != NULL, "buffer release received null data");
	testRequire(iSize != 0, "buffer release received zero size");
	(*pCount)++;
}



typedef struct testnetbufreentrant {
	xnetbuf* Buffer;
	size_t Released;
} testnetbufreentrant;



/* 最后一个引用块离链时，释放回调应能立即向同一缓冲追加新块。 */
static void testNetBufReentrantRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testnetbufreentrant* pTest = (testnetbufreentrant*)pContext;

	testRequire((pData != NULL) && (iSize == 3),
		"reentrant buffer release arguments mismatch");
	pTest->Released++;
	testRequire(xrtNetBufAppend(pTest->Buffer, "next", 4),
		"reentrant buffer append failed");
}



/* 继承旧版 Append、Peek、Find 和 Consume 的基础行为。 */
static void testNetBufBasics(void)
{
	xnetbuf Buffer;
	xnetspan Span = { (cbytes)"dirty", 5 };
	char sOutput[16] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL), "buffer init failed");
	testRequire(xrtNetBufEmpty(&Buffer) &&
		(xrtNetBufSize(&Buffer) == 0) &&
		(xrtNetBufSpanCount(&Buffer) == 0),
		"new buffer is not empty");
	testRequire(!xrtNetBufFront(&Buffer, &Span) &&
		(Span.Data == NULL) && (Span.Size == 0),
		"empty buffer published a front span");
	testRequire(xrtNetBufAppend(&Buffer, "hello", 5),
		"small buffer append failed");
	testRequire((xrtNetBufSize(&Buffer) == 5) &&
		(xrtNetBufFind(&Buffer, 'l', 0) == 2),
		"small buffer query mismatch");
	testRequire(xrtNetBufPeek(&Buffer, 0, sOutput, sizeof(sOutput)) == 5,
		"small buffer peek length mismatch");
	testRequire(memcmp(sOutput, "hello", 5) == 0,
		"small buffer peek data mismatch");
	testRequire(xrtNetBufConsume(&Buffer, 2) == 2,
		"small buffer consume count mismatch");
	memset(sOutput, 0, sizeof(sOutput));
	testRequire(xrtNetBufRead(&Buffer, sOutput, sizeof(sOutput)) == 3,
		"small buffer read count mismatch");
	testRequire(memcmp(sOutput, "llo", 3) == 0,
		"small buffer read data mismatch");
	testRequire(xrtNetBufEmpty(&Buffer),
		"fully read buffer is not empty");
	xrtNetBufClear(&Buffer);
}



/* 多 Span 查询和跨块读取必须保留旧 Chain 的协议解析能力。 */
static void testNetBufSpans(void)
{
	xnetbuf Buffer;
	xnetspan Spans[2];
	char aLeft[300];
	char aRight[50];
	char sOutput[8];

	memset(aLeft, 'A', sizeof(aLeft));
	memset(aRight, 'B', sizeof(aRight));
	testRequire(xrtNetBufInit(&Buffer, NULL), "span buffer init failed");
	testRequire(xrtNetBufAppendBorrow(&Buffer, aLeft, sizeof(aLeft)) &&
		xrtNetBufAppendBorrow(&Buffer, aRight, sizeof(aRight)),
		"borrowed span append failed");
	testRequire((xrtNetBufSize(&Buffer) == 350) &&
		(xrtNetBufSpanCount(&Buffer) == 2),
		"multi-span buffer shape mismatch");
	testRequire(xrtNetBufSpans(&Buffer, Spans, 2) == 2,
		"multi-span extraction failed");
	testRequire((Spans[0].Data == (cbytes)aLeft) &&
		(Spans[1].Data == (cbytes)aRight),
		"borrowed spans do not point to source data");
	testRequire(xrtNetBufFind(&Buffer, 'B', 0) == 300,
		"cross-span byte find mismatch");
	testRequire(xrtNetBufPeek(&Buffer, 296, sOutput, sizeof(sOutput)) == 8,
		"cross-span peek length mismatch");
	testRequire(memcmp(sOutput, "AAAABBBB", 8) == 0,
		"cross-span peek data mismatch");
	testRequire(xrtNetBufConsume(&Buffer, 300) == 300,
		"whole first span consume failed");
	testRequire(xrtNetBufFront(&Buffer, &Spans[0]) &&
		(Spans[0].Size == 50) && (Spans[0].Data[0] == 'B'),
		"front span after consume mismatch");
	testRequire(xrtNetBufConsume(&Buffer, 1000) == 50,
		"consume beyond end did not report actual bytes");
	xrtNetBufClear(&Buffer);
}



/* 前插必须保持原有借用块地址不变，并正确更新协议可见字节序列。 */
static void testNetBufPrepend(void)
{
	xnetbuf Buffer;
	xnetspan Spans[2];
	char sPayload[] = "payload";
	char sOutput[16] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL),
		"prepend buffer init failed");
	testRequire(xrtNetBufAppendBorrow(
		&Buffer,
		sPayload,
		strlen(sPayload)
	), "prepend payload append failed");
	testRequire(xrtNetBufPrepend(&Buffer, "head:", 5),
		"buffer prepend failed");
	testRequire((xrtNetBufSize(&Buffer) == 12) &&
		(xrtNetBufSpanCount(&Buffer) == 2),
		"prepend buffer shape mismatch");
	testRequire((xrtNetBufSpans(&Buffer, Spans, 2) == 2) &&
		(Spans[1].Data == (cbytes)sPayload),
		"prepend moved the borrowed payload");
	testRequire((xrtNetBufPeek(
		&Buffer,
		0,
		sOutput,
		sizeof(sOutput)
	) == 12) && (memcmp(sOutput, "head:payload", 12) == 0),
		"prepend contents mismatch");
	testRequire(xrtNetBufPrepend(&Buffer, NULL, 0),
		"empty prepend failed");
	xrtNetBufClear(&Buffer);
}



/* 外部引用在部分消费期间保持存活，最后离开链时释放一次。 */
static void testNetBufOwnership(void)
{
	xnetbuf Buffer;
	char sData[] = "ref-data";
	char sOutput[8] = { 0 };
	size_t iReleased = 0;
	str sTaken;

	testRequire(xrtNetBufInit(&Buffer, NULL), "ownership buffer init failed");
	testRequire(xrtNetBufAppendRef(&Buffer, sData, strlen(sData),
		testNetBufRelease, &iReleased), "reference append failed");
	testRequire(xrtNetBufConsume(&Buffer, 4) == 4,
		"reference partial consume failed");
	testRequire(iReleased == 0,
		"reference release ran before final consumption");
	testRequire(xrtNetBufPeek(&Buffer, 0, sOutput, sizeof(sOutput)) == 4,
		"reference suffix peek failed");
	testRequire(memcmp(sOutput, "data", 4) == 0,
		"reference suffix mismatch");
	xrtNetBufClear(&Buffer);
	testRequire(iReleased == 1,
		"reference release did not run exactly once");

	sTaken = (str)xrtMemDup("owned", 5);
	testRequire(sTaken != NULL, "taken buffer setup allocation failed");
	testRequire(xrtNetBufAppendTake(&Buffer, sTaken, 5),
		"taken buffer append failed");
	xrtNetBufClear(&Buffer);
}



/* 释放回调重入后，头尾、块数和逻辑内容必须已经处于稳定状态。 */
static void testNetBufReleaseAppend(void)
{
	xnetbuf Buffer;
	testnetbufreentrant Test;
	char sOutput[4] = { 0 };
	static const char sOld[] = "old";

	memset(&Test, 0, sizeof(Test));
	Test.Buffer = &Buffer;
	testRequire(xrtNetBufInit(&Buffer, NULL),
		"reentrant buffer init failed");
	testRequire(xrtNetBufAppendRef(
		&Buffer,
		sOld,
		3,
		testNetBufReentrantRelease,
		&Test
	), "reentrant reference append failed");
	testRequire(xrtNetBufConsume(&Buffer, 3) == 3,
		"reentrant reference consume failed");
	testRequire((Test.Released == 1) &&
		(xrtNetBufSize(&Buffer) == 4) &&
		(xrtNetBufSpanCount(&Buffer) == 1),
		"reentrant append left an invalid buffer shape");
	testRequire(xrtNetBufPeek(
		&Buffer,
		0,
		sOutput,
		sizeof(sOutput)
	) == sizeof(sOutput), "reentrant append peek failed");
	testRequire(memcmp(sOutput, "next", sizeof(sOutput)) == 0,
		"reentrant append contents mismatch");
	xrtNetBufClear(&Buffer);
}



/* Pullup 必须只复制分散前缀并保持完整逻辑内容。 */
static void testNetBufPullup(void)
{
	xnetbuf Buffer;
	xnetspan Span;
	char sLeft[] = "header-";
	char sRight[] = "value-body";
	char sAll[32] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL), "pullup buffer init failed");
	testRequire(xrtNetBufAppendBorrow(&Buffer, sLeft, strlen(sLeft)) &&
		xrtNetBufAppendBorrow(&Buffer, sRight, strlen(sRight)),
		"pullup setup append failed");
	testRequire(xrtNetBufPullup(&Buffer, 12, &Span),
		"cross-span pullup failed");
	testRequire((Span.Size == 12) &&
		(memcmp(Span.Data, "header-value", 12) == 0),
		"pullup prefix mismatch");
	testRequire(xrtNetBufPeek(&Buffer, 0, sAll, sizeof(sAll)) == 17,
		"pullup changed logical size");
	testRequire(memcmp(sAll, "header-value-body", 17) == 0,
		"pullup changed logical contents");
	xrtNetBufClear(&Buffer);
}



/* Move 必须转移块所有权而不复制，并保持源缓冲可继续使用。 */
static void testNetBufMove(void)
{
	xnetbuf Left;
	xnetbuf Right;
	xnetspan Before;
	xnetspan After;

	testRequire(xrtNetBufInit(&Left, NULL) && xrtNetBufInit(&Right, NULL),
		"move buffer init failed");
	testRequire(xrtNetBufAppend(&Left, "left", 4) &&
		xrtNetBufAppend(&Right, "right", 5),
		"move setup append failed");
	testRequire(xrtNetBufFront(&Right, &Before),
		"move source front query failed");
	testRequire(xrtNetBufMove(&Left, &Right),
		"buffer move failed");
	testRequire((xrtNetBufSize(&Left) == 9) && xrtNetBufEmpty(&Right),
		"buffer move sizes mismatch");
	testRequire(xrtNetBufSpans(&Left, &After, 1) == 1,
		"moved buffer span query failed");
	testRequire(xrtNetBufAppend(&Right, "new", 3),
		"moved-from buffer cannot be reused");
	(void)Before;
	(void)After;
	xrtNetBufClear(&Left);
	xrtNetBufClear(&Right);
}



/* 活动尾部预留期间可以消费已提交前缀，但其他结构修改仍必须被拒绝。 */
static void testNetBufConsumeReservedTail(void)
{
	xnetbuf Buffer;
	xnetwspan Write;
	xnetspan Read = { (cbytes)"dirty", 5 };
	char sOutput[8] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL),
		"reserved-tail buffer init failed");
	testRequire(xrtNetBufAppend(&Buffer, "old", 3),
		"reserved-tail prefix append failed");
	testRequire(xrtNetBufReserve(&Buffer, 3, &Write),
		"existing tail reservation failed");
	testRequire(Write.Size >= 3,
		"existing tail reservation is too small");
	testRequire(xrtNetBufRead(&Buffer, sOutput, sizeof(sOutput)) == 3,
		"committed prefix read during reservation failed");
	testRequire((memcmp(sOutput, "old", 3) == 0) && xrtNetBufEmpty(&Buffer),
		"committed prefix changed during reservation");
	testRequire((xrtNetBufSpanCount(&Buffer) == 0) &&
		!xrtNetBufFront(&Buffer, &Read) &&
		(Read.Data == NULL) && (Read.Size == 0),
		"consumed reserved tail published an empty span");
	testRequire(!xrtNetBufAppend(&Buffer, "x", 1),
		"append unexpectedly changed a buffer with an active reservation");
	memcpy(Write.Data, "new", 3);
	testRequire(xrtNetBufCommit(&Buffer, 3),
		"existing tail reservation commit failed after prefix consumption");
	memset(sOutput, 0, sizeof(sOutput));
	testRequire(xrtNetBufRead(&Buffer, sOutput, sizeof(sOutput)) == 3,
		"committed reservation read failed");
	testRequire(memcmp(sOutput, "new", 3) == 0,
		"committed reservation contents mismatch");
	xrtNetBufClear(&Buffer);
}



/* 独立新尾块在提交前不属于可读链，旧前缀可被完整消费后再接入新数据。 */
static void testNetBufConsumeBeforeNewTailCommit(void)
{
	xnetbuf Buffer;
	xnetwspan Write;
	char sPrefix[] = "prefix";
	char sOutput[8] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL),
		"new-tail buffer init failed");
	testRequire(xrtNetBufAppendBorrow(&Buffer, sPrefix, strlen(sPrefix)),
		"new-tail borrowed prefix append failed");
	testRequire(xrtNetBufReserve(&Buffer, 4, &Write),
		"new tail reservation failed");
	testRequire(Write.Size >= 4,
		"new tail reservation is too small");
	testRequire(xrtNetBufConsume(&Buffer, strlen(sPrefix)) == strlen(sPrefix),
		"borrowed prefix consume during new tail reservation failed");
	testRequire(xrtNetBufEmpty(&Buffer),
		"uncommitted new tail became readable");
	memcpy(Write.Data, "tail", 4);
	testRequire(xrtNetBufCommit(&Buffer, 4),
		"new tail commit failed after prefix consumption");
	testRequire(xrtNetBufRead(&Buffer, sOutput, sizeof(sOutput)) == 4,
		"newly linked tail read failed");
	testRequire(memcmp(sOutput, "tail", 4) == 0,
		"newly linked tail contents mismatch");
	xrtNetBufClear(&Buffer);
}



/* 执行网络缓冲链基础回归。 */
int main(void)
{
	testNetBufBasics();
	testNetBufSpans();
	testNetBufPrepend();
	testNetBufOwnership();
	testNetBufReleaseAppend();
	testNetBufPullup();
	testNetBufMove();
	testNetBufConsumeReservedTail();
	testNetBufConsumeBeforeNewTailCommit();
	return 0;
}
