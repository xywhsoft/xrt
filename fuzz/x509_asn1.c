#include <stdlib.h>

#include <xrt/asn1.h>
#include <xrt/error.h>
#include <xrt/x509.h>



#define XRT_X509_FUZZ_INPUT_MAX ((size_t)1048576u)



/* 判断借用字节视图是否完整位于原始 DER 输入中。 */
static bool __xrtX509FuzzViewInside(
	xbytesview Input,
	xbytesview View
)
{
	uintptr_t iInput;
	uintptr_t iView;

	if ( View.Data == NULL ) {
		return View.Size == 0;
	}
	if ( Input.Data == NULL ) {
		return false;
	}
	iInput = (uintptr_t)Input.Data;
	iView = (uintptr_t)View.Data;
	return (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 遍历一个 DER 层级并验证游标单调性与借用范围。 */
static void __xrtX509FuzzDerLevel(
	xbytesview Input,
	xdercursor* pCursor,
	bool bEnter
)
{
	for ( size_t iGuard = 0; iGuard <= (Input.Size + 1u); iGuard++ ) {
		xdervalue Value;
		size_t iBefore = pCursor->Offset;
		xderresult Result = xrtDerRead(pCursor, &Value);

		if ( Result != XDER_VALUE ) {
			break;
		}
		if ( (pCursor->Offset <= iBefore) ||
			(pCursor->Offset > pCursor->Size) ||
			!__xrtX509FuzzViewInside(Input, Value.Raw) ||
			!__xrtX509FuzzViewInside(Input, Value.Value) ||
			(Value.HeaderSize > Value.Raw.Size) ) {
			abort();
		}
		if ( bEnter && Value.Tag.Constructed ) {
			xdercursor Child;

			if ( !xrtDerEnter(&Value, &Child) ) {
				abort();
			}
			__xrtX509FuzzDerLevel(Input, &Child, false);
		}
	}
	xrtClearError();
}



/* 覆盖 DER 验证、游标和独立 X.509 字段解析器。 */
static void __xrtX509FuzzDer(xbytesview Input)
{
	xdercursor Cursor;
	xx509algorithm Algorithm;
	xtime iTime;

	(void)xrtDerValidate(Input.Data, Input.Size);
	xrtClearError();
	if ( xrtDerInit(&Cursor, Input.Data, Input.Size) ) {
		__xrtX509FuzzDerLevel(Input, &Cursor, true);
	}
	(void)xrtX509AlgorithmParse(Input, &Algorithm);
	xrtClearError();
	(void)xrtX509TimeParse(Input, &iTime);
	xrtClearError();
}



/* 证书成功解析后继续遍历名称、扩展和公钥视图。 */
static void __xrtX509FuzzCertificate(xbytesview Input)
{
	xx509cert Cert;
	xx509namecursor Name;
	xx509nameattr Attribute;
	xx509extcursor Extensions;
	xx509ext Extension;
	xx509pubkey PublicKey;

	if ( !xrtX509Parse(Input.Data, Input.Size, &Cert) ) {
		xrtClearError();
		return;
	}
	if ( (Cert.Raw.Data != Input.Data) || (Cert.Raw.Size != Input.Size) ||
		!__xrtX509FuzzViewInside(Input, Cert.Tbs) ||
		!__xrtX509FuzzViewInside(Input, Cert.Serial) ||
		!__xrtX509FuzzViewInside(Input, Cert.Issuer) ||
		!__xrtX509FuzzViewInside(Input, Cert.Subject) ||
		!__xrtX509FuzzViewInside(Input, Cert.SubjectPublicKeyInfo) ||
		!__xrtX509FuzzViewInside(Input, Cert.Extensions) ||
		!__xrtX509FuzzViewInside(Input, Cert.Signature) ) {
		abort();
	}
	if ( xrtX509NameInit(Cert.Subject, &Name) ) {
		for ( size_t i = 0; i <= (Input.Size + 1u); i++ ) {
			xx509result Result = xrtX509NameRead(&Name, &Attribute);

			if ( Result != X509_VALUE ) {
				break;
			}
			if ( !__xrtX509FuzzViewInside(Input, Attribute.Raw) ||
				!__xrtX509FuzzViewInside(Input, Attribute.Oid) ||
				!__xrtX509FuzzViewInside(Input, Attribute.Value) ) {
				abort();
			}
		}
	}
	xrtClearError();
	if ( xrtX509ExtensionInit(&Cert, &Extensions) ) {
		for ( size_t i = 0; i <= (Input.Size + 1u); i++ ) {
			xx509result Result = xrtX509ExtensionRead(
				&Extensions, &Extension
			);

			if ( Result != X509_VALUE ) {
				break;
			}
			if ( !__xrtX509FuzzViewInside(Input, Extension.Raw) ||
				!__xrtX509FuzzViewInside(Input, Extension.Oid) ||
				!__xrtX509FuzzViewInside(Input, Extension.Value) ) {
				abort();
			}
		}
	}
	xrtClearError();
	(void)xrtX509PublicKey(&Cert, &PublicKey);
	xrtClearError();
}



/* 统一公开确定性回归和 libFuzzer 使用的 X.509/ASN.1 入口。 */
int xrtX509FuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	xbytesview Input;

	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_X509_FUZZ_INPUT_MAX) ) {
		return 0;
	}
	Input.Data = pData;
	Input.Size = iSize;
	__xrtX509FuzzDer(Input);
	__xrtX509FuzzCertificate(Input);
	return 0;
}



#if defined(XRT_X509_FUZZ_LIBFUZZER)

/* 把独立 X.509/ASN.1 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtX509FuzzerTestOneInput(pData, iSize);
}

#endif
