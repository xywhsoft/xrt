#include "../test.h"



/* 使用固定种子的轻量随机数，保证失败序列可复现。 */
static uint32 testNetBufRandom(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13;
	iValue ^= iValue >> 17;
	iValue ^= iValue << 5;
	*pState = iValue;
	return iValue;
}



/* 对照平坦字节数组验证缓冲链长序列状态。 */
int main(void)
{
	static uint8 arrModel[131072];
	uint8 arrInput[384];
	uint8 arrCheck[512];
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf Buffer;
	xnetbufpoolinfo Info;
	uint32 iRandom = 0xC001D00Du;
	size_t iModelSize = 0;
	size_t i;

	testRequire((pPool != NULL) && xrtNetBufInit(&Buffer, pPool),
		"buffer stress setup failed");
	for ( i = 0; i < 30000; i++ ) {
		uint32 iChoice = testNetBufRandom(&iRandom) % 6u;

		if ( (iModelSize > 120000) || (iChoice == 2) ) {
			size_t iConsume = iModelSize == 0 ? 0 :
				(size_t)(testNetBufRandom(&iRandom) % 512u);
			size_t iActual = iConsume < iModelSize ? iConsume : iModelSize;

			testRequire(xrtNetBufConsume(&Buffer, iConsume) == iActual,
				"buffer stress consume count mismatch");
			if ( iActual != 0 ) {
				memmove(arrModel, arrModel + iActual, iModelSize - iActual);
				iModelSize -= iActual;
			}
		} else if ( iChoice == 0 ) {
			size_t iSize = (size_t)(testNetBufRandom(&iRandom) % 256u);
			size_t j;

			for ( j = 0; j < iSize; j++ ) {
				arrInput[j] = (uint8)testNetBufRandom(&iRandom);
			}
			testRequire(xrtNetBufAppend(&Buffer, arrInput, iSize),
				"buffer stress append failed");
			memcpy(arrModel + iModelSize, arrInput, iSize);
			iModelSize += iSize;
		} else if ( iChoice == 1 ) {
			xnetwspan Write;
			size_t iMinimum = 1 +
				(size_t)(testNetBufRandom(&iRandom) % 384u);
			size_t iCommit;
			size_t j;

			testRequire(xrtNetBufReserve(&Buffer, iMinimum, &Write),
				"buffer stress reserve failed");
			iCommit = (size_t)(testNetBufRandom(&iRandom) %
				(uint32)(iMinimum + 1));
			for ( j = 0; j < iCommit; j++ ) {
				Write.Data[j] = (uint8)testNetBufRandom(&iRandom);
			}
			memcpy(arrModel + iModelSize, Write.Data, iCommit);
			if ( iCommit == 0 ) {
				testRequire(xrtNetBufCancel(&Buffer),
					"buffer stress cancel failed");
			} else {
				testRequire(xrtNetBufCommit(&Buffer, iCommit),
					"buffer stress commit failed");
				iModelSize += iCommit;
			}
		} else if ( iChoice == 3 ) {
			size_t iPrefix = iModelSize == 0 ? 0 :
				(size_t)(testNetBufRandom(&iRandom) %
				(uint32)(iModelSize + 1));
			xnetspan Span;

			testRequire(xrtNetBufPullup(&Buffer, iPrefix, &Span),
				"buffer stress pullup failed");
			testRequire((Span.Size == iPrefix) &&
				((iPrefix == 0) ||
				 (memcmp(Span.Data, arrModel, iPrefix) == 0)),
				"buffer stress pullup data mismatch");
		} else if ( iChoice == 4 ) {
			size_t iOffset = iModelSize == 0 ? 0 :
				(size_t)(testNetBufRandom(&iRandom) %
				(uint32)(iModelSize + 1));
			size_t iNeed = (size_t)(testNetBufRandom(&iRandom) % sizeof(arrCheck));
			size_t iExpected = iOffset < iModelSize ? iModelSize - iOffset : 0;
			size_t iRead;

			if ( iExpected > iNeed ) {
				iExpected = iNeed;
			}
			iRead = xrtNetBufPeek(&Buffer, iOffset, arrCheck, iNeed);
			testRequire((iRead == iExpected) &&
				((iRead == 0) ||
				 (memcmp(arrCheck, arrModel + iOffset, iRead) == 0)),
				"buffer stress peek mismatch");
		} else {
			uint8 iByte = (uint8)testNetBufRandom(&iRandom);
			size_t iOffset = iModelSize == 0 ? 0 :
				(size_t)(testNetBufRandom(&iRandom) %
				(uint32)(iModelSize + 1));
			cbytes pFound = iOffset < iModelSize ?
				(cbytes)memchr(arrModel + iOffset, iByte,
					iModelSize - iOffset) : NULL;
			size_t iExpected = pFound != NULL ?
				(size_t)(pFound - arrModel) : XRT_NPOS;

			testRequire(xrtNetBufFind(&Buffer, iByte, iOffset) == iExpected,
				"buffer stress find mismatch");
		}

		testRequire(xrtNetBufSize(&Buffer) == iModelSize,
			"buffer stress logical size drifted");
		if ( (i % 257u) == 0 ) {
			uint8* pSnapshot = (uint8*)malloc(iModelSize != 0 ? iModelSize : 1);

			testRequire(pSnapshot != NULL,
				"buffer stress snapshot allocation failed");
			testRequire(xrtNetBufPeek(&Buffer, 0, pSnapshot, iModelSize) == iModelSize,
				"buffer stress snapshot length mismatch");
			testRequire((iModelSize == 0) ||
				(memcmp(pSnapshot, arrModel, iModelSize) == 0),
				"buffer stress snapshot data mismatch");
			free(pSnapshot);
		}
	}

	xrtNetBufClear(&Buffer);
	xrtNetBufPoolGet(pPool, &Info);
	testRequire((Info.LiveBlocks == 0) && (Info.LiveBytes == 0),
		"buffer stress leaked live pool blocks");
	testRequire(xrtNetBufPoolDestroy(pPool),
		"buffer stress pool destroy failed");
	return 0;
}
