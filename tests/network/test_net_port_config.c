#include "../test.h"



/* 端口核心必须稳定初始化配置并明确报告未编译后端。 */
int main(void)
{
	xnetportconfig Config;
	xnetport* pPort;

	memset(&Config, 0xA5, sizeof(Config));
	xrtNetPortConfigInit(&Config);
	testRequire((Config.Backend == XNET_PORT_AUTO) &&
		(Config.Flags == 0) && (Config.PostLimit == 4096) &&
		(Config.WatchLimit == 0) && (Config.OperationLimit == 0) &&
		(Config.OperationCache == 64),
		"port default config mismatch");

	Config.PostLimit = 0;
	testRequire(xrtNetPortCreate(&Config) == NULL,
		"zero port post limit was accepted");
	Config.PostLimit = 4096;
	Config.Backend = (xnetportbackend)99;
	testRequire(xrtNetPortCreate(&Config) == NULL,
		"unknown port backend was accepted");

	xrtNetPortConfigInit(&Config);
	pPort = xrtNetPortCreate(&Config);
	#if defined(XRT_FEATURE_NET_PORT_SELECT) || \
		defined(XRT_FEATURE_NET_PORT_EPOLL) || \
		defined(XRT_FEATURE_NET_PORT_KQUEUE) || \
		defined(XRT_FEATURE_NET_PORT_IOCP)
		xnetportconfig Effective;

		testRequire(pPort != NULL,
			"AUTO did not select the compiled fallback backend");
		testRequire(xrtNetPortGetConfig(pPort, &Effective) &&
			(Effective.Backend == xrtNetPortBackend(pPort)) &&
			(Effective.WatchLimit != 0) &&
			(Effective.OperationLimit != 0),
			"AUTO port capacities were not resolved");
		#if defined(_WIN32) || defined(_WIN64)
			#if defined(XRT_FEATURE_NET_PORT_IOCP)
				testRequire(
					(Effective.Backend != XNET_PORT_IOCP) ||
					(Effective.OperationLimit >= 65536u),
					"IOCP automatic operation capacity is too small"
				);
			#endif
		#endif
		testRequire(xrtNetPortBackend(NULL) == XNET_PORT_AUTO,
			"invalid port query unexpectedly succeeded");
		{
			xerror* pPrevious = xrtErrorRef(xrtGetError());

			testRequire((pPrevious != NULL) && xrtNetPortDestroy(pPort),
				"closing AUTO fallback port failed");
			testRequire(xrtGetError() == pPrevious,
				"successful port destroy discarded the previous error");
			xrtClearError();
			xrtErrorFree(pPrevious);
		}
	#else
		testRequire(pPort == NULL,
			"port core created an unavailable backend");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
			(xrtErrorCode(xrtGetError()) == XNET_ERROR_PORT_CREATE),
			"unavailable backend error mismatch");
	#endif
	return 0;
}
