#include <stdio.h>
#include <string.h>

#include <xrt.h>



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
