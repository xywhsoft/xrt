#include <xrt.h>

#include "../../../tests/fixtures/x509_legacy_cert.h"

#include <stdio.h>



/*
 * 范例：x509/verify —— 证书签名独立验证（自签名证书自查）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509CertificateVerify   用签发者证书的公钥验证签名
 * 模块宏：XRT_MODULE_X509（依赖 CRYPTO）
 * 编译（单头形态，Windows，仓库根目录）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c ${BS}
 *       examples/x509/verify/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   certificate signature is valid
 *
 * CertificateVerify vs PathValidate 的分工：
 *   前者只回答"这个签名在数学上是否成立"——参数为
 *   (待验证证书, 签发者证书)；本例两者都是同一张自签名证书
 *   （自己给自己签，公钥自带）。
 *   "是否信任这个签发者"是链验证（path 范例）的职责。
 *   根证书健康检查、CA 材料审计用独立 Verify。
 */


/* 解析并验证从旧版示例继承的真实自签名 RSA 证书。 */
int main(void)
{
	xx509cert Certificate;

	if ( !xrtX509Parse(
		X509_LEGACY_RSA_CERT,
		sizeof(X509_LEGACY_RSA_CERT),
		&Certificate
	) || !xrtX509CertificateVerify(
		&Certificate, &Certificate
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate signature is valid\n");
	return 0;
}
