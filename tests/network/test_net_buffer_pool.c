#include "../test.h"



/* Reserve 直接暴露池块，不再为每个连接固定常驻 8K 接收区。 */
static void testNetBufReserve(void)
{
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf Buffer;
	xnetwspan Write;
	xnetspan Read;

	testRequire(pPool != NULL, "default buffer pool create failed");
	testRequire(xrtNetBufInit(&Buffer, pPool), "reserve buffer init failed");
	testRequire(xrtNetBufReserve(&Buffer, 1, &Write),
		"small direct reservation failed");
	testRequire(Write.Size == 512,
		"small reservation retained a fixed 8K block");
	memcpy(Write.Data, "hello", 5);
	testRequire(xrtNetBufCommit(&Buffer, 5),
		"small direct reservation commit failed");
	testRequire(xrtNetBufFront(&Buffer, &Read) &&
		(Read.Size == 5) && (memcmp(Read.Data, "hello", 5) == 0),
		"direct reservation data mismatch");

	testRequire(xrtNetBufReserve(&Buffer, 600, &Write),
		"medium direct reservation failed");
	testRequire(Write.Size == 2048,
		"medium reservation chose the wrong size class");
	testRequire(xrtNetBufCancel(&Buffer),
		"direct reservation cancel failed");
	testRequire(xrtNetBufSize(&Buffer) == 5,
		"cancelled reservation changed logical contents");

	testRequire(xrtNetBufReserve(&Buffer, 4, &Write),
		"tail reuse reservation failed");
	testRequire(Write.Size == 507,
		"tail reservation did not reuse writable capacity");
	memcpy(Write.Data, "-x", 2);
	testRequire(xrtNetBufCommit(&Buffer, 2),
		"tail reservation commit failed");
	xrtNetBufClear(&Buffer);
	testRequire(xrtNetBufPoolDestroy(pPool),
		"default buffer pool destroy failed");
}



/* 池缓存必须有硬字节上限，并区分分配、复用和动态大块。 */
static void testNetBufPoolStats(void)
{
	xnetbufpoolconfig Config;
	xnetbufpoolinfo Info;
	xnetbufpool* pPool;
	xnetbuf Buffers[5];
	xnetwspan Span;
	static const size_t arrNeed[5] = { 1, 9, 17, 33, 100 };
	size_t i;

	xrtNetBufPoolConfigInit(&Config);
	Config.BlockSize[0] = 8;
	Config.BlockSize[1] = 16;
	Config.BlockSize[2] = 32;
	Config.BlockSize[3] = 64;
	Config.CacheLimit[0] = 4;
	Config.CacheLimit[1] = 4;
	Config.CacheLimit[2] = 4;
	Config.CacheLimit[3] = 4;
	Config.MaxCacheBytes = 40;
	pPool = xrtNetBufPoolCreate(&Config);
	testRequire(pPool != NULL, "custom buffer pool create failed");
	for ( i = 0; i < 5; i++ ) {
		testRequire(xrtNetBufInit(&Buffers[i], pPool),
			"custom pool buffer init failed");
		testRequire(xrtNetBufReserve(&Buffers[i], arrNeed[i], &Span),
			"custom pool reservation failed");
		Span.Data[0] = (uint8)i;
		testRequire(xrtNetBufCommit(&Buffers[i], 1),
			"custom pool commit failed");
	}
	xrtNetBufPoolGet(pPool, &Info);
	testRequire((Info.LiveBlocks == 5) && (Info.DynamicCount == 1),
		"custom pool live or dynamic statistics mismatch");
	for ( i = 0; i < 5; i++ ) {
		xrtNetBufClear(&Buffers[i]);
	}
	xrtNetBufPoolGet(pPool, &Info);
	testRequire((Info.LiveBlocks == 0) && (Info.LiveBytes == 0),
		"cleared buffers remain live in pool statistics");
	testRequire((Info.CachedBytes <= Config.MaxCacheBytes) &&
		(Info.CachedBlocks != 0),
		"custom pool cache budget was not enforced");

	testRequire(xrtNetBufInit(&Buffers[0], pPool) &&
		xrtNetBufReserve(&Buffers[0], 1, &Span) &&
		xrtNetBufCommit(&Buffers[0], 1),
		"custom pool reuse setup failed");
	xrtNetBufPoolGet(pPool, &Info);
	testRequire(Info.ReuseCount != 0,
		"custom pool did not count cached block reuse");
	xrtNetBufClear(&Buffers[0]);
	testRequire(xrtNetBufPoolTrim(pPool, 0) != 0,
		"custom pool trim did not release cached blocks");
	xrtNetBufPoolGet(pPool, &Info);
	testRequire((Info.CachedBlocks == 0) && (Info.CachedBytes == 0),
		"custom pool trim left cached blocks");
	testRequire(xrtNetBufPoolDestroy(pPool),
		"custom buffer pool destroy failed");
}



/* 缓冲池不能在块被移动或继续存活时提前销毁。 */
static void testNetBufPoolBusy(void)
{
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf Buffer;

	testRequire((pPool != NULL) && xrtNetBufInit(&Buffer, pPool),
		"busy pool setup failed");
	testRequire(xrtNetBufAppend(&Buffer, "live", 4),
		"busy pool live block append failed");
	testRequire(!xrtNetBufPoolDestroy(pPool),
		"busy buffer pool was destroyed");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_POOL_BUSY),
		"busy buffer pool error mismatch");
	xrtNetBufClear(&Buffer);
	testRequire(xrtNetBufPoolDestroy(pPool),
		"drained buffer pool destroy failed");
}



/* 执行缓冲池、直接接收和生命周期回归。 */
int main(void)
{
	testNetBufReserve();
	testNetBufPoolStats();
	testNetBufPoolBusy();
	return 0;
}
