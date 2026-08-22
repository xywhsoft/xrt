#include <stdio.h>

#include <xrt.h>



/* 范例只统计运行时产生的线路负载。 */
static bool onCompressed(xbytesview Data, ptr pData)
{
	size_t* pSize = (size_t*)pData;

	*pSize += Data.Size;
	return true;
}



/* 流式压缩一条消息，并展示去除同步尾部后的长度。 */
int main(void)
{
	xwsdeflaterconfig Config;
	xwsdeflater* pDeflater;
	size_t iSize = 0;

	xrtWsDeflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pDeflater = xrtWsDeflaterCreate(&Config);
	if ( (pDeflater == NULL) ||
		!xrtWsDeflaterBegin(pDeflater, true) ||
		!xrtWsDeflaterWrite(
			pDeflater,
			XRT_BYTES_LITERAL("Hello permessage-deflate"),
			onCompressed,
			&iSize
		) ||
		!xrtWsDeflaterEnd(
			pDeflater,
			onCompressed,
			&iSize
		) ) {
		xrtWsDeflaterDestroy(pDeflater);
		return 1;
	}
	printf("wire bytes=%zu\n", iSize);
	xrtWsDeflaterDestroy(pDeflater);
	return 0;
}
