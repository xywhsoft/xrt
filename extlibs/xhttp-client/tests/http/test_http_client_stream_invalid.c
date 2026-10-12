#include "../test.h"



/* 不应进入非法参数测试的完成过程。 */
static void testHttpClientStreamUnexpectedDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
	testRequire(false,
		"invalid HTTP call unexpectedly completed");
}



/* 验证调用驱动器的空参数、配置和状态错误。 */
int main(void)
{
	uint8 ConfigStorage[sizeof(xhttp1callconfig) + 2u];
	uint8 EventsStorage[sizeof(xhttp1callevents) + 2u];
	xhttp1callconfig Config;
	xhttp1callevents Events;

	xrtHttp1CallConfigInit(&Config);
	xrtHttp1CallEventsInit(&Events);
	testRequire(Config.WriteSize == 16384u,
		"HTTP call default write size mismatch");
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttp1CallConfigInit((xhttp1callconfig*)(void*)(
		ConfigStorage + 1u
	));
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire((Config.WriteSize == 16384u) &&
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5),
		"HTTP call config init did not support unaligned storage");
	memset(EventsStorage, 0xA5, sizeof(EventsStorage));
	xrtHttp1CallEventsInit((xhttp1callevents*)(void*)(
		EventsStorage + 1u
	));
	memcpy(&Events, EventsStorage + 1u, sizeof(Events));
	testRequire((Events.Done == NULL) &&
		(Events.Progress == NULL) && (Events.Data == NULL) &&
		(EventsStorage[0] == 0xA5) &&
		(EventsStorage[sizeof(EventsStorage) - 1u] == 0xA5),
		"HTTP call events init did not support unaligned storage");
	Events.Done = testHttpClientStreamUnexpectedDone;

	xrtClearError();
	xrtHttp1CallConfigInit(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call config error mismatch"
	);

	xrtClearError();
	xrtHttp1CallEventsInit(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call events error mismatch"
	);

	xrtClearError();
	xrtHttp1CallConfigInit((xhttp1callconfig*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP call config init accepted wrapping output");

	xrtClearError();
	xrtHttp1CallEventsInit((xhttp1callevents*)(uintptr_t)(
		UINTPTR_MAX - 1u
	));
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"HTTP call events init accepted wrapping output");

	xrtClearError();
	testRequire(!xrtHttp1CallTcp(
		NULL,
		NULL,
		NULL,
		&Events
	), "null HTTP TCP call unexpectedly succeeded");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP TCP call error mismatch"
	);

	xrtClearError();
	testRequire(!xrtHttp1CallCancel(NULL),
		"null HTTP call cancel unexpectedly succeeded");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call cancel error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtHttp1CallPause(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call pause error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtHttp1CallResume(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call resume error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtHttp1CallPaused(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call paused query error mismatch"
	);

	xrtClearError();
	testRequire(
		xrtHttp1CallState(NULL) == XHTTP1_CALL_FAILED,
		"null HTTP call state mismatch"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call state error mismatch"
	);

	xrtClearError();
	testRequire(
		(xrtHttp1CallError(NULL) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null HTTP call error query mismatch"
	);
	printf("[PASS] HTTP/1 call invalid inputs\n");
	return 0;
}
