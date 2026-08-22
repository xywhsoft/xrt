#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xssh.h>



/* 示例使用固定 padding 以便复现输出；生产环境应传入 xrtSshSecurePadding。 */
static bool exampleAesGcmPadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	memset(pOutput, 0xa5, iSize);
	return true;
}



/* 展示独立读写状态上的 AES-GCM packet 往返。 */
int main(void)
{
	unsigned char arrKey[16] = { 0u };
	unsigned char arrIV[XSSH_AES_GCM_IV_SIZE] = { 0u };
	unsigned char arrWire[128];
	unsigned char arrPlain[64];
	xsshaesgcm WriteState;
	xsshaesgcm ReadState;
	xsshwriter Writer;
	xsshreader Reader;
	xsshpacketview Packet;

	if ( (xrtSshAesGcmInit(
		&WriteState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) != XSSH_OK) || (xrtSshAesGcmInit(
		&ReadState,
		(xbytesview){ arrKey, sizeof(arrKey) },
		(xbytesview){ arrIV, sizeof(arrIV) }
	) != XSSH_OK) || !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshAesGcmWrite(
			&Writer,
			XRT_BYTES_LITERAL("encrypted"),
			&WriteState,
			NULL,
			exampleAesGcmPadding,
			NULL
		) != XSSH_OK) || !xrtSshReaderInit(
			&Reader,
			(xbytesview){ arrWire, Writer.Size }
		) || (xrtSshAesGcmRead(
			&Reader,
			&ReadState,
			0u,
			NULL,
			&Packet,
			arrPlain,
			sizeof(arrPlain)
		) != XSSH_OK) ) {
		xrtSshAesGcmClear(&WriteState);
		xrtSshAesGcmClear(&ReadState);
		return 1;
	}
	printf("wire=%zu payload=%zu packet=%" PRIu32 "\n",
		Writer.Size, Packet.Payload.Size, Packet.PacketSize);
	xrtSshAesGcmClear(&WriteState);
	xrtSshAesGcmClear(&ReadState);
	return 0;
}
