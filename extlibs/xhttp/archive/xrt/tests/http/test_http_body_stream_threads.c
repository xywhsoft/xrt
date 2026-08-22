#include "../test.h"



#define TEST_HTTP_BODY_STREAM_PRODUCERS 4u
#define TEST_HTTP_BODY_STREAM_RECORDS 1000u
#define TEST_HTTP_BODY_STREAM_TOTAL \
	(TEST_HTTP_BODY_STREAM_PRODUCERS * TEST_HTTP_BODY_STREAM_RECORDS)



/* 每个生产线程独占一个生产端引用和一段不重叠的记录编号。 */
typedef struct test_http_body_stream_producer {
	xhttpbodystream* Stream;
	uint32 First;
	uint32 Count;
} test_http_body_stream_producer;



/* 在硬背压下等待可写，并保证退出时释放自己的生产端引用。 */
static int32 testHttpBodyStreamProduce(ptr pData)
{
	test_http_body_stream_producer* pProducer =
		(test_http_body_stream_producer*)pData;
	uint32 i;
	int32 iResult = 0;

	for ( i = 0; i < pProducer->Count; i++ ) {
		uint32 iRecord = pProducer->First + i;

		for ( ;; ) {
			xhttpbodystreamresult Result = xrtHttpBodyStreamWrite(
				pProducer->Stream,
				(xbytesview){
					(cbytes)&iRecord,
					sizeof(iRecord)
				}
			);

			if ( Result == XHTTP_BODY_STREAM_OK ) {
				break;
			}
			if ( Result == XHTTP_BODY_STREAM_AGAIN ) {
				xfuture* pWritable = xrtHttpBodyStreamWaitWritable(
					pProducer->Stream
				);

				if ( (pWritable == NULL) ||
					(xrtFutureWait(pWritable) != XWAIT_OK) ||
					(xrtFutureState(pWritable) != XFUTURE_RESOLVED) ) {
					xrtFutureDestroy(pWritable);
					iResult = 1;
					break;
				}
				xrtFutureDestroy(pWritable);
				continue;
			}
			iResult = 2;
			break;
		}
		if ( iResult != 0 ) {
			break;
		}
	}
	xrtHttpBodyStreamDestroy(pProducer->Stream);
	pProducer->Stream = NULL;
	return iResult;
}



/* 以三字节切片重组四字节记录，验证并发写入不会交叉或丢失。 */
static void testHttpBodyStreamConsume(
	xhttpbodyreader* pReader,
	bool* pSeen
)
{
	uint8 Record[sizeof(uint32)];
	size_t iRecordOffset = 0;
	size_t iReceived = 0;

	for ( ;; ) {
		xhttpbodychunk Chunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader, 3, &Chunk
		);

		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pReadable = xrtHttpBodyReaderWait(pReader);

			testRequire(
				(pReadable != NULL) &&
				(xrtFutureWait(pReadable) == XWAIT_OK) &&
				(xrtFutureState(pReadable) == XFUTURE_RESOLVED),
				"HTTP body stream readable wait failed"
			);
			xrtFutureDestroy(pReadable);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire(Status == XHTTP_BODY_DATA,
			"HTTP body stream reader failed under concurrency");
		for ( size_t i = 0; i < Chunk.Size; i++ ) {
			Record[iRecordOffset++] = Chunk.Data[i];
			if ( iRecordOffset == sizeof(Record) ) {
				uint32 iRecord;

				memcpy(&iRecord, Record, sizeof(iRecord));
				testRequire(
					(iRecord < TEST_HTTP_BODY_STREAM_TOTAL) &&
					!pSeen[iRecord],
					"HTTP body stream record was corrupt or duplicated"
				);
				pSeen[iRecord] = true;
				iReceived++;
				iRecordOffset = 0;
			}
		}
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(
		(iRecordOffset == 0) &&
		(iReceived == TEST_HTTP_BODY_STREAM_TOTAL),
		"HTTP body stream lost concurrent records"
	);
}



/* 验证多生产者、小预算、共享可写 Future 和最后生产端自动 EOF。 */
int main(void)
{
	xhttpbodystreamconfig Config = { 64, 8 };
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(&Config, &pStream);
	xhttpbodyreader* pReader;
	test_http_body_stream_producer
		Producers[TEST_HTTP_BODY_STREAM_PRODUCERS];
	xthread* Threads[TEST_HTTP_BODY_STREAM_PRODUCERS];
	bool Seen[TEST_HTTP_BODY_STREAM_TOTAL];
	size_t i;

	testRequire((pBody != NULL) && (pStream != NULL),
		"HTTP body stream thread setup failed");
	pReader = xrtHttpBodyOpen(pBody);
	testRequire(pReader != NULL,
		"HTTP body stream thread reader open failed");
	memset(Seen, 0, sizeof(Seen));
	for ( i = 0; i < TEST_HTTP_BODY_STREAM_PRODUCERS; i++ ) {
		Producers[i].Stream = xrtHttpBodyStreamRef(pStream);
		Producers[i].First = (uint32)(i * TEST_HTTP_BODY_STREAM_RECORDS);
		Producers[i].Count = TEST_HTTP_BODY_STREAM_RECORDS;
		testRequire(Producers[i].Stream != NULL,
			"HTTP body stream producer retain failed");
		Threads[i] = xrtThreadCreate(
			testHttpBodyStreamProduce,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP body stream producer thread create failed");
	}
	xrtHttpBodyStreamDestroy(pStream);
	pStream = NULL;

	testHttpBodyStreamConsume(pReader, Seen);
	for ( i = 0; i < TEST_HTTP_BODY_STREAM_PRODUCERS; i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0) &&
			(Producers[i].Stream == NULL),
			"HTTP body stream producer thread failed"
		);
		xrtThreadDestroy(Threads[i]);
	}
	for ( i = 0; i < TEST_HTTP_BODY_STREAM_TOTAL; i++ ) {
		testRequire(Seen[i],
			"HTTP body stream record was not observed");
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] threaded HTTP body stream\n");
	return 0;
}
