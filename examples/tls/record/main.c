#include <stdio.h>

#include <xrt.h>



/*
 * 范例：tls/record —— 记录层：明文记录编解码与套件元数据
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsRecordEncode/Parse   记录封包 / 借用解析（类型+版本+负载）
 *   xrtTlsRecordName           记录类型枚举 → 名字
 *   xrtTlsCipherInfo / CipherName / KeySize   套件元数据查询
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/record/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   type=application_data legacy=0x0303 payload=7 ${BS}
 *   cipher=TLS_AES_128_GCM_SHA256 key=16
 *
 * 记录层是 TLS 的"信封"：5 字节头（类型 + legacy 版本 + 长度）
 *   + 负载。握手完成前记录本身不加密——Encode/Parse 操作的
 *   就是这层中立结构，加解密在记录与消息层之间发生。
 *   自定义传输（内存回环、KCP）接 TLS 的挂载点也在这里。
 */


/* 编码并解析一条可由自定义可靠传输承载的 TLS 记录。 */
int main(void)
{
	uint8 Buffer[64];
	xtlsrecord Record;
	const xtlscipherinfo* pInfo;

	pInfo = xrtTlsCipherInfo(XTLS_AES_128_GCM_SHA256);
	if ( pInfo == NULL ) {
		return 1;
	}

	if ( !xrtTlsRecordEncode(
		XTLS_RECORD_APPLICATION_DATA,
		UINT16_C(0x0303),
		XRT_BYTES_LITERAL("example"),
		Buffer,
		sizeof(Buffer)
	) ) {
		return 1;
	}
	if ( xrtTlsRecordParse(
		(xbytesview) { Buffer, 12 }, &Record, NULL
	) != XTLS_OK ) {
		return 1;
	}
	printf(
		"type=%s legacy=0x%04x payload=%zu cipher=%s key=%u\n",
		xrtTlsRecordName(Record.Type),
		(unsigned)Record.LegacyVersion,
		Record.Payload.Size,
		xrtTlsCipherName(pInfo->Cipher),
		(unsigned)pInfo->KeySize
	);
	return 0;
}
