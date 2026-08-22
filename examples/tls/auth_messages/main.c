#include <stdio.h>

#include <xrt.h>



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
