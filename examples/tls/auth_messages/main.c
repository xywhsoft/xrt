#include <stdio.h>

#include <xrt.h>



/*
 * 范例：tls/auth_messages —— TLS 1.2 ServerKeyExchange 编解码
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTls12ServerKeyExchangeSize    精确长度查询（两遍式写法）
 *   xrtTls12ServerKeyExchangeEncode  组 + 公钥 + 签名 → 正文
 *   xrtTls12ServerKeyExchangeParse   正文 → 借用视图
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/auth_messages/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   group=29 key=4 signature=3
 *
 * TLS 1.2 的 ECDHE 关键报文：服务端选组 + 公钥 + 对参数的
 *   签名。Size 先查再 Encode 是"两遍式"分配友好写法
 *   （先算长度再给精确缓冲）。Parse 出的视图直接用于
 *   签名验证与密钥派生（key_exchange 范例的 Derive）。
 */


/* 构造并解析一个 TLS 1.2 ECDHE ServerKeyExchange 正文。 */
int main(void)
{
	static const uint8 PublicKey[] = { 1, 2, 3, 4 };
	static const uint8 Signature[] = { 5, 6, 7 };
	xtlscertificateverify Verify;
	xtls12serverkeyexchange Exchange;
	uint8 Body[64];
	size_t iBodySize;

	Verify.Scheme = XTLS_SIGNATURE_ED25519;
	Verify.Signature = (xbytesview) { Signature, sizeof(Signature) };
	iBodySize = xrtTls12ServerKeyExchangeSize(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify
	);
	if ( (iBodySize == 0) || !xrtTls12ServerKeyExchangeEncode(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify,
		Body, sizeof(Body)
	) || !xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Body, iBodySize }, &Exchange
	) ) {
		return 1;
	}
	printf(
		"group=%u key=%zu signature=%zu\n",
		(unsigned)Exchange.Group, Exchange.PublicKey.Size,
		Exchange.Verify.Signature.Size
	);
	return 0;
}
