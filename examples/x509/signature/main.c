#include <stdio.h>

#include <xrt.h>



/* 演示从独立 AlgorithmIdentifier 读取完整 RSA-PSS 参数。 */
int main(void)
{
	static const uint8 AlgorithmDer[] = {
		0x30, 0x0D,
		0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
		0xF7, 0x0D, 0x01, 0x01, 0x0A,
		0x30, 0x00
	};
	xx509algorithm Algorithm;
	xx509signature Signature;

	if ( !xrtX509AlgorithmParse(
		(xbytesview) { AlgorithmDer, sizeof(AlgorithmDer) }, &Algorithm
	) || (xrtX509SignatureParse(
		&Algorithm, &Signature
	) != X509_VALUE) ) {
		return 1;
	}
	printf(
		"type=%d hash=%d mgf-hash=%d salt=%zu trailer=%u\n",
		(int)Signature.Type, (int)Signature.Hash,
		(int)Signature.MaskHash, Signature.SaltSize,
		(unsigned)Signature.Trailer
	);
	return 0;
}
