#include "../internal/xrt_http.h"

#include <xrt/http_upgrade.h>



#if defined(XRT_FEATURE_HTTP_UPGRADE_WRITE)

/* 加载并验证一个写出描述符。 */
static bool __xrtHttpUpgradeDescriptorLoad(
	const xhttpupgradeitem* pInput,
	xhttpupgradeitem* pUpgrade,
	size_t* pSize
)
{
	xhttpupgradeitem Upgrade;
	size_t iRequired = 0;

	memcpy(&Upgrade, pInput, sizeof(Upgrade));
	if ( !__xrtHttpViewValid(Upgrade.Protocol) ||
		!__xrtHttpViewValid(Upgrade.Version) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Upgrade.Protocol) ||
		((Upgrade.Version.Size != 0) &&
		 !xrtHttpTokenValid(Upgrade.Version)) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpSizeAdd(
		&iRequired, Upgrade.Protocol.Size
	) || ((Upgrade.Version.Size != 0) &&
		(!__xrtHttpSizeAdd(&iRequired, 1u) ||
		 !__xrtHttpSizeAdd(
			&iRequired, Upgrade.Version.Size
		 ))) ) {
		return false;
	}
	memcpy(pUpgrade, &Upgrade, sizeof(Upgrade));
	*pSize = iRequired;
	return true;
}



/* 判断一段内存是否覆盖描述符数组或任一借用值。 */
static bool __xrtHttpUpgradeArrayOverlap(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xhttpupgradeitem Upgrade;
	size_t i;

	if ( __xrtRangesOverlap(
		pUpgrades, iCount * sizeof(*pUpgrades),
		pMemory, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Upgrade, pUpgrades + i, sizeof(Upgrade));
		if ( __xrtRangesOverlap(
			Upgrade.Protocol.Data,
			Upgrade.Protocol.Size,
			pMemory,
			iSize
		) || __xrtRangesOverlap(
			Upgrade.Version.Data,
			Upgrade.Version.Size,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 规范写出一个或多个 Upgrade 协议。 */
XRT_API bool xrtHttpUpgradeWrite(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpupgradeitem Upgrade;
	bytes pBytes = (bytes)pOutput;
	size_t iElement;
	size_t iRequired = 0;
	size_t iPosition = 0;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(*pUpgrades))) ||
		!__xrtRangeValid(
			pUpgrades, iCount * sizeof(*pUpgrades)
		) || !__xrtRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtRangeValid(pOutput, iCapacity)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtHttpUpgradeArrayOverlap(
		pUpgrades, iCount, pSize, sizeof(iRequired)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpUpgradeDescriptorLoad(
			pUpgrades + i, &Upgrade, &iElement
		) || ((i != 0) &&
			!__xrtHttpSizeAdd(&iRequired, 2u)) ||
			!__xrtHttpSizeAdd(&iRequired, iElement) ) {
			return false;
		}
	}
	if ( (pOutput != NULL) &&
		(__xrtHttpUpgradeArrayOverlap(
			pUpgrades, iCount, pOutput, iRequired
		 ) || __xrtRangesOverlap(
			pOutput, iRequired,
			pSize, sizeof(iRequired)
		 )) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		__xrtErrorSetRange();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Upgrade, pUpgrades + i, sizeof(Upgrade));
		if ( i != 0 ) {
			pBytes[iPosition++] = (uint8)',';
			pBytes[iPosition++] = (uint8)' ';
		}
		memcpy(
			pBytes + iPosition,
			Upgrade.Protocol.Data,
			Upgrade.Protocol.Size
		);
		iPosition += Upgrade.Protocol.Size;
		if ( Upgrade.Version.Size != 0 ) {
			pBytes[iPosition++] = (uint8)'/';
			memcpy(
				pBytes + iPosition,
				Upgrade.Version.Data,
				Upgrade.Version.Size
			);
			iPosition += Upgrade.Version.Size;
		}
	}
	return true;
}



/* 规范写出一个 Upgrade 协议元素。 */
XRT_API bool xrtHttpUpgradeElementWrite(
	const xhttpupgradeitem* pUpgrade,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpUpgradeWrite(
		pUpgrade, 1u, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾 Upgrade 字段值。 */
XRT_API str xrtHttpUpgradeBuild(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	size_t* pSize
)
{
	size_t iRequired;
	str sOutput;

	if ( (pSize != NULL) &&
		(!__xrtRangeValid(pSize, sizeof(*pSize)) ||
		 ((iCount <= (SIZE_MAX / sizeof(*pUpgrades))) &&
		  __xrtRangeValid(
			pUpgrades, iCount * sizeof(*pUpgrades)
		  ) && __xrtHttpUpgradeArrayOverlap(
			pUpgrades, iCount, pSize, sizeof(*pSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpUpgradeWrite(
		pUpgrades, iCount, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpUpgradeWrite(
		pUpgrades,
		iCount,
		sOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = 0;
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}

#endif
