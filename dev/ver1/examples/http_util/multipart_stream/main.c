/*
 * XRT Example - Multipart Stream
 * XRT 范例 - Multipart 流式解析
 *
 * Description / 说明:
 *   EN: Demonstrates building a multipart body, extracting its boundary from Content-Type, and parsing the body incrementally with xrtMultipartStream.
 *   CN: 演示构建 multipart body、从 Content-Type 提取 boundary，并通过 xrtMultipartStream 做增量解析。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_util_multipart_stream.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_util_multipart_stream -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


int main(void)
{
	const char* sBoundary = "uploadB";
	char sContentType[128];
	char sBody[2048];
	char sFieldValue[64];
	char sFileValue[64];
	char sFileName[128];
	size_t iContentTypeLen = 0u;
	size_t iBodyLen = 0u;
	size_t iFieldValueLen = 0u;
	size_t iFileValueLen = 0u;
	size_t iFileNameLen = 0u;
	xrtmultipartstreamconfig tCfg;
	xrtmultipartstream tStream;
	xrtmultipartstreamevent tEvent;
	xrtstrview tParsedBoundary;
	int iPartIndex = 0;
	int iPartBeginCount = 0;
	int iPartEndCount = 0;
	int iRet = 0;

	xrtInit();
	memset(sContentType, 0, sizeof(sContentType));
	memset(sBody, 0, sizeof(sBody));
	memset(sFieldValue, 0, sizeof(sFieldValue));
	memset(sFileValue, 0, sizeof(sFileValue));
	memset(sFileName, 0, sizeof(sFileName));
	memset(&tCfg, 0, sizeof(tCfg));
	memset(&tStream, 0, sizeof(tStream));
	memset(&tEvent, 0, sizeof(tEvent));
	memset(&tParsedBoundary, 0, sizeof(tParsedBoundary));

	if ( !xrtMultipartBuildContentType(sBoundary, sContentType, sizeof(sContentType), &iContentTypeLen) ) {
		printf("build_content_type = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtMultipartAppendFieldPart(sBody, sizeof(sBody), &iBodyLen, sBoundary, "meta", "{\"ok\":1}") ) {
		printf("append_field = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtMultipartAppendFilePart(sBody, sizeof(sBody), &iBodyLen, sBoundary, "file", "a.txt", "text/plain", "hello file", 10u) ) {
		printf("append_file = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtMultipartAppendFinish(sBody, sizeof(sBody), &iBodyLen, sBoundary) ) {
		printf("append_finish = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtMultipartBoundaryFromContentType(sContentType, &tParsedBoundary) ) {
		printf("parse_boundary = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtMultipartStreamConfigInit(&tCfg);
	tCfg.iMaxBufferedBytes = 4096u;
	if ( !xrtMultipartStreamInit(&tStream, tParsedBoundary.sPtr, tParsedBoundary.iLen, &tCfg) ) {
		printf("stream_init = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtMultipartStreamFeed(&tStream, sBody, 43u) ) {
		printf("feed_chunk_1 = failed\n");
		iRet = 1;
		goto cleanup_stream;
	}
	while ( xrtMultipartStreamNext(&tStream, &tEvent) > 0 ) {
	}
	if ( !xrtMultipartStreamFeed(&tStream, sBody + 43u, iBodyLen - 43u) ) {
		printf("feed_chunk_2 = failed\n");
		iRet = 1;
		goto cleanup_stream;
	}
	xrtMultipartStreamFinish(&tStream);

	for ( ;; ) {
		xrtmultipartstreamresult iResult = xrtMultipartStreamNext(&tStream, &tEvent);

		if ( iResult == XRT_MULTIPART_STREAM_RESULT_NEED_MORE ) {
			printf("stream = unexpected_need_more\n");
			iRet = 1;
			goto cleanup_stream;
		}
		if ( iResult == XRT_MULTIPART_STREAM_RESULT_ERROR ) {
			printf("stream_error = %u\n", (unsigned)xrtMultipartStreamError(&tStream));
			iRet = 1;
			goto cleanup_stream;
		}
		if ( iResult == XRT_MULTIPART_STREAM_RESULT_END ) {
			break;
		}
		if ( iResult == XRT_MULTIPART_STREAM_RESULT_PART_BEGIN ) {
			++iPartIndex;
			++iPartBeginCount;
			printf("part[%d].name = %.*s\n", iPartIndex, (int)tEvent.tPart.tName.iLen, tEvent.tPart.tName.sPtr);
			if ( (tEvent.tPart.iFlags & XRT_MULTIPART_F_HAS_FILENAME) != 0u ) {
				if ( !xrtMultipartDecodeFileNameTo(&tEvent.tPart, sFileName, sizeof(sFileName), &iFileNameLen) ) {
					printf("decode_part_filename = failed\n");
					iRet = 1;
					goto cleanup_stream;
				}
				printf("part[%d].filename = %s\n", iPartIndex, sFileName);
			}
			continue;
		}
		if ( iResult == XRT_MULTIPART_STREAM_RESULT_DATA ) {
			if ( iPartIndex == 1 ) {
				memcpy(sFieldValue + iFieldValueLen, tEvent.tData.sPtr, tEvent.tData.iLen);
				iFieldValueLen += tEvent.tData.iLen;
				sFieldValue[iFieldValueLen] = '\0';
			} else if ( iPartIndex == 2 ) {
				memcpy(sFileValue + iFileValueLen, tEvent.tData.sPtr, tEvent.tData.iLen);
				iFileValueLen += tEvent.tData.iLen;
				sFileValue[iFileValueLen] = '\0';
			}
			continue;
		}
		if ( iResult == XRT_MULTIPART_STREAM_RESULT_PART_END ) {
			++iPartEndCount;
		}
	}

	printf("content_type = %s\n", sContentType);
	printf("boundary = %.*s\n", (int)tParsedBoundary.iLen, tParsedBoundary.sPtr);
	printf("field_value = %s\n", sFieldValue);
	printf("file_value = %s\n", sFileValue);
	printf("part_begin = %d\n", iPartBeginCount);
	printf("part_end = %d\n", iPartEndCount);

cleanup_stream:
	xrtMultipartStreamUnit(&tStream);
cleanup:
	xrtUnit();
	return iRet;
}
