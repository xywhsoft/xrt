#include "../test.h"



typedef struct multipart_stream_result {
	char Names[2][16];
	uint8 Bodies[2][64];
	size_t BodySizes[2];
	size_t Parts;
	size_t Ends;
} multipart_stream_result;



/* 以指定小块喂入 Reader，并立即消费所有借用事件。 */
static bool testMultipartStreamRun(
	const uint8* pSource,
	size_t iSourceSize,
	size_t iChunk,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits,
	multipart_stream_result* pResult,
	xmultiparterrorinfo* pFinalError
)
{
	xmultipartreader Reader;
	xmultiparterrorinfo Error;
	xmultipartpart Part;
	xbytesview Data;
	uint8 Buffer[256];
	size_t iBuffered = 0;
	size_t iSource = 0;
	size_t iConsumed;

	memset(pResult, 0, sizeof(*pResult));
	memset(pFinalError, 0, sizeof(*pFinalError));
	if ( !xrtMultipartReaderInit(
		&Reader, pBoundary, pLimits
	) ) {
		return false;
	}
	for ( ;; ) {
		xmultipartreadstatus Status;
		bool bEnd;

		if ( (iSource < iSourceSize) &&
			(iBuffered < sizeof(Buffer)) ) {
			size_t iAdd = iSourceSize - iSource;

			if ( iAdd > iChunk ) {
				iAdd = iChunk;
			}
			if ( iAdd > (sizeof(Buffer) - iBuffered) ) {
				iAdd = sizeof(Buffer) - iBuffered;
			}
			memcpy(
				Buffer + iBuffered,
				pSource + iSource, iAdd
			);
			iBuffered += iAdd;
			iSource += iAdd;
		}
		bEnd = iSource == iSourceSize;
		Status = xrtMultipartReaderRead(
			&Reader,
			(xbytesview){ Buffer, iBuffered },
			bEnd, &iConsumed, &Part, &Data, &Error
		);
		if ( Status == XMULTIPART_READ_ERROR ) {
			*pFinalError = Error;
			return false;
		}
		if ( Status == XMULTIPART_READ_PART ) {
			size_t iName;

			if ( pResult->Parts >= 2 ) {
				return false;
			}
			if ( (Part.Flags &
				XMULTIPART_PART_DISPOSITION) != 0 ) {
				if ( !xrtMultipartPartNameWrite(
					&Part,
					pResult->Names[pResult->Parts],
					sizeof(pResult->Names[0]),
					&iName
				) || (iName >=
					sizeof(pResult->Names[0])) ) {
					return false;
				}
				pResult->Names[pResult->Parts][iName] = '\0';
			}
			pResult->Parts++;
		} else if ( Status == XMULTIPART_READ_DATA ) {
			size_t iPart;

			if ( pResult->Parts == 0 ) {
				return false;
			}
			iPart = pResult->Parts - 1u;
			if ( Data.Size >
				(sizeof(pResult->Bodies[iPart]) -
				 pResult->BodySizes[iPart]) ) {
				return false;
			}
			memcpy(
				pResult->Bodies[iPart] +
					pResult->BodySizes[iPart],
				Data.Data, Data.Size
			);
			pResult->BodySizes[iPart] += Data.Size;
		} else if ( Status ==
			XMULTIPART_READ_PART_END ) {
			pResult->Ends++;
		} else if ( Status == XMULTIPART_READ_DONE ) {
			return xrtMultipartReaderDone(&Reader) &&
				(iSource == iSourceSize) &&
				(iConsumed <= iBuffered);
		}
		if ( iConsumed > iBuffered ) {
			return false;
		}
		if ( iConsumed != 0 ) {
			memmove(
				Buffer, Buffer + iConsumed,
				iBuffered - iConsumed
			);
			iBuffered -= iConsumed;
		}
		if ( (Status == XMULTIPART_READ_MORE) &&
			(iConsumed == 0) && bEnd ) {
			return false;
		}
		if ( (Status == XMULTIPART_READ_MORE) &&
			(iConsumed == 0) &&
			(iBuffered == sizeof(Buffer)) ) {
			return false;
		}
	}
}



/* 验证每个分块位置、false candidate、preamble 和 epilogue。 */
static void testMultipartStreamChunks(void)
{
	static const char Body[] =
		"preamble\r\n"
		"--stream\r\n"
		"Content-Disposition: form-data; name=\"first\"\r\n"
		"\r\n"
		"one\r\n--streamX\r\ntwo\r\n"
		"--stream\r\n"
		"Content-Disposition: form-data; name=\"file\"\r\n"
		"Content-Type: application/octet-stream\r\n"
		"\r\n"
		"hello\r\nworld\r\n"
		"--stream--\r\n"
		"epilogue";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	multipart_stream_result Result;
	size_t iChunk;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("stream"), &Boundary
	), "Multipart stream boundary init failed");
	for ( iChunk = 1; iChunk <= 17; iChunk++ ) {
		testRequire(testMultipartStreamRun(
			(const uint8*)Body, sizeof(Body) - 1u,
			iChunk, &Boundary, NULL, &Result, &Error
		), "Multipart stream chunked parse failed");
		testRequire((Result.Parts == 2) &&
			(Result.Ends == 2) &&
			(strcmp(Result.Names[0], "first") == 0) &&
			(strcmp(Result.Names[1], "file") == 0) &&
			(Result.BodySizes[0] == 19) &&
			(memcmp(
				Result.Bodies[0],
				"one\r\n--streamX\r\ntwo", 19
			) == 0) &&
			(Result.BodySizes[1] == 12) &&
			(memcmp(
				Result.Bodies[1],
				"hello\r\nworld", 12
			) == 0), "Multipart stream event mismatch");
	}
}



/* 验证截断、Header、Part 和 delimiter 限额。 */
static void testMultipartStreamLimits(void)
{
	static const char Truncated[] =
		"--b\r\n\r\nbody";
	static const char PartLimit[] =
		"--b\r\n\r\nbody\r\n--b--\r\n";
	static const char LongDelimiter[] =
		"--b                              \r\n"
		"\r\n"
		"x\r\n"
		"--b--\r\n";
	xmultipartboundary Boundary;
	xmultiparterrorinfo Error;
	xmultipartlimits Limits;
	multipart_stream_result Result;
	bool bResult;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"), &Boundary
	), "Multipart stream limit boundary init failed");
	bResult = testMultipartStreamRun(
		(const uint8*)Truncated,
		sizeof(Truncated) - 1u, 3,
		&Boundary, NULL, &Result, &Error
	);
	testRequire(!bResult,
		"Multipart stream accepted truncated body");
	testRequire(Error.Code == XMULTIPART_ERROR_TRUNCATED,
		"Multipart stream truncated error mismatch");
	xrtClearError();

	xrtMultipartLimitsInit(&Limits);
	Limits.MaxPartBytes = 2;
	bResult = testMultipartStreamRun(
		(const uint8*)PartLimit,
		sizeof(PartLimit) - 1u, 2,
		&Boundary, &Limits, &Result, &Error
	);
	testRequire(!bResult,
		"Multipart stream Part limit was not enforced");
	testRequire(Error.Code == XMULTIPART_ERROR_PART_BYTES_LIMIT,
		"Multipart stream Part limit error mismatch");
	xrtClearError();

	xrtMultipartLimitsInit(&Limits);
	Limits.MaxDelimiterBytes = 12;
	bResult = testMultipartStreamRun(
		(const uint8*)LongDelimiter,
		sizeof(LongDelimiter) - 1u, 4,
		&Boundary, &Limits, &Result, &Error
	);
	testRequire(!bResult,
		"Multipart stream delimiter limit was not enforced");
	testRequire(Error.Code ==
		XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
		"Multipart stream delimiter limit error mismatch");
	testRequire(!xrtMultipartValidate(
		(xbytesview){
			(const uint8*)LongDelimiter,
			sizeof(LongDelimiter) - 1u
		}, &Boundary, &Limits, &Error
	) && (Error.Code ==
		XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT),
		"Multipart complete delimiter limit was not enforced");
	xrtClearError();
}



/* 验证 Reset 保留配置并清空所有解析进度。 */
static void testMultipartStreamReset(void)
{
	xmultipartboundary Boundary;
	xmultipartreader Reader;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xbytesview Data;
	size_t iConsumed;

	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("reset"), &Boundary
	) && xrtMultipartReaderInit(
		&Reader, &Boundary, NULL
	), "Multipart Reader reset init failed");
	testRequire(xrtMultipartReaderRead(
		&Reader,
		(xbytesview){
			(const uint8*)"--reset\r\n\r\n", 11u
		}, false, &iConsumed, &Part, &Data, &Error
	) == XMULTIPART_READ_PART,
		"Multipart Reader did not enter Part");
	xrtMultipartReaderReset(&Reader);
	testRequire((Reader.Parts == 0) &&
		(Reader.WireBytes == 0) &&
		!xrtMultipartReaderDone(&Reader),
		"Multipart Reader reset retained progress");
}



/* 验证 Reader 快照固定描述符、支持未对齐输出并拒绝损坏状态。 */
static void testMultipartStreamMemoryContracts(void)
{
	static const char InputText[] =
		"--b\r\n"
		"Content-Disposition: form-data; name=x\r\n"
		"\r\n";
	uint8 BoundaryStorage[sizeof(xmultipartboundary) + 2u];
	uint8 LimitsStorage[sizeof(xmultipartlimits) + 2u];
	uint8 ConsumedStorage[sizeof(size_t) + 2u];
	uint8 PartStorage[sizeof(xmultipartpart) + 2u];
	uint8 DataStorage[sizeof(xbytesview) + 2u];
	uint8 ErrorStorage[sizeof(xmultiparterrorinfo) + 2u];
	uint8 ReaderStorage[sizeof(xmultipartreader) + 2u];
	xmultipartboundary Boundary;
	xmultipartreader Reader;
	xmultipartpart Part;
	xmultiparterrorinfo Error;
	xbytesview Data;
	size_t iConsumed;
	xmultipartreadstatus Status;
	xmultipartreader* pUnaligned = (xmultipartreader*)(void*)(
		(uintptr_t)ReaderStorage | (uintptr_t)1u
	);

	memset(BoundaryStorage, 0xA5, sizeof(BoundaryStorage));
	memset(LimitsStorage, 0xA5, sizeof(LimitsStorage));
	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("b"),
		(xmultipartboundary*)(void*)(BoundaryStorage + 1u)
	), "Multipart stream unaligned boundary init failed");
	xrtMultipartLimitsInit(
		(xmultipartlimits*)(void*)(LimitsStorage + 1u)
	);
	memcpy(&Boundary, BoundaryStorage + 1u, sizeof(Boundary));
	testRequire(xrtMultipartReaderInit(
		&Reader,
		(const xmultipartboundary*)(const void*)(BoundaryStorage + 1u),
		(const xmultipartlimits*)(const void*)(LimitsStorage + 1u)
	), "Multipart Reader did not snapshot unaligned descriptors");
	memset(BoundaryStorage + 1u, 0, sizeof(xmultipartboundary));
	memset(LimitsStorage + 1u, 0, sizeof(xmultipartlimits));
	memset(ConsumedStorage, 0xA5, sizeof(ConsumedStorage));
	memset(PartStorage, 0xA5, sizeof(PartStorage));
	memset(DataStorage, 0xA5, sizeof(DataStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	Status = xrtMultipartReaderRead(
		&Reader,
		(xbytesview){
			(const uint8*)InputText,
			sizeof(InputText) - 1u
		},
		false,
		(size_t*)(void*)(ConsumedStorage + 1u),
		(xmultipartpart*)(void*)(PartStorage + 1u),
		(xbytesview*)(void*)(DataStorage + 1u),
		(xmultiparterrorinfo*)(void*)(ErrorStorage + 1u)
	);
	memcpy(&iConsumed, ConsumedStorage + 1u, sizeof(iConsumed));
	memcpy(&Part, PartStorage + 1u, sizeof(Part));
	memcpy(&Data, DataStorage + 1u, sizeof(Data));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire((Status == XMULTIPART_READ_PART) &&
		(iConsumed == (sizeof(InputText) - 1u)) &&
		(Part.HeaderCount == 1u) &&
		(Data.Data == NULL) && (Data.Size == 0) &&
		(Error.Code == XMULTIPART_ERROR_NONE) &&
		(ConsumedStorage[0] == 0xA5) &&
		(ConsumedStorage[sizeof(ConsumedStorage) - 1u] == 0xA5) &&
		(PartStorage[0] == 0xA5) &&
		(PartStorage[sizeof(PartStorage) - 1u] == 0xA5) &&
		(DataStorage[0] == 0xA5) &&
		(DataStorage[sizeof(DataStorage) - 1u] == 0xA5) &&
		(ErrorStorage[0] == 0xA5) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] == 0xA5),
		"Multipart Reader unaligned output mismatch"
	);

	xrtMultipartReaderReset(&Reader);
	Reader.PartBytes = 2u;
	Reader.Limits.MaxPartBytes = 1u;
	Status = xrtMultipartReaderRead(
		&Reader,
		(xbytesview){ NULL, 0 },
		false,
		&iConsumed,
		&Part,
		&Data,
		&Error
	);
	testRequire((Status == XMULTIPART_READ_ERROR) &&
		(Error.Code == XMULTIPART_ERROR_ARGUMENT) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Multipart Reader accepted counters beyond hard limits");
	xrtClearError();

	testRequire(!xrtMultipartReaderInit(
		pUnaligned,
		&Boundary,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Multipart Reader accepted an unaligned state object");
	xrtClearError();
	testRequire(!xrtMultipartReaderDone(
		(const xmultipartreader*)(uintptr_t)(UINTPTR_MAX - 1u)
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Multipart Reader query accepted wrapping state");
	xrtClearError();
}



int main(void)
{
	testMultipartStreamChunks();
	testMultipartStreamLimits();
	testMultipartStreamReset();
	testMultipartStreamMemoryContracts();
	printf("[PASS] multipart_stream\n");
	return 0;
}
