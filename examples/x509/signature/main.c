#include <stdio.h>

#include <xrt.h>



/*
 * 范例：x509/signature —— AlgorithmIdentifier 解析（RSA-PSS 全参数）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509AlgorithmParse    独立 DER → 算法标识
 *   xrtX509SignatureParse    标识 → 规范化签名参数结构
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/signature/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   type=2 hash=1 mgf-hash=1 salt=20 trailer=1
 *
 * 读数：type=2（RSA-PSS）、hash=SHA-256 族枚举、
 *   MGF 掩码生成函数同哈希、盐长 20 字节、trailer 0xBC。
 *   OID（1.2.840.113549.1.1.10）已翻译为枚举——
 *   调用方拿到的是可直接驱动 crypto 模块的规范参数，
 *   不用维护自己的 OID 对照表。
 */


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
