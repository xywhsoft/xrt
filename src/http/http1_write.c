#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP1_HEAD)

/* 安全累加封包长度。 */
bool __xrtHttp1SizeAdd(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 判断输出范围是否会覆盖仍需读取的输入视图。 */
static bool __xrtHttp1Overlap(
	const void* pOutput,
	size_t iOutputSize,
	xstrview Input
)
{
	uintptr_t iOutput;
	uintptr_t iInput;

	if ( (iOutputSize == 0) || (Input.Size == 0) ) {
		return false;
	}
	iOutput = (uintptr_t)pOutput;
	iInput = (uintptr_t)Input.Data;
	if ( iOutput <= iInput ) {
		return (iInput - iOutput) < iOutputSize;
	}
	return (iOutput - iInput) < Input.Size;
}



/* 校验公共封包参数并累计全部字段长度。 */
bool __xrtHttp1WriteMeasure(
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t iBase,
	size_t* pRequired,
	cstr sOperation
)
{
	xhttpfield Field;
	size_t i;
	size_t iSize = iBase;

	if ( !__xrtHttpFieldArrayValid(
		pFields, iFieldCount
	) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 field array is invalid"
		);
		return false;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpTokenValid(Field.Name) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_FIELD_NAME, i, 0,
				XERR_VALUE, sOperation,
				"HTTP/1 output field name is invalid"
			);
			return false;
		}
		if ( !xrtHttpFieldValueValid(Field.Value) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_FIELD_VALUE, i, 0,
				XERR_VALUE, sOperation,
				"HTTP/1 output field value is invalid"
			);
			return false;
		}
		if ( !__xrtHttp1SizeAdd(&iSize, Field.Name.Size) ||
			!__xrtHttp1SizeAdd(&iSize, 2) ||
			!__xrtHttp1SizeAdd(&iSize, Field.Value.Size) ||
			!__xrtHttp1SizeAdd(&iSize, 2) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, i, 0,
				XERR_RANGE, sOperation,
				"HTTP/1 output size overflows"
			);
			return false;
		}
	}
	if ( !__xrtHttp1SizeAdd(&iSize, 2) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, sOperation,
			"HTTP/1 output size overflows"
		);
		return false;
	}
	*pRequired = iSize;
	return true;
}



/* 在容量已经确认后写入字段区和最终空行。 */
size_t __xrtHttp1FieldsWrite(
	bytes pOutput,
	size_t iPosition,
	const xhttpfield* pFields,
	size_t iFieldCount
)
{
	return iPosition + __xrtHttpFieldWriteUnchecked(
		pFields, iFieldCount, true,
		pOutput + iPosition
	);
}



/* 检查封包输出查询、容量和所有输入视图的非重叠契约。 */
bool __xrtHttp1WriteOutputValid(
	const xstrview* pParts,
	size_t iPartCount,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize,
	cstr sOperation
)
{
	xhttpfield Field;
	size_t iCheckSize;
	size_t i;

	if ( !__xrtRangeValid(pSize, sizeof(iRequired)) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 output size range is invalid"
		);
		return false;
	}
	for ( i = 0; i < iPartCount; i++ ) {
		if ( __xrtRangesOverlap(
			pSize, sizeof(iRequired),
			pParts[i].Data, pParts[i].Size
		) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_ARGUMENT, i, 0,
				XERR_ARGUMENT, sOperation,
				"HTTP/1 output size overlaps an input view"
			);
			return false;
		}
	}
	if ( __xrtHttpFieldArrayOverlap(
		pFields, iFieldCount,
		pSize, sizeof(iRequired)
	) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 output size overlaps the field array"
		);
		return false;
	}
	if ( pOutput == NULL ) {
		if ( iCapacity != 0 ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
				XERR_ARGUMENT, sOperation,
				"HTTP/1 null output has nonzero capacity"
			);
			return false;
		}
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	iCheckSize = __xrtOutputCheckSize(iRequired, iCapacity);
	if ( !__xrtRangeValid(pOutput, iCheckSize) ||
		__xrtRangesOverlap(
			pOutput, iCheckSize,
			pSize, sizeof(iRequired)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iFieldCount,
			pOutput, iCheckSize
		) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_ARGUMENT, 0, 0,
			XERR_ARGUMENT, sOperation,
			"HTTP/1 output range or alias is invalid"
		);
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, sOperation,
			"HTTP/1 output capacity is insufficient"
		);
		return false;
	}
	for ( i = 0; i < iPartCount; i++ ) {
		if ( __xrtHttp1Overlap(pOutput, iRequired, pParts[i]) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_ARGUMENT, i, 0,
				XERR_ARGUMENT, sOperation,
				"HTTP/1 output overlaps an input view"
			);
			return false;
		}
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtHttp1Overlap(
			pOutput, iRequired, Field.Name
		) || __xrtHttp1Overlap(
			pOutput, iRequired, Field.Value
		) ) {
			(void)__xrtHttp1Fail(
				NULL, NULL, XHTTP1_ERROR_ARGUMENT, i, 0,
				XERR_ARGUMENT, sOperation,
				"HTTP/1 output overlaps a field view"
			);
			return false;
		}
	}
	return true;
}



/* 校验并写入完整请求 Header。 */
XRT_API bool xrtHttp1RequestWrite(
	xstrview Method,
	xstrview Target,
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	cstr sVersion = __xrtHttp1VersionText(Version);
	xstrview Parts[2];
	size_t iRequired;
	size_t iPosition = 0;
	bytes pBytes = (bytes)pOutput;
	bool bMethod = xrtHttpTokenValid(Method);
	bool bTarget = xrtHttp1TargetValid(Target);

	if ( (sVersion == NULL) || !bMethod || !bTarget ) {
		(void)__xrtHttp1Fail(
			NULL, NULL,
			(sVersion == NULL) ? XHTTP1_ERROR_VERSION :
				(!bMethod ?
				 XHTTP1_ERROR_METHOD : XHTTP1_ERROR_TARGET),
			0, 0, XERR_VALUE, "write-http1-request",
			"HTTP/1 request line is invalid"
		);
		return false;
	}
	if ( (Method.Size > (SIZE_MAX - Target.Size)) ||
		((Method.Size + Target.Size) > (SIZE_MAX - 12u)) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, "write-http1-request",
			"HTTP/1 request size overflows"
		);
		return false;
	}
	if ( !__xrtHttp1WriteMeasure(
		pFields,
		iFieldCount,
		Method.Size + Target.Size + 12u,
		&iRequired,
		"write-http1-request"
	) ) {
		return false;
	}
	Parts[0] = Method;
	Parts[1] = Target;
	if ( !__xrtHttp1WriteOutputValid(
		Parts, 2, pFields, iFieldCount,
		pOutput, iCapacity, iRequired, pSize,
		"write-http1-request"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	memcpy(pBytes + iPosition, Method.Data, Method.Size);
	iPosition += Method.Size;
	pBytes[iPosition++] = (uint8)' ';
	memcpy(pBytes + iPosition, Target.Data, Target.Size);
	iPosition += Target.Size;
	pBytes[iPosition++] = (uint8)' ';
	memcpy(pBytes + iPosition, sVersion, 8);
	iPosition += 8;
	memcpy(pBytes + iPosition, "\r\n", 2);
	iPosition += 2;
	iPosition = __xrtHttp1FieldsWrite(
		pBytes, iPosition, pFields, iFieldCount
	);
	memcpy(pSize, &iPosition, sizeof(iPosition));
	return true;
}



/* 校验并写入完整响应 Header。 */
XRT_API bool xrtHttp1ResponseWrite(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	cstr sVersion = __xrtHttp1VersionText(Version);
	xstrview Parts[1];
	size_t iRequired;
	size_t iPosition = 0;
	bytes pBytes = (bytes)pOutput;

	if ( (sVersion == NULL) || (iStatus < 100) ||
		(iStatus > 999) || !xrtHttpFieldValueValid(Reason) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL,
			(sVersion == NULL) ? XHTTP1_ERROR_VERSION :
				(((iStatus < 100) || (iStatus > 999)) ?
				 XHTTP1_ERROR_STATUS : XHTTP1_ERROR_REASON),
			0, 0, XERR_VALUE, "write-http1-response",
			"HTTP/1 response line is invalid"
		);
		return false;
	}
	if ( Reason.Size > (SIZE_MAX - 15u) ) {
		(void)__xrtHttp1Fail(
			NULL, NULL, XHTTP1_ERROR_OUTPUT_SIZE, 0, 0,
			XERR_RANGE, "write-http1-response",
			"HTTP/1 response size overflows"
		);
		return false;
	}
	if ( !__xrtHttp1WriteMeasure(
		pFields,
		iFieldCount,
		Reason.Size + 15u,
		&iRequired,
		"write-http1-response"
	) ) {
		return false;
	}
	Parts[0] = Reason;
	if ( !__xrtHttp1WriteOutputValid(
		Parts, 1, pFields, iFieldCount,
		pOutput, iCapacity, iRequired, pSize,
		"write-http1-response"
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	memcpy(pBytes + iPosition, sVersion, 8);
	iPosition += 8;
	pBytes[iPosition++] = (uint8)' ';
	pBytes[iPosition++] = (uint8)('0' + (iStatus / 100));
	pBytes[iPosition++] = (uint8)('0' + ((iStatus / 10) % 10));
	pBytes[iPosition++] = (uint8)('0' + (iStatus % 10));
	pBytes[iPosition++] = (uint8)' ';
	if ( Reason.Size != 0 ) {
		memcpy(pBytes + iPosition, Reason.Data, Reason.Size);
	}
	iPosition += Reason.Size;
	memcpy(pBytes + iPosition, "\r\n", 2);
	iPosition += 2;
	iPosition = __xrtHttp1FieldsWrite(
		pBytes, iPosition, pFields, iFieldCount
	);
	memcpy(pSize, &iPosition, sizeof(iPosition));
	return true;
}

#endif
