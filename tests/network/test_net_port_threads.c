#include "../test.h"
#include "../test_thread.h"



#define TEST_PORT_PRODUCERS 4
#define TEST_PORT_POSTS 1000



/* 每个生产者使用独立 ID 区间并发投递。 */
typedef struct testportproducer {
	xnetport* Port;
	uint64 Base;
} testportproducer;



/* 投递固定数量事件，任一背压或唤醒失败都直接返回失败。 */
static int testPortProducer(ptr pData)
{
	testportproducer* pProducer = (testportproducer*)pData;

	for ( uint64 i = 0; i < TEST_PORT_POSTS; i++ ) {
		if ( !xrtNetPortPost(pProducer->Port,
			pProducer->Base + i, pProducer) ) {
			return 1;
		}
	}
	return 0;
}



/* 多生产者投递必须无丢失、无重复，并能持续唤醒单消费者。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Events[64];
	xnetport* pPort;
	testportproducer Producers[TEST_PORT_PRODUCERS];
	testthread Threads[TEST_PORT_PRODUCERS];
	bool Seen[TEST_PORT_PRODUCERS * TEST_PORT_POSTS];
	size_t iReceived = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_AUTO;
	Config.PostLimit = TEST_PORT_PRODUCERS * TEST_PORT_POSTS;
	pPort = xrtNetPortCreate(&Config);
	testRequire(pPort != NULL, "threaded network port create failed");
	memset(Seen, 0, sizeof(Seen));
	memset(Threads, 0, sizeof(Threads));

	for ( size_t i = 0; i < TEST_PORT_PRODUCERS; i++ ) {
		Producers[i].Port = pPort;
		Producers[i].Base = (uint64)(i * TEST_PORT_POSTS);
		Threads[i].Proc = testPortProducer;
		Threads[i].Data = &Producers[i];
	}
	testThreadsStart(Threads, TEST_PORT_PRODUCERS);

	while ( iReceived < (TEST_PORT_PRODUCERS * TEST_PORT_POSTS) ) {
		size_t iCount = 0;

		testRequire(xrtNetPortWait(pPort, Events, 64,
			xrtDeadlineAfter(5000000), &iCount) == XNET_RESULT_OK,
			"threaded network port wait failed");
		for ( size_t i = 0; i < iCount; i++ ) {
			size_t iId;

			testRequire(Events[i].Type == XNET_PORT_EVENT_USER,
				"threaded network port returned a non-user event");
			iId = (size_t)Events[i].Id;
			testRequire((iId < (TEST_PORT_PRODUCERS * TEST_PORT_POSTS)) &&
				!Seen[iId], "threaded network port duplicated an event");
			Seen[iId] = true;
			iReceived++;
		}
	}

	testThreadsJoin(Threads, TEST_PORT_PRODUCERS);
	for ( size_t i = 0; i < TEST_PORT_PRODUCERS; i++ ) {
		testRequire(Threads[i].Result == 0,
			"network port producer failed");
	}
	for ( size_t i = 0;
		i < (TEST_PORT_PRODUCERS * TEST_PORT_POSTS); i++ ) {
		testRequire(Seen[i], "threaded network port lost an event");
	}
	testRequire(xrtNetPortDestroy(pPort),
		"threaded network port destroy failed");
	return 0;
}
