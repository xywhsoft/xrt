#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 TLS 1.2 ECDHE 认证消息写入。 */
int main(void)
{
	static const uint8 Key[] = { 1, 2, 3 };
	static const uint8 Signature[] = { 4, 5 };
	xtlscertificateverify Verify;
	xtls12serverkeyexchange Exchange;
	uint8 Body[32];
	size_t iSize;

	Verify.Scheme = XTLS_SIGNATURE_ED25519;
	Verify.Signature = (xbytesview) { Signature, sizeof(Signature) };
	iSize = xrtTls12ServerKeyExchangeSize(
		XTLS_GROUP_X25519, (xbytesview) { Key, sizeof(Key) }, &Verify
	);
	return (iSize != 0) && xrtTls12ServerKeyExchangeEncode(
		XTLS_GROUP_X25519, (xbytesview) { Key, sizeof(Key) }, &Verify,
		Body, sizeof(Body)
	) && xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Body, iSize }, &Exchange
	) && (Exchange.PublicKey.Size == sizeof(Key)) ? 0 : 1;
}
