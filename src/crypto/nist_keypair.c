#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_NIST_KEYPAIR)

/* 从安全随机源采样私钥并原子发布密钥对。 */
bool __xrtNistKeyPairApi(
	int iCurve,
	size_t iPrivateSize,
	size_t iPublicSize,
	void* pPrivate,
	void* pPublic,
	cstr sOperation
)
{
	const uint8* pOrder = __xrtNistOrder(iCurve, NULL);
	uint8 Private[48];
	uint8 Public[97];

	if ( __xrtCryptoRangesOverlap(
		pPrivate, iPrivateSize, pPublic, iPublicSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	do {
		if ( !xrtSecureRandom(Private, iPrivateSize) ) {
			xrtSecureZero(Private, sizeof(Private));
			xrtSecureZero(Public, sizeof(Public));
			return false;
		}
	} while ( __xrtNistScalarValid(Private, pOrder, iPrivateSize) == 0 );

	if ( !__xrtNistPublicApi(
		iCurve,
		Private,
		pOrder,
		iPrivateSize,
		iPublicSize,
		Public,
		sOperation
	) ) {
		xrtSecureZero(Private, sizeof(Private));
		xrtSecureZero(Public, sizeof(Public));
		return false;
	}
	memcpy(pPrivate, Private, iPrivateSize);
	memcpy(pPublic, Public, iPublicSize);
	xrtSecureZero(Private, sizeof(Private));
	xrtSecureZero(Public, sizeof(Public));
	return true;
}

#endif
