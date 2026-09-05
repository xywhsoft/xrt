#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：x509/crl_policy —— 已验证 CRL 的轻量查询路径
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtX509CrlCheck   查询证书序列号是否被撤销
 *   xx509revocation   结果：State（REVOKED/OK/UNKNOWN）等
 * 模块宏：XRT_MODULE_X509
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/x509/crl_policy/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   revocation state: 1
 *
 * 两层设计：CrlValidate（验签 + 时间窗 + 颁发者匹配）重、
 *   产出 xx509crlvalid；CrlCheck 拿 Valid 轻查多张证书——
 *   验一次查 N 次。本例手工构造 Valid 聚焦查询语义：
 *   序列号 0x2A 在列表中 → State=1（X509_REVOCATION_REVOKED）。
 */


/* 展示复用已验证 CRL 视图查询多张证书的轻量路径。 */
int main(void)
{
	static const uint8 Name[] = {
		0x30, 0x0E, 0x31, 0x0C, 0x30, 0x0A, 0x06, 0x03, 0x55, 0x04,
		0x03, 0x0C, 0x03, 0x43, 0x41, 0x31
	};
	static const uint8 Entries[] = {
		0x30, 0x14, 0x30, 0x12, 0x02, 0x01, 0x2A, 0x17, 0x0D, 0x32,
		0x36, 0x30, 0x34, 0x30, 0x38, 0x30, 0x30, 0x30, 0x30, 0x30,
		0x30, 0x5A
	};
	static const uint8 Serial[] = { 0x2A };
	xx509cert Issuer;
	xx509cert Certificate;
	xx509crl Crl;
	xx509crlvalid Valid;
	xx509revocation Revocation;
	xx509result Result;

	/* 实际程序应由 xrtX509CrlValidate 产生 Valid；这里聚焦查询接口。 */
	memset(&Issuer, 0, sizeof(Issuer));
	memset(&Crl, 0, sizeof(Crl));
	memset(&Valid, 0, sizeof(Valid));
	Issuer.Subject = (xbytesview) { Name, sizeof(Name) };
	Crl.Issuer = Issuer.Subject;
	Crl.Version = X509_CRL_VERSION_1;
	Crl.Revoked = (xbytesview) { Entries, sizeof(Entries) };
	Valid.Crl = &Crl;
	Valid.Issuer = &Issuer;

	memset(&Certificate, 0, sizeof(Certificate));
	Certificate.Version = X509_VERSION_3;
	Certificate.Issuer = Issuer.Subject;
	Certificate.Serial = (xbytesview) { Serial, sizeof(Serial) };
	Result = xrtX509CrlCheck(&Valid, &Certificate, &Revocation);
	if ( Result != X509_VALUE ) {
		return 1;
	}
	printf("revocation state: %d\n", (int)Revocation.State);
	return Revocation.State == X509_REVOCATION_REVOKED ? 0 : 1;
}
