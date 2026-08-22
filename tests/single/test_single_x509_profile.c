#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_profile_vectors.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 X.509 profile 类型化扩展。 */
int main(void)
{
	static const uint8 AuthorityKeyId[] = {
		0x30, 0x05, 0x80, 0x03, 0x01, 0x02, 0x03
	};
	static const uint8 IssuerAltName[] = {
		0x30, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x1D, 0x12, 0x04,
		0x09, 0x30, 0x07, 0x86, 0x05, 0x63, 0x61, 0x3A, 0x2F, 0x2F
	};
	xx509cert Cert;
	xx509gencursor Names;
	xx509genname Name;
	xx509basicconstraints Constraints;
	xx509authoritykeyid Authority;
	uint16 iUsage;

	if ( !xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	) || (xrtX509SubjectAltName(&Cert, &Names) != X509_VALUE) ||
		(xrtX509GeneralNameRead(&Names, &Name) != X509_VALUE) ||
		(Name.Type != X509_NAME_DNS) ||
		(xrtX509KeyUsage(&Cert, &iUsage) != X509_VALUE) ||
		(iUsage != (X509_USAGE_DIGITAL_SIGNATURE |
		 X509_USAGE_KEY_ENCIPHERMENT)) ||
		(xrtX509BasicConstraints(&Cert, &Constraints) != X509_VALUE) ||
		!Constraints.CA || !Constraints.HasPathLimit ||
		(Constraints.PathLimit != 2u) ||
		!xrtX509AuthorityKeyIdParse(
			(xbytesview) { AuthorityKeyId, sizeof(AuthorityKeyId) }, &Authority
		) || !Authority.HasKeyId || Authority.HasIssuer || Authority.HasSerial ) {
		return 1;
	}

	memset(&Cert, 0, sizeof(Cert));
	Cert.Extensions = (xbytesview) {
		IssuerAltName, sizeof(IssuerAltName)
	};
	if ( (xrtX509IssuerAltName(&Cert, &Names) != X509_VALUE) ||
		(xrtX509GeneralNameRead(&Names, &Name) != X509_VALUE) ||
		(Name.Type != X509_NAME_URI) || (Name.Value.Size != 5u) ||
		(memcmp(Name.Value.Data, "ca://", 5u) != 0) ) {
		return 1;
	}
	printf("[PASS] single-x509-profile\n");
	return 0;
}
