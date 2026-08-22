#define XRT_IMPLEMENTATION
#define XRT_MODULE_HTTP_SSE_CLIENT
#include "../../single/xrt.h"



/* 验证单头文件发布 EventSource 客户端默认策略和参数错误契约。 */
int main(void)
{
	xhttpsseclientconfig Config;

	xrtHttpSseClientConfigInit(&Config);
	if ( (Config.Http.Timeout != XHTTP_CLIENT_TIMEOUT_NONE) ||
		(Config.Http.IdleTimeout != XHTTP_CLIENT_TIMEOUT_NONE) ||
		(Config.Http.ResponseBodyLimit != UINT64_MAX) ||
		(Config.Http.Redirect != XHTTP_REDIRECT_FOLLOW) ||
		(Config.MaxReconnects != XHTTP_SSE_RECONNECT_MAX_DEFAULT) ||
		(Config.RetryMin != XHTTP_SSE_RETRY_MIN_DEFAULT) ||
		(Config.RetryMax != XHTTP_SSE_RETRY_MAX_DEFAULT) ||
		(Config.Parser.Retry != XHTTP_SSE_RETRY_DEFAULT) ) {
		return 1;
	}

	xrtClearError();
	if ( (xrtHttpSseConnect(NULL, XRT_STR_LITERAL("http://localhost/"),
		NULL, NULL) != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	return 0;
}
