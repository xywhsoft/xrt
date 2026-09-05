#include <xrt/http_decode.h>

#include <stdio.h>



/* 把解码片段直接写到标准输出，实际网络代码可改为业务消费回调。 */
static bool printBody(xbytesview Data, ptr pData)
{
	FILE* pOutput = (FILE*)pData;

	return fwrite(Data.Data, 1, Data.Size, pOutput) == Data.Size;
}



/*
 * 范例：http/decode —— 流式正文解码器：从 Content-Encoding 构建
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpDecodeCreate   由响应头字段建解码器（自动串接多级）
 *   xrtHttpDecodeWrite    喂入压缩片段，解出数据推给回调
 *   xrtHttpDecodeDone     校验收尾（gzip CRC/长度 trailer）
 * 模块宏：XRT_MODULE_HTTP_DECODE（依赖 COMPRESS）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/decode/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello compressed world
 *
 * 网络流式姿势：片段随到达随 Write（bFinal=false 表示
 *   还有后续），最后一段传 true；解出的明文直接推回调——
 *   全程不攒完整响应，内存占用与正文大小无关。
 *   gzip CRC 不匹配会在 Done 处失败（防静默损坏）。
 */


/* 展示从 HTTP/1 Header 建立流式 gzip 解码器。 */
int main(void)
{
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
		0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
		0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
		0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
		0x00, 0x00
	};
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	xhttpdecode* pDecode = xrtHttpDecodeCreate(Fields, 1, NULL);
	bool bSuccess;

	if ( pDecode == NULL ) {
		return 1;
	}
	bSuccess = xrtHttpDecodeWrite(
		pDecode,
		(xbytesview){ Gzip, sizeof(Gzip) },
		true,
		printBody,
		stdout
	) && xrtHttpDecodeDone(pDecode);
	xrtHttpDecodeDestroy(pDecode);
	return bSuccess ? 0 : 1;
}
